from logging import info, debug
import sqlite3
import time

import numpy as np
import pandas as pd
from plato.backend.units import Units


class SpartaDataSource:
    '''
    this will read the sqlite database produced by a SPARTA1.7+ simulation run
    '''

    reportHeaderQuery = """SELECT TimeseriesID, StartTime, EndTime, ReportName, SILocations from ReportHeader LIMIT 0,1"""

    statInstValuesQuery = """SELECT TimeseriesChunkID, RawBytes, NumPts, Id from StatInstValues ORDER BY Id"""

    statQuery = """SELECT COUNT(*) from StatInstValues"""

    cyclesStat = 'clocks.Root.cycles'

    minMaxCycleQuery = """SELECT StartCycle FROM 'TimeseriesChunk' WHERE Id = (SELECT min(Id) FROM TimeSeriesChunk) OR Id = (SELECT max(Id) FROM TimeSeriesChunk) ORDER BY StartCycle ASC"""

    cycleQuery = """SELECT StartCycle, Id FROM 'TimeseriesChunk' ORDER BY Id ASC"""

    def __init__(self, filename):
        start = int(round(time.time() * 1000))
        self._pdf = None

        with sqlite3.connect(filename) as conn:
            cursor = conn.cursor()

            for currentRow in cursor.execute(SpartaDataSource.reportHeaderQuery):
                nodeList = []
                SILocations = currentRow[4].split(',')

                for statName in SILocations:
                    if statName.startswith("top") and not statName.endswith("probability") and 'count' not in statName[-15:] and 'Testing' not in statName[-15:]:
                        nodeList.append(statName)

                statsList = [np.frombuffer(statRow[1], dtype = 'd') for statRow in conn.cursor().execute(SpartaDataSource.statInstValuesQuery)]

                # build the dataframe and cut out columns that aren't useful
                self._pdf = pd.DataFrame(statsList,
                                         columns = SILocations).filter(items = nodeList)

                # get the starting cycle for each epoch
                startCycleList = []
                for statRow in conn.cursor().execute(SpartaDataSource.cycleQuery):
                    startCycleList.append(statRow[0])

                self._pdf[SpartaDataSource.cyclesStat] = startCycleList

                # TODO supports only one time series for now
                break

        info("spent {}ms building dataframe".format(time.time() - start))

    @staticmethod
    def get_source_information(filename: str):
        '''
        go grab basic stats from this sqlite file
        '''
        with sqlite3.connect(filename) as conn:
            cursor = conn.cursor()
            for currentRow in cursor.execute(SpartaDataSource.reportHeaderQuery):
                locations = currentRow[4].split(',')

                nodeList = []
                for statName in locations:
                    if statName.startswith("top") and not statName.endswith("probability") and 'count' not in statName[-15:] and 'Testing' not in statName[-15:]:
                        nodeList.append(statName)

            maxValue = None
            minValue = None
            startCycleArray = []
            for statRow in conn.cursor().execute(SpartaDataSource.cycleQuery):
                maxValue = max(maxValue, statRow[0]) if maxValue else statRow[0]
                minValue = min(minValue, statRow[0]) if minValue else statRow[0]
                startCycleArray.append(int(statRow[0]))
                debug((maxValue, minValue))

        return {'timeRange': [{'units': Units.CYCLES,
                               'first': minValue,
                               'last': maxValue}],
                'typeSpecific': {'statCount': len(nodeList),
                                 'startCycle': startCycleArray},
                'stats': nodeList
                }

    @property
    def stats(self):
        return self._pdf.columns

    # Overriding GeneralTraceDataSource to give GeneralTraceAdapter adapter access to raw data
    def pdf(self) -> pd.DataFrame:
        return self._pdf
