from _collections import OrderedDict
from pathlib import Path
import sys
import time

import h5py
import hdf5plugin
# Is this package used in here?  There is no mainstream conda package for it: import tables

import pandas as pd
from plato.backend.units import Units


class PeventDataSource:
    '''
    this will read a pevent trace from a data store
    '''

    peventDecodePath = '''peventdb/SOURCE/'''

    cyclesStat = 'CYC'

    def __init__(self, filename):

        start = time.time()

        with h5py.File(filename) as h5File:

            tableNames = h5File[PeventDataSource.peventDecodePath].keys()
            knownTables = []

            for tableName in tableNames:
                try:
                    currentTable = h5File[f"{PeventDataSource.peventDecodePath}/{str(tableName)}"]
                    knownTables.append(pd.Series(data = currentTable[PeventDataSource.cyclesStat]))
                except ValueError as e:
                    print(e)
                    print(f"error loading table {str(tableName)}, will figure out later")

            self._pdf = pd.concat(knownTables, axis = 1, ignore_index = True)
            self._pdf.columns = [str(tableName) for tableName in tableNames]

        print(f"loaded in {(time.time() - start) * 1000}ms")

    @staticmethod
    def can_read(filename):
        try:
            with h5py.File(filename) as h5File:
                h5Keys = [str(x) for x in h5File[PeventDataSource.peventDecodePath].keys()]

                if h5Keys:
                    for tableName in h5Keys:
                        currentTable = h5File[f"{PeventDataSource.peventDecodePath}/{str(tableName)}"]
                        tableKeys = [a for a in currentTable.dtype.fields.keys()]

                        if PeventDataSource.cyclesStat not in tableKeys:
                            print(f"every table (including {tableName}) must have a {PeventDataSource.cyclesStat} column: {', '.join(tableKeys)}")
                            return False

                    return True
                else:
                    print(f"wrong schema, no tables in {PeventDataSource.peventDecodePath}")
                    return False
        except Exception as e:
            print(e)
            return False

    def pdf(self):
        return self._pdf

    @staticmethod
    def get_source_information(filename):

        start = time.time()
        with h5py.File(filename) as h5File:
            maxVal = -sys.maxsize
            minVal = sys.maxsize

            h5Keys = h5File[PeventDataSource.peventDecodePath].keys()

            stats = []

            typeSpecific = {'shape': {}}

            for tableName in h5Keys:

                stats.append(tableName)

                # just test the first couple to see the range
                # only using CYC as an index for now
                currentTable = h5File[f"{PeventDataSource.peventDecodePath}/{str(tableName)}"]
                typeSpecific['shape'][str(tableName)] = {'table': currentTable.shape}
                firstRow = currentTable[0]
                minVal = min(firstRow[PeventDataSource.cyclesStat], minVal)
                lastRow = currentTable[-1]
                maxVal = max(lastRow[PeventDataSource.cyclesStat], maxVal)

            print(f"min is {minVal}, max is {maxVal}")

            timeRanges = [{'units': Units.CYCLES,
                           'first': int(minVal),
                           'last': int(maxVal)},
                           # TODO more
                           ]

        print(f"get_source_information in {(time.time() - start) * 1000}ms")

        return {
            'timeRange': timeRanges,
            'typeSpecific': typeSpecific,
            'stats': stats
            }
