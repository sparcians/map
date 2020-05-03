from _collections import OrderedDict
from logging import info
import time
from typing import Union

import numpy as np
from plato.backend.adapters.general_trace.adapter import GeneralTraceAdapter


# Can generate a time-series data for a selected range of time
class PeventTraceGenerator:
    def __init__(self, adapter: Union[GeneralTraceAdapter]):
        self.adapter = adapter

    # See GeneralTraceAdapter.get_points for explanation
    def generate_histograms(self, first, last, units, stat_cols, max_points_per_series):
        start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('not a valid range, must have last > first')

        points = self.adapter.get_points(first, last, units, stat_cols, max_points_per_series)
        histograms = OrderedDict()

        for i, stat in enumerate(stat_cols):
            histograms[stat] = np.histogram(points[i], bins = max_points_per_series, range = (first, last))

        info(f'computing histograms took {(time.time() - start) * 1000}ms')

        return histograms
