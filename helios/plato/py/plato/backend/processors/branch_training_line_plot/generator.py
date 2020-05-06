import time
import logging
import numpy as np

from plato.backend.branch_common import *

from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.general_line_plot.generator import GeneralTraceLinePlotGenerator

logger = logging.getLogger("plato.backend.branchTrainingLinePlot")

# Can generate a time-series data for a selected range of time
class BranchTrainingLinePlotGenerator(GeneralTraceLinePlotGenerator):
    def __init__(self, adapter: BranchTrainingTraceAdapter):
        super().__init__(adapter)

    # See GeneralTraceAdapter.get_points for explanation
    def generate_lines(self, first, last, units, stat_cols, max_points_per_series, branch_predictor_filtering={}, point_cache=True):
        t_start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('first/last not in range of data')

        brnfilt = make_branch_filter(branch_predictor_filtering)

        points = self.adapter.get_points(first, last, units, stat_cols, max_points_per_series, brnfilt, point_cache)

        duration = time.time() - t_start
        logger.debug(f'computing time-series over {last - first + 1} branches for {len(stat_cols)} stats took {duration} s')

        return points

    # Generate a series of points using int64 info. See `generate_lines`
    def generate_simple_int_points(self, first, last, units, stat_cols, branch_predictor_filtering={}, point_cache=True):
        t_start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('first/last not in range of data')

        brnfilt = make_branch_filter(branch_predictor_filtering)

        points = self.adapter.get_simple_int_points(first, last, units, stat_cols, brnfilt, point_cache)

        duration = time.time() - t_start
        logger.debug(
            f'computing time-series and modalities over {last - first + 1} branches for {len(stat_cols)} stats took {duration} s')

        return points

    # Generate lines along with modality and downsamplg info. See `generate_lines`
    def generate_lines_and_more(self, first, last, units, stat_cols, max_points_per_series, branch_predictor_filtering={}, point_cache=True):
        t_start = time.time()

        # Bounds checking
        if not (first >= 0):
            raise ValueError('first/last not in range of data')
        if not (last >= first):
            raise ValueError('first/last not in range of data')

        brnfilt = make_branch_filter(branch_predictor_filtering)

        output_meta = {'downsampling': None}
        points = self.adapter.get_points(first, last, units, stat_cols, max_points_per_series, brnfilt,
                                         point_cache, output_meta)

        modalities = calc_modalities(points, 5)
        if points.shape[1] == 0:
            yExtents = [-1, 1]
        else:
            yExtents = [np.nanmin(points[1:]), np.nanmax(points[1:])]

        duration = time.time() - t_start
        logger.debug(f'computing time-series and modalities over {last - first + 1} branches for {len(stat_cols)} stats took {duration} s')

        return points, modalities, output_meta['downsampling'], yExtents


# Calculate and return the modality of a single series (how many different values there are) up to a maximum number.
# Modalities larger than the given maximum return -1.
@numba.jit(nopython=True, nogil=True)
def calc_modality(row, max_modality):
    s = set()
    for x in row:
        if x not in s:
            s.add(x)
            if len(s) > max_modality:
                return -1  # unknown
    return len(s)


# Calculate the modalities (number of different values in the series) for each series in the points matrix
def calc_modalities(points, max_modality):
    modalities = np.empty(points.shape[0], dtype='i2')
    for r in range(points.shape[0]):
        modalities[r] = calc_modality(points[r, :], max_modality)
    return modalities

