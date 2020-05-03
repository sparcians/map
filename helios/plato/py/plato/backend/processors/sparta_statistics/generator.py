import array
from logging import error
import logging
import time
from typing import Union, Tuple

from numpy.linalg.linalg import LinAlgError

import numpy as np
from plato.backend.adapters.sparta_statistics.adapter import SpartaStatisticsAdapter
from plato.backend.common import logtime


logger = logging.getLogger("plato.backend.sparta")


# Can generate a time-series data for a selected range of time
class SpartaStatsGenerator:

    def __init__(self, adapter: Union[SpartaStatisticsAdapter]):
        self._adapter = adapter

    @property
    def adapter(self):
        return self._adapter

    @logtime(logger)
    def generate_lines(self, first, last, units, stat_cols, max_points_per_series):
        start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('first/last not in range of data')

        points = self._adapter.get_points(first, last, units, stat_cols, max_points_per_series)

        print(f'SpartaStatsGenerator::generate_lines() over {last - first} cycles for {len(stat_cols)} stats took {(time.time() - start) * 1000} ms')

        return points

    @logtime(logger)
    def generate_histogram(self, first, last, unit, stat_col: str, max_points_per_series):

        # TODO stat_col could be data, skip the lookup if it is
        _, x = self._adapter.get_points(first, last, unit, [stat_col], 0)

        hhist, hedges = np.histogram(x, bins = 55)

        return (array.array('f', hedges),
                array.array('f', hhist))

    @logtime(logger)
    def generate_regression_line(self, first, last, units, stat_cols: Tuple[str, str], max_points_per_series = None, whichChanged = None, isAllData = None) -> Tuple[np.ndarray, np.ndarray]:
        '''
        create a regression line for two columns
        '''
        # compute the trend line
        try:
            _, x, y = self._adapter.get_points(first, last, units, stat_cols, 0)

            par = np.polyfit(x, y, 1, full = True)
            slope = par[0][0]
            intercept = par[0][1]

            xValues = [min(x), max(x)]
            yValues = [slope * i + intercept for i in xValues]

            return (array.array('f', xValues),
                    array.array('f', yValues))
        except (LinAlgError, TypeError) as le:
            error('regline error:' + str(le))
            return (array.array('f', [1E-5, 1E-4]),
                    array.array('f', [1E-5, 1E-4]))
