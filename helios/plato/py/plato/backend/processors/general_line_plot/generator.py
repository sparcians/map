import logging
import time
from typing import Union

from plato.backend.adapters.general_trace.adapter import GeneralTraceAdapter

logger = logging.getLogger("plato.backend.generalLinePlot")


# Can generate a time-series data for a selected range of time
class GeneralTraceLinePlotGenerator:

    def __init__(self, adapter: Union[GeneralTraceAdapter]):
        self.adapter = adapter

    # See GeneralTraceAdapter.get_points for explanation
    def generate_lines(self, first, last, units, stat_cols, max_points_per_series):
        t_start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('not a valid range, must have last > first')

        points = self.adapter.get_points(first, last, units, stat_cols, max_points_per_series)

        t_end = time.time()
        logger.debug(f'GeneralTraceLinePlotGenerator::generate_lines() over {last - first + 1} branches for {len(stat_cols)} stats took {(t_end - t_start)*1000:.2f}ms')
        if (t_end - t_start) * 1000 > 800:
            logger.warn(f'GeneralTraceLinePlotGenerator::generate_lines() over {last - first + 1} branches for {len(stat_cols)} stats took {(t_end - t_start)*1000:.2f}ms')

        return points
