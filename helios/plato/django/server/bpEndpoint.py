import asyncio
from collections import OrderedDict
import concurrent
from contextlib import contextmanager
from functools import wraps
import functools
import itertools
import json
import logging
import math
from os import access
import os
from os.path import realpath, basename, dirname
from pathlib import Path
from threading import Lock
import threading
import time
import traceback
from uuid import uuid5
import uuid

from asgiref.sync import async_to_sync
from channels.generic.websocket import AsyncWebsocketConsumer
from django.core.cache import caches

from server import settings
from server.beakercache import cache
import numpy as np
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.adapters.pevent_trace.adapter import PeventTraceAdapter
from plato.backend.adapters.sparta_statistics.adapter import SpartaStatisticsAdapter
from plato.backend.common import logtime, synchronized, synchronizedFine, countactive
from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.datasources.pevent_trace.datasource import PeventDataSource
from plato.backend.datasources.sparta_statistics.datasource import SpartaDataSource
from plato.backend.processors.branch_training_heatmap.adapter import BranchTrainingHeatMapAdapter
from plato.backend.processors.branch_training_heatmap.generator import BranchTrainingHeatMapGenerator
from plato.backend.processors.branch_training_line_plot.generator import BranchTrainingLinePlotGenerator
from plato.backend.processors.branch_training_profile.generator import BranchTrainingProfileHtmlGenerator
from plato.backend.processors.branch_training_table.generator import BranchTrainingListHtmlGenerator
from plato.backend.processors.general_line_plot.generator import GeneralTraceLinePlotGenerator
from plato.backend.processors.pevent_trace_generator.generator import PeventTraceGenerator
from plato.backend.processors.sparta_statistics.generator import SpartaStatsGenerator
from plato.backend.units import Units
from .models import DataId, ProcessorId, LongFunctionCall

logger = logging.getLogger("plato.backend.bpEndpoint")


class bpEndpoint(AsyncWebsocketConsumer):
    '''
    this is the endpoint for most of the data operations that happen in plato
    handles branch prediction, SPARTA data sources, etc.
    '''

    # this is the cache that will speed up complex lookups and hold references
    # to useful data structures that are used for doing data processing
    beakerCache = cache.get_cache('branchPredictor', expire = 3600)

    def __init__(self, *args, **kwargs):
        '''
        ctor
        '''
        super().__init__(*args, **kwargs)
        self.callback = None
        self.sendLock = Lock()
        self.loop = asyncio.get_event_loop()
        # the thread pool, whenever a request is received via an async function
        # call, it is immediately launched onto a concurrent thread to avoid
        # keeping the async thread busy, when the processing is done then it
        # will come back to the async thread to send the result back to the client
        self.threadPoolExecutor = concurrent.futures.ThreadPoolExecutor(max_workers = settings.WORKER_THREADS,
                                                                        thread_name_prefix = "wsEndpointWorker")

    async def connect(self):
        '''
        look at the call that this wants to use and cache that
        '''
        path = self.scope.get('path', '')

        if path == '/ws/getData':
            self.callback = self.getData
            await self.accept()
        elif path == '/ws/sources':
            self.callback = self.sources
            await self.accept()
        else:
            logger.error(f"error: not recognized: {path}")
            await self.close()

    async def disconnect(self, close_code):
        '''
        when the client disconnects from the websocket
        '''
        await self.close()

    async def receive(self, text_data = None, bytes_data = None):
        '''
        capture and route the actual request
        '''
        # go run this on a separate thread
        # TODO ensure that only text_data is received, clients should be sending
        # json as text, not binary
        blockingTask = asyncio.get_event_loop().run_in_executor(self.threadPoolExecutor,
                                                                self.callback,
                                                                text_data)

    def sendMessage(self, text_data = None, bytes_data = None, close = False):
        with self.sendLock:
            loop = self.loop

            if text_data:
                future = asyncio.run_coroutine_threadsafe(self.send(json.dumps(text_data), None, close), loop)
                future.result()
            if bytes_data is not None:
                future = asyncio.run_coroutine_threadsafe(self.send(None, bytes_data if len(bytes_data) else b'0', close), loop)
                future.result()

    @logtime(logger)
    @cache.cache('getSourceInformation', expire = 360)
    def getSourceInformation(self, path):
        '''
        go look at the file type and get info about it, determine what can read
        it and then go cache the reader for it
        '''
        isHdf5File = path.endswith("hdf5")

        if isHdf5File:
            if BranchTrainingDatasource.can_read(path):
                return BranchTrainingDatasource.get_source_information(path), "branch-predictor-training-trace"
            elif PeventDataSource.can_read(path):
                return PeventDataSource.get_source_information(path), "pevent-trace"
            else:
                raise ValueError("unknown hdf5 type, pevent and branch training data sources cannot read this")
        else:
            return SpartaDataSource.get_source_information(path), "sparta-statistics"

    @logtime(logger)
    def loadFile(self, jsonData, returnValue):
        '''
        load a single file and return data
        '''
        returnValue['waitEstimate'] = 12345
        file = jsonData['file']

        if not access(file, os.R_OK):
            raise ValueError("cannot read file")

        currentFile = realpath(file)
        # use the django ORM to lookup or create a persistent UUID for this file
        dataIdObj, created = DataId.objects.get_or_create(path = file,
                                                          defaults = {'uuid': str(uuid5(uuid.NAMESPACE_URL,
                                                                                        file))})

        returnValue["result"] = "complete"
        logger.debug(f"{currentFile} created? {created}: {dataIdObj}")

        statVals, typeId = self.getSourceInformation(file)

        newDict = {"name": str(basename(file)),
                   "directory": dirname(file),
                   "typeId": typeId,
                   "dataId": dataIdObj.uuid}

        for key, value in statVals.items():
            newDict[key] = value

        returnValue["sources"] = [newDict]

    @logtime(logger)
    def getProcessor(self, jsonData, returnValue):
        '''
        get the processor ID for a given processor type + data ID
        this may take a long time so it should be run in a separate thread,
        rather than the main event loop
        '''
        dataId = jsonData['dataId']
        processor = jsonData['processor']
        kwargs = json.dumps(OrderedDict(sorted(jsonData['kwargs'].items(), key = lambda kv: kv[0])))

        # TODO need a better UUID?
        processorUuid = str(uuid.uuid5(uuid.NAMESPACE_DNS, name = str(time.time())))

        processorIdObj, created = ProcessorId.objects.get_or_create(dataUuid = dataId,
                                                                    processor = processor,
                                                                    kwargs = kwargs,
                                                                    defaults = {'uuid': processorUuid, })
        logger.debug(f"{dataId} created? {created}: {processorIdObj}")
        returnValue['processorId'] = processorIdObj.uuid

        if processor == "shp-heatmap-generator":
            bpEndpoint.getShpHeatmapGenerator(processorIdObj.uuid)
        elif processor == "shp-line-plot-generator":
            bpEndpoint.getShpLinePlotGenerator(processorIdObj.uuid)
        elif processor == "shp-branch-list-generator":
            bpEndpoint.getShpTableHtmlGenerator(processorIdObj.uuid)
        elif processor == "simdb-line-plot-generator":
            bpEndpoint.getSimDbLinePlotGenerator(processorIdObj.uuid)
        elif processor == "simdb-get-all-data-generator":
            bpEndpoint.getSimDbStatsGenerator(processorIdObj.uuid)
        elif processor == "shp-branch-profile-generator":
            bpEndpoint.getShpBranchProfileHtmlGenerator(processorIdObj.uuid)
        elif processor == "pevent-trace-generator":
            bpEndpoint.getPeventTraceGenerator(processorIdObj.uuid)
        else:
            raise ValueError(f"unknown processor type {processor}")

        returnValue['result'] = 'complete'

    @logtime(logger)
    def loadDirectory(self, jsonData, returnValue):
        '''
        go scan a directory and return metadata + generated UUIDs
        '''
        returnValue['waitEstimate'] = 12345
        directory = Path(jsonData['directory']).resolve()
        returnValue['directory'] = str(directory)
        returnValue["result"] = "in-progress"
        hdf5GlobPattern = jsonData.get('globPattern', "*hdf5")
        dbGlobPattern = jsonData.get('globPattern', "*db")

        self.sendMessage(returnValue)

        # detect errors
        if not directory.is_dir():
            raise ValueError(f"cannot read directory: {str(directory)}")
        if not access(directory, os.R_OK):
            raise ValueError("cannot access directory")

        returnValue['subDirectories'] = [str(x) for x in Path(directory).iterdir() if x.is_dir()]

        dbFiles = list(filter(os.path.isfile, directory.glob(dbGlobPattern)))

        hdf5Files = list(filter(os.path.isfile, directory.glob(hdf5GlobPattern)))

        returnValue['result'] = 'partial'
        returnValue['sources'] = []

        for i, currentFile in enumerate(hdf5Files + dbFiles):
            # need the canonical path of the file to generate a UUID
            relativePath = currentFile.relative_to(directory)
            currentFile = realpath(currentFile)

            isHdf5File = currentFile.endswith("hdf5")

            if isHdf5File:
                # two options, branch training or p-events
                if BranchTrainingDatasource.can_read(currentFile):
                    typeId = "branch-predictor-training-trace"
                elif PeventDataSource.can_read(currentFile):
                    typeId = "pevent-trace"
                else:
                    raise ValueError("unknown hdf5 type, pevent and branch training data sources cannot read this")
            else:
                typeId = "sparta-statistics"

            dataIdObj, created = DataId.objects.get_or_create(path = currentFile,
                                                              defaults = {'uuid': str(uuid5(uuid.NAMESPACE_URL,
                                                                                            currentFile))})

            logger.debug(f"{currentFile} created? {created}: {dataIdObj}")
            newDict = {"name": str(relativePath),
                       "typeId": typeId,
                       "dataId": dataIdObj.uuid}

            returnValue['sources'].append(newDict)

            if i + 1 != len(hdf5Files + dbFiles):
                self.sendMessage(returnValue)

        returnValue['result'] = 'complete'

    @logtime(logger)
    def sources(self, value):
        '''
        load sources and return progress
        this may take a while and should be run in its own thread when possible
        '''
        returnValue = {}

        try:
            jsonData = json.loads(value)
            command = returnValue["command"] = jsonData['command']
            returnValue["result"] = "error"
            returnValue["reqSeqNum"] = jsonData["reqSeqNum"]

            if command == 'getProcessor':
                # cache a processor
                self.getProcessor(jsonData, returnValue)

            elif command == 'loadFile':
                # populate with just a single file
                self.loadFile(jsonData, returnValue)

            elif command == 'loadDirectory':
                # then populate the cache with all files in this directory
                self.loadDirectory(jsonData, returnValue)
            else:
                raise ValueError(f"unknown command {command}")

            self.sendMessage(returnValue)

        except Exception as e:
            logger.exception("problem in sources()")
            returnValue["result"] = "error"
            returnValue["stackTrace"] = traceback.format_exc()
            returnValue["errorMessage"] = "error in sources(): {}".format(e)

            self.sendMessage(returnValue)

    @countactive
    @logtime(logger)
    def getData(self, value):
        '''
        get a branch predictor heatmap or line-plot in byte format
        '''
        logger.debug(f"getData() with request ${json.loads(value)}")
        start = time.time()

        responseValue = {"reqSeqNum": -1,
                         "result": "error",
                         "duration": 0,
                         "processorSpecific": {}}

        try:
            jsonData = json.loads(value)
            first = jsonData['first']
            last = jsonData['last']
            processorId = jsonData['processorId']
            kwargs = jsonData['kwargs']
            responseValue["reqSeqNum"] = jsonData['reqSeqNum']

            processor = self.lookupProcessorId(processorId)[0]

            if processor.processor == "shp-heatmap-generator":
                bphmg = bpEndpoint.getShpHeatmapGenerator(processor.uuid)

                logger.debug(f"calling generate_2d_heatmap_with_profiles() with {[first,last]}, {kwargs}")

                heatMap, tableMeans, rowMeans, tableStds, tableMins, tableMaxs, tableMedians, = \
                    bphmg.generate_2d_heatmap_with_profiles_and_stats(first, last, **kwargs)

                responseValue["processorSpecific"]["numRows"] = heatMap.shape[0]
                responseValue["processorSpecific"]["numCols"] = heatMap.shape[1]
                responseValue["processorSpecific"]["zMin"] = int(round(np.nanmin(heatMap)))
                responseValue["processorSpecific"]["zMax"] = int(round(np.nanmax(heatMap)))
                responseValue["processorSpecific"]["zBlobOffset"] = 0
                resultBin = heatMap.tobytes()

                responseValue["processorSpecific"]["tableMeansBlobOffset"] = len(resultBin)
                resultBin += tableMeans.tobytes()

                responseValue["processorSpecific"]["rowMeansBlobOffset"] = len(resultBin)
                resultBin += rowMeans.tobytes()

                responseValue["processorSpecific"]["tableStdsBlobOffset"] = len(resultBin)
                resultBin += tableStds.tobytes()

                responseValue["processorSpecific"]["tableMinsBlobOffset"] = len(resultBin)
                resultBin += tableMins.tobytes()

                responseValue["processorSpecific"]["tableMaxsBlobOffset"] = len(resultBin)
                resultBin += tableMaxs.tobytes()

                responseValue["processorSpecific"]["tableMediansBlobOffset"] = len(resultBin)
                resultBin += tableMedians.tobytes()

            elif processor.processor == "shp-line-plot-generator":
                logger.debug(f"calling generate_lines() with {[first,last]}, {kwargs}")
                resultBin = self.getShpLinePlot(jsonData, responseValue)

            elif processor.processor == "shp-branch-list-generator":
                bptg = bpEndpoint.getShpTableHtmlGenerator(processor.uuid)
                logger.debug(f"calling generate_table() with {[first,last]}, {kwargs}")
                result = bptg.generate_table(first, last, **kwargs)

                responseValue["processorSpecific"] = {"stringLength": len(result),
                                                      "stringBlobOffset": 0}
                resultBin = result.encode('utf-8')

            elif processor.processor == "shp-branch-profile-generator":
                sbpg = bpEndpoint.getShpBranchProfileHtmlGenerator(processor.uuid)
                logger.debug(f"calling generate_table() with {[first,last]}, {kwargs}")

                result = sbpg.generate_table(first, last, **kwargs)

                responseValue["processorSpecific"] = {"stringLength": len(result),
                                                      "stringBlobOffset": 0}
                resultBin = result.encode('utf-8')

            elif processor.processor == "simdb-line-plot-generator":
                sdblp = bpEndpoint.getSimDbLinePlotGenerator(processor.uuid)

                result = sdblp.generate_lines(first, last, **kwargs)

                if result.shape[1] == 0:
                    yExtents = [-1, 1]
                else:
                    yExtents = [np.nanmin(result[1:]), np.nanmax(result[1:])]  # Skip the first row (units)

                responseValue["processorSpecific"]["numSeries"] = result.shape[0]
                responseValue["processorSpecific"]["pointsPerSeries"] = result.shape[1]
                responseValue["processorSpecific"]["seriesBlobOffset"] = 0
                responseValue["processorSpecific"]["yExtents"] = yExtents
                resultBin = result.astype(dtype = 'f4', copy = False).tobytes()

            elif processor.processor == "simdb-get-all-data-generator":
                resultBin = self.simdbGetAllData(processor.uuid, jsonData, responseValue)

            elif processor.processor == "pevent-trace-generator":
                peventTraceGenerator = bpEndpoint.getPeventTraceGenerator(processor.uuid)

                histograms = peventTraceGenerator.generate_histograms(first, last, **kwargs)

                psData = responseValue["processorSpecific"] = {}
                psData["numSeries"] = len(histograms.keys())
                psData["series"] = list(histograms.keys())
                psData["histLengths"] = [len(v[0]) for v in histograms.values()]
                psData["edgesLengths"] = [len(v[1]) for v in histograms.values()]

                resultBin = next(itertools.islice(histograms.values(), 0, 1))[1].astype(dtype = 'f4').tobytes()
                for v in histograms.values():
                    resultBin += v[0].astype(dtype = 'f4').tobytes()

            else:
                raise ValueError(f"{processorId} isn't in the database, can't do a lookup")

            responseValue["result"] = "complete"
            responseValue["durationMs"] = math.ceil((time.time() - start) * 1000)

            # send the metadata back in json format
            self.sendMessage(responseValue, resultBin)

        except Exception as e:
            # something went wrong, need to let the client know
            logger.exception(f"problem in getData(), {value}")
            responseValue["stackTrace"] = traceback.format_exc()
            responseValue["errorMessage"] = str(e)
            responseValue["durationMs"] = math.ceil((time.time() - start) * 1000)

            self.sendMessage(responseValue)

    def getShpLinePlot(self, jsonData, responseValue):
        '''
        find the shp generator then tell it to generate a line plot
        '''
        processorId = jsonData['processorId']

        if True:
            processor = self.lookupProcessorId(processorId)[0]
        else:
            # TODO this will saturate the db connections quickly, need to either
            # find the connection leak or add db connection pooling
            processor = ProcessorId.objects.get(uuid = processorId)
        bplpg = bpEndpoint.getShpLinePlotGenerator(processor.uuid)

        first = jsonData['first']
        last = jsonData['last']
        kwargs = jsonData['kwargs']
        result, modalities, downsamplingLevel, yExtents = bplpg.generate_lines_and_more(first, last, **kwargs)

        responseValue["processorSpecific"]["numSeries"] = result.shape[0]
        responseValue["processorSpecific"]["pointsPerSeries"] = result.shape[1]
        responseValue["processorSpecific"]["downsampling"] = downsamplingLevel
        responseValue["processorSpecific"]["seriesBlobOffset"] = 0
        responseValue["processorSpecific"]["yExtents"] = yExtents
        blob = result.astype(dtype = 'f4', copy = False).tobytes()

        responseValue["processorSpecific"]["modalitiesOffset"] = len(blob)
        blob += modalities.astype(dtype = 'i2', copy = False).tobytes()

        if downsamplingLevel <= 1:
            # Get some data that only makes sense at no-downsampling. User can view individual points' data
            otherKwargs = {"units": kwargs["units"],
                           "stat_cols": ['pc', 'tgt', 'trn_idx', 'instructions'],
                           "branch_predictor_filtering": kwargs.get("branch_predictor_filtering", {}),
                           "point_cache": kwargs.get("point_cache", True)}
            points = bplpg.generate_simple_int_points(first, last, **otherKwargs)
            pcs = points[0]
            tgts = points[1]
            indices = points[2]
            instructions = points[3]

            # Since these arrays are 8B, they must be aligned to 8B for javascript arrays to be happy.
            misalignment = len(blob) % 8
            if misalignment > 0:
                blob += bytes([0] * (8 - misalignment))
            responseValue["processorSpecific"]["indicesOffset"] = len(blob)
            blob += indices.tobytes()

            misalignment = len(blob) % 8
            if misalignment > 0:
                blob += bytes([0] * (8 - misalignment))
            responseValue["processorSpecific"]["addressesOffset"] = len(blob)
            blob += pcs.tobytes()

            misalignment = len(blob) % 8
            if misalignment > 0:
                blob += bytes([0] * (8 - misalignment))
            responseValue["processorSpecific"]["targetsOffset"] = len(blob)
            blob += tgts.tobytes()

            misalignment = len(blob) % 8
            if misalignment > 0:
                blob += bytes([0] * (8 - misalignment))
            responseValue["processorSpecific"]["instructionNumsOffset"] = len(blob)
            blob += instructions.tobytes()

        return blob

    def simdbGetAllData(self, processorUuid, jsonData, responseValue):

        first = jsonData['first']
        last = jsonData['last']

        statsGen = bpEndpoint.getSimDbStatsGenerator(processorUuid)
        kwargs = jsonData['kwargs']
        whichChanged = kwargs["whichChanged"]

        psData = responseValue["processorSpecific"]

        if whichChanged:
            del kwargs["whichChanged"]
            # so x or y is updated
            a = self.threadPoolExecutor.submit(functools.partial(statsGen.generate_lines,
                                                                 first,
                                                                 last,
                                                                 **kwargs))
            blockingTasks = [a]

        else:
            a = None
            blockingTasks = []
            psData["pointsPerSeries"] = 0

        psData["stat_cols"] = kwargs['stat_cols']

        # TODO this is redundant, should either be able to pass this to other calls and save them
        # the trouble or get it as a byproduct of others calls calculating this
        b = self.threadPoolExecutor.submit(functools.partial(statsGen.adapter.range_to_rows,
                                                             first,
                                                             last,
                                                             kwargs["units"]))
        c = self.threadPoolExecutor.submit(functools.partial(statsGen.generate_histogram,
                                                             first,
                                                             last,
                                                             kwargs["units"],
                                                             kwargs["stat_cols"][0],
                                                             -1))
        d = self.threadPoolExecutor.submit(functools.partial(statsGen.generate_histogram,
                                                             first,
                                                             last,
                                                             kwargs["units"],
                                                             kwargs["stat_cols"][1],
                                                             -1))
        e = self.threadPoolExecutor.submit(functools.partial(statsGen.generate_regression_line,
                                                             first,
                                                             last,
                                                             **kwargs))
        blockingTasks.extend([b, c, d, e])
        results = []
        for futureTask in blockingTasks:
            result = futureTask.result()
            results.append((futureTask, result))

        x = y = indices = None

        for futureTask, result in results:
            if futureTask == a:
                indices, x, y = result
                psData["xMin"] = min(x)
                psData["xMax"] = max(x)
                psData["yMin"] = min(y)
                psData["yMax"] = max(y)
                psData["pointsPerSeries"] = len(x)

            elif futureTask == b:
                firstRow, lastRow, _ = result
                psData["firstRow"] = firstRow
                psData["lastRow"] = lastRow
                psData["first"] = first
                psData["last"] = last

            elif futureTask == c:
                hEdges, hHist = result
                psData["hHistLength"] = len(hHist)
                psData["hEdgesLength"] = len(hEdges)

            elif futureTask == d:
                vEdges, vHist = result
                psData["vEdgesLength"] = len(vEdges)
                psData["vHistLength"] = len(vHist)

            elif futureTask == e:
                regX, regY = result
                psData["regLength"] = len(regX)

        if x is not None:
            return indices.astype(dtype = 'f4').tobytes() + \
                x.astype(dtype = 'f4').tobytes() + y.astype(dtype = 'f4').tobytes() + \
                hEdges.tobytes() + hHist.tobytes() + \
                vEdges.tobytes() + vHist.tobytes() + \
                regX.tobytes() + regY.tobytes()
        else:
            return hEdges.tobytes() + hHist.tobytes() + \
                vEdges.tobytes() + vHist.tobytes() + \
                regX.tobytes() + regY.tobytes()

    @staticmethod
    @logtime(logger)
    @cache.cache('lookupProcessorId', expire = 3600)
    def lookupProcessorId(processorId):
        logger.debug(f"lookup in ProcessorId for {processorId}")
        procId = ProcessorId.objects.get(uuid = processorId)
        dataUuid = procId.dataUuid
        logger.debug(f"returned {dataUuid}")
        path = DataId.objects.filter(uuid = dataUuid)[0].path
        logger.debug(f"returned {path}")

        return procId, path

    endpointLock = Lock()
    endpointLockDict = {}

    @staticmethod
    # TODO need to make a mutex per path, or globally
    @synchronized(endpointLock)
    # @synchronizedFine(endpointLockDict, endpointLock)
    @cache.cache('dataSource', expire = 3600)
    @logtime(logger)
    def getDataSource(path):
        logger.debug(f"open data source: {path}")
        if path.endswith("hdf5"):
            if BranchTrainingDatasource.can_read(path):
                return BranchTrainingDatasource(path)
            elif PeventDataSource.can_read(path):
                return PeventDataSource(path)
            else:
                raise ValueError("unknown type of file, can't choose data source")
        else:
            return SpartaDataSource(path)

    @staticmethod
    @cache.cache('shpHtmlTable', expire = 3600)
    @logtime(logger)
    def getShpTableHtmlGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        branch_hm_data = bpEndpoint.getDataSource(path)

        bptta = BranchTrainingTraceAdapter(branch_hm_data)

        bptg = BranchTrainingListHtmlGenerator(bptta, **json.loads(procIdObj.kwargs))

        return bptg

    @staticmethod
    @cache.cache('shpHtmlTable', expire = 3600)
    @logtime(logger)
    def getShpBranchProfileHtmlGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        branch_hm_data = bpEndpoint.getDataSource(path)

        bptta = BranchTrainingTraceAdapter(branch_hm_data)

        bptg = BranchTrainingProfileHtmlGenerator(bptta, **json.loads(procIdObj.kwargs))

        return bptg

    @staticmethod
    @cache.cache('shpLinePlot', expire = 3600)
    @logtime(logger)
    def getShpLinePlotGenerator(processorId):

        logger.info(f"getShpLinePlotGenerator for pid={processorId}")
        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        # load source data
        branchHeatMapDataSource = bpEndpoint.getDataSource(path)
        logger.debug(f'List of stats are: {branchHeatMapDataSource.stats}')

        # constructor adapter
        branchTrainingTraceAdapter = BranchTrainingTraceAdapter(branchHeatMapDataSource)

        # construct generator
        linePlotGenerator = BranchTrainingLinePlotGenerator(branchTrainingTraceAdapter, **json.loads(procIdObj.kwargs))

        return linePlotGenerator

    @staticmethod
    @cache.cache('simDbLinePlot', expire = 3600)
    @logtime(logger)
    def getSimDbLinePlotGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        # load source data
        simDbDataSource = bpEndpoint.getDataSource(path)
        logger.debug(f'List of stats are: {simDbDataSource.stats}')

        # constructor adapter
        spartaStatsAdapter = SpartaStatisticsAdapter(simDbDataSource)

        # construct generator
        linePlotGenerator = GeneralTraceLinePlotGenerator(spartaStatsAdapter, **json.loads(procIdObj.kwargs))

        return linePlotGenerator

    @staticmethod
    @cache.cache('peventTraceGenerator', expire = 3600)
    @logtime(logger)
    def getPeventTraceGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        # load source data
        peventDataSource = bpEndpoint.getDataSource(path)

        # construct adapter
        peventTraceAdapter = PeventTraceAdapter(peventDataSource)

        # construct generator
        peventTraceGenerator = PeventTraceGenerator(peventTraceAdapter, **json.loads(procIdObj.kwargs))

        return peventTraceGenerator

    @staticmethod
    @cache.cache('simDbStatsAdapter', expire = 3600)
    @logtime(logger)
    def getSimDbStatsGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        # load source data
        simDbDataSource = bpEndpoint.getDataSource(path)
        logger.debug(f'List of stats are: {simDbDataSource.stats}')

        # constructor adapter
        spartaStatsAdapter = SpartaStatisticsAdapter(simDbDataSource)

        # construct generator
        generator = SpartaStatsGenerator(spartaStatsAdapter, **json.loads(procIdObj.kwargs))

        return generator

    @staticmethod
    @logtime(logger)
    @cache.cache('shpHeatMap', expire = 3600)
    def getShpHeatmapGenerator(processorId):

        procIdObj, path = bpEndpoint.lookupProcessorId(processorId)

        # load source data
        branchHeatMapDataSource = bpEndpoint.getDataSource(path)

        # constructor adapter
        branchTrainingHeatMapAdapter = BranchTrainingHeatMapAdapter(branchHeatMapDataSource)

        heatMapGenerator = BranchTrainingHeatMapGenerator(branchTrainingHeatMapAdapter, **json.loads(procIdObj.kwargs))

        return heatMapGenerator

