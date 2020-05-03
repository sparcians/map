import logging
import numpy as np
import numba
import math
import sys
import time

from multiprocessing.pool import ThreadPool

from plato.backend.common import *
from plato.backend.branch_common import *
from plato.backend.stat_expression import *

from .adapter import BranchTrainingHeatMapAdapter, calc_location

logger = logging.getLogger("plato.backend.processors.branchTrainingHeatmap")


@numba.jit(nopython=True, nogil=True)
def apply_bins(last, start_bin_idx, bin_size, bins, max_event, hm):
    num_bins_used = 0
    bin_idx = start_bin_idx
    # for bin_idx in range(start_bin_idx, self.num_bins):
    while bin_idx < len(bins):
        if last >= (bin_idx + 1) * bin_size or last >= max_event:
            hm += bins[bin_idx]
            num_bins_used += 1
            bin_idx += 1
        else:
            end_bin_idx = bin_idx
            break
    else:
        end_bin_idx = len(bins)

    return end_bin_idx, num_bins_used


MODE_SUM = 'sum'  # Sum up all the deltas
MODE_DIFF = 'diff'  # Last value minus the first value
MODE_LAST = 'last'  # Last value
MODE_FIRST = 'first'  # First value

# Object that can generate a heatmap and performs some binning to optimize creation of on-demand heatmaps
# for real-time display.
# Uses an adapters to read raw data and populate heatmap. Adapter also provides transform from the
# event-dimension of this heatmap (e.g. branches) to some underlying dimension (e.g. weight updates) so
# that sampling can be done in that dimension.
#
# TODO: Bins may not be needed immediately. They take time to generate and the first heatmap requested
#       can be generated faster than the bins.
#
# TODO: There is very likely a faster way to generate heatmap bins by iterating the weight-update event
#       table and looking at start/end bin indices rather than getting numpy arrays for each bin.
#
@logtime(logger)
class BranchTrainingHeatMapGenerator:

    # Construct a heatmap generator.
    # bin_size should be at least several times as big as the number of elements in a heatmap or the binning
    #          optimization is lost.
    #
    def __init__(self, adapter: BranchTrainingHeatMapAdapter, stat_columns, bin_size=300000):
        self.adapter = adapter
        self.bin_size = bin_size

        self.num_bins = math.ceil(adapter.num_events / self.bin_size)
        self.bins = {}  # Per stat-col

        self.coalesced_bins_right = {}
        self.wupdate_bin_size = adapter.num_heatmap_elements * 10  # Ensure iterating over 1 heatmap-sized object is much faster than the number of items in a bin.
        self.num_wupate_bins = math.ceil(len(adapter.pdf_wupdates) / self.wupdate_bin_size)

        # Divide into bins with even branch counts. Will look up the corresponding weight-update event table row numbers
        # referenced by the branch training events table and store the information needed to generate a heatmap for each
        # bin.
        apply_range_inputs = self.__compute_adapted_indices()

        # Transform stat names into actual stat columns and modifiers
        real_stats = self.__get_real_stats_and_xforms(stat_columns)

        for stat_name, xform in real_stats:
            logger.debug('Generating {} bins for {} {} to hold {} events'.format(self.num_bins, stat_name, xform, self.adapter.num_events))
            sys.stdout.flush()
            self.bins[(stat_name, xform)] = self.__generate_bins(apply_range_inputs, stat_name, xform)

            t = time.time()
            self.coalesced_bins_right[(stat_name, xform)] = self.__generate_coalesced_bins_right(stat_name, xform)
            d = time.time() - t
            logger.debug(f'Took {d:.2f} s to generate coalesced bins for {stat_name}')

        # Create a small cache for different masks
        self._mask_cache = NRUCache(5)

    # Generate a series of bins representing the right-most (latest) values for each heatmap cell within the bin
    def __generate_coalesced_bins_right(self, stat_col, xform):
        bins = [None] * self.num_wupate_bins
        for bin_idx in range(0, self.num_wupate_bins):
            bins[bin_idx] = self.adapter.make_nan_flat_heatmap()

        banks = self.adapter.pdf_wupdates.bank.values
        tables = self.adapter.pdf_wupdates.table.values
        rows = self.adapter.pdf_wupdates.row.values
        values = self.adapter.pdf_wupdates[stat_col].values

        generate_coalesced_bins_right(bins, xform, self.num_wupate_bins, self.wupdate_bin_size, len(self.adapter.pdf_wupdates), banks, tables, rows, values, self.adapter.num_banks, self.adapter.num_rows)

        return bins

    # Given stats of the form 'table[{table}].statname', figure out which actual stat columns
    # we need to extract and how they must be transformed for implement that stat.
    def __get_real_stats_and_xforms(self, stat_names):
        # Extract up stat columns from compound stats
        real_stats = interpret_compound_stats(stat_names, self.adapter.data_source._compound_stats)

        # Adjust any stat name with the table prefix into a real column name because that is how the heatmap operates
        return list(map(lambda p: (p[0].replace(self.adapter.data_source.TABLE_STAT_PREFIX, ''), p[1]), real_stats))

    def __compute_adapted_indices(self):
        t = time.time()

        # Start branch index of each bin
        bin_start_list = range(0, self.adapter.num_events, self.bin_size)
        assert(len(bin_start_list) == self.num_bins)

        # Figure out ranges of weight updates based on training event table
        # NOTE: This is the slow part. Actually counting within each bucket (later) is fast
        branch_ranges_incl = list(map(lambda r: (r, r+self.bin_size-1), bin_start_list))
        weight_ranges_incl = self.adapter.batch_pre_apply_range(branch_ranges_incl)

        duration = time.time() - t
        logger.debug('Generating {} table indices for bins took {} s'.format(self.num_bins, duration))
        sys.stdout.flush()


        t = time.time()

        weight_frames = self.adapter.batch_get_weight_buckets(weight_ranges_incl)

        # Construct num_bins tuples, each containing a heatmap range, the weight-data frame, and the bin index to
        # populate for that heatmap
        bin_indices = range(0, len(bin_start_list))
        apply_range_inputs = list(zip(weight_ranges_incl, weight_frames, bin_indices))

        # Log timing
        duration = time.time() - t
        logger.debug('Getting {} table weight buckets for bins took {} s'.format(self.num_bins, duration))
        sys.stdout.flush()

        return apply_range_inputs

    def __generate_bins(self, apply_range_inputs, stat_col, xform):
        t = time.time()
        bins = np.zeros(shape=(self.num_bins, self.adapter.num_heatmap_elements))

        # make_bin using pre-computed data-frame
        def populate_bins(items):
            for item in items:
                # Unpack work
                bin_range = item[0]
                data_frame = item[1]
                bin_idx = item[2]

                # Weight update event indices
                first = bin_range[0]
                last = bin_range[1]

                # Fill the heatmap
                self.adapter.batch_apply_range_to_heatmap(first, last, data_frame, stat_col, bins[bin_idx,:], value_transform_func=xform)


        # NOTE: Multiprocessing doesn't work here - there is too much data to transfer. It is possible that a
        #       numpy memmap solution will be better and can run in a multiprocess environment. Someone should try that.
        if 1:
            # Partition the heatmap 'apply_range' inputs into thread work groups and use a pool to generate bins
            NUM_THREADS = 8
            def partition(l, chunk_size):
                for i in range(0, len(l), chunk_size):
                    yield l[i:i+chunk_size]
            partitioned_apply_range_inputs = list(partition(apply_range_inputs, math.ceil(len(apply_range_inputs) / NUM_THREADS)))

            assert(len(partitioned_apply_range_inputs) <= NUM_THREADS), '{} != {}'.format(len(partitioned_apply_range_inputs),NUM_THREADS)

            pool = ThreadPool()
            pool.map(populate_bins, partitioned_apply_range_inputs)
            pool.close()
            pool.join()
        else:
            # Fallback non-threaded mode (debug only)
            populate_bins(apply_range_inputs)

        # Log timing
        duration = time.time() - t
        logger.debug('Generating {} bins for {} took {} s'.format(self.num_bins, stat_col, duration))
        sys.stdout.flush()

        return bins

    # Debugging utility. Accepts only concrete stat names (not compound)
    def _check_bin_sum(self, stat_col):
        bins = self.bins[(stat_col, NoTransform)]
        cs = 0
        for b in bins:
            cs += b.sum()
        return cs

    # Get a 2d Heatmap over range of branch index: first (incusive) to last (inclusive)
    # where these are indices of BRANCHES (from ddf_branches)
    # `allow_bins` lets the heatmap rely on the `bins` optimization. Disable this only for debugging
    # We could optimize this to only include changes at the endpoints from last update rather than re-iterating, but its
    # already super-fast.
    def generate_2d_heatmap(self, first_unit, last_unit, units, stat_col, allow_bins=True, allow_threads=False, branch_predictor_filtering={}, mode=MODE_SUM):
        # Convert first/last to branch indices because that's what the heatmap operates on. It is ok that
        # interpolation within this range is done by branch indices because the heatmap is just summing things up
        # anyway.
        unit_stat = self.adapter.get_stat_for_units(units)
        first_row, last_row = self.adapter.lookup_rows_for_values(first_unit, last_unit, unit_stat)

        # Interpret stat
        if type(stat_col) != str:
            raise ValueError('stat_col was not a string')

        # Transform stat names into actual stat columns and modifiers
        real_stat_cols = self.__get_real_stats_and_xforms([stat_col])

        # Bounds checking
        if not (first_row >= 0):
            raise ValueError('first/last not in range of data')
        if not (last_row >= first_row):
            raise ValueError('first/last not in range of data')
        if not (last_row <= self.adapter.num_events):
            raise ValueError('first/last not in range of data')

        # Create a filter mask for the selected branches if required
        brnfilt = make_branch_filter(branch_predictor_filtering)
        # TODO: Move this logic into adapter
        if brnfilt is not None:
            if mode not in [MODE_SUM]:
                raise ValueError(f'Cannot generate a heatmap with a branch filter using a mode other than "sum" because individual branches must be filtered out')

            if brnfilt not in self._mask_cache:
                # TODO: This cache may need a mutex if this is acutally multithreaded
                # Make the entire mask here and now. The will be the last time it is needed
                filter_mask, npoints = brnfilt.make_mask(0, len(self.adapter.pdf_brns), self.adapter.pdf_brns, self.adapter.pdf_wupdates)
                self._mask_cache[brnfilt] = filter_mask
            else:
                filter_mask = self._mask_cache[brnfilt]
        else:
            filter_mask = None

        intermediate = []
        for col, xform in real_stat_cols:
            intermediate.append(self._generate_2d_heatmap(first_row, last_row, col, xform, filter_mask, allow_bins=allow_bins, mode=mode))

        hm = assemble_compound_stat(stat_col, self.adapter.data_source._compound_stats, *intermediate)

        # Convert flat heatmap to a matrix
        return self.adapter.reshape_flat_heatmap(hm)

    # Generate a 2d heatmap
    def _generate_2d_heatmap(self, first_row, last_row, stat_col, xform=NoTransform, filter_mask=None, allow_bins=True, mode=MODE_SUM):
        # Create the empty flat heatmap to which bins will be added
        hm = self.adapter.make_zero_flat_heatmap()

        if mode == MODE_SUM:
            return self._generate_2d_sum_heatmap(hm, first_row, last_row, stat_col, xform, filter_mask, allow_bins)
        elif mode == MODE_DIFF:
            return self._generate_2d_diff_heatmap(hm, first_row, last_row, stat_col, xform, allow_bins)
        elif mode == MODE_LAST:
            return self._generate_2d_last_heatmap(hm, first_row, last_row, stat_col, xform, allow_bins)
        elif mode == MODE_FIRST:
            return self._generate_2d_first_heatmap(hm, first_row, last_row, stat_col, xform, allow_bins)
        else:
            raise NotImplementedError(f'heatmap mode {mode} not implemented')

    def _generate_2d_sum_heatmap(self, hm, first_row, last_row, stat_col, xform, filter_mask, allow_bins):
        t_start = time.time()

        range_func = self.adapter.apply_range_to_heatmap

        # Calculate indices of bins to use
        c_start_bin_idx = math.ceil(first_row / self.bin_size)
        c_end_bin_idx = math.floor(last_row / self.bin_size)

        num_bins_used = 0
        if allow_bins and filter_mask is None:  # Cannot use bins if filtering on specific branches
            if c_end_bin_idx > c_start_bin_idx:
                slice = self.bins[(stat_col, xform)][c_start_bin_idx:c_end_bin_idx].sum(axis=0)
                hm += slice
                num_bins_used = c_end_bin_idx - c_start_bin_idx

                # Get raw data at the endpoints
                if c_start_bin_idx > 0:
                    range_func(first_row, c_start_bin_idx * self.bin_size - 1, stat_col, hm, value_transform_func=xform)
                    # logger.debug('  hm=', hm.sum())

                if c_end_bin_idx < self.num_bins:
                    range_func(c_end_bin_idx * self.bin_size, last_row, stat_col, hm, value_transform_func=xform)
                    # logger.debug('  hm=', hm.sum())
            else:
                range_func(first_row, last_row, stat_col, hm)

        else:
            # TODO: Need to optimize heatmap with branch filtering
            range_func(first_row, last_row, stat_col, hm, value_transform_func=xform, filter_mask=filter_mask)

        duration = time.time() - t_start

        # Logging for debug
        if num_bins_used == 0:
            pts_per_bin = 0
        else:
            pts_per_bin = hm.sum() / num_bins_used
        logger.debug('computing 2d sum heatmap (allow_bins={}) over {} rows took {} s and visited {} bins ({}/bin)' \
                     .format(allow_bins, last_row - first_row + 1, duration, num_bins_used, pts_per_bin))

        return hm

    # Generate a 2d heatmap by diffing the endpoints
    # NOTE: Filtering not supported because it doesn't make sense
    def _generate_2d_diff_heatmap(self, hm, first_row, last_row, stat_col, xform, use_bins=True):
        t_start = time.time()

        first_weight_idx, last_weight_idx = self.adapter.get_weight_indices(first_row, last_row)

        if first_weight_idx > last_weight_idx:
            return hm  # Skip. no need to compute anything

        # Work from the first weight position upward until each cell has been touched
        banks = self.adapter.pdf_wupdates.bank.values
        tables = self.adapter.pdf_wupdates.table.values
        rows = self.adapter.pdf_wupdates.row.values
        values = self.adapter.pdf_wupdates[stat_col].values

        if use_bins:
            # Use the optimized approach to calculate the diff
            hm_first = last_values(self.coalesced_bins_right[(stat_col, xform)], 0, first_weight_idx - 1,  xform, self.wupdate_bin_size,
                                    banks, tables, rows, values, self.adapter.num_banks, self.adapter.num_rows, len(hm))

            hm_last = last_values(self.coalesced_bins_right[(stat_col, xform)], 0, last_weight_idx, xform, self.wupdate_bin_size,
                                  banks, tables, rows, values, self.adapter.num_banks, self.adapter.num_rows, len(hm))

            # Fill in the blanks with 0 (should be configurable)
            hm_first[np.isnan(hm_first)] = 0
            hm_last[np.isnan(hm_last)] = 0

            hm = hm_last - hm_first

            duration = time.time() - t_start
            logger.debug('computing 2d diff heatmap over {} rows took {} s'
                         .format(last_row - first_row + 1, duration))

            return hm

        else:
            hm_first = np.empty(hm.shape, dtype=hm.dtype)
            hm_first.fill(np.nan)
            hm_last = np.empty(hm.shape, dtype=hm.dtype)
            hm_last.fill(np.nan)

            # TODO: Optimize this with bins that indicate the first and last value of each (bank,table,row) for any period
            num_points_visited = 0
            num_points_visited += get_latest_heatmap_values_descending(hm_first, xform, 0, first_weight_idx-1, banks, tables,
                                                                      rows, values, self.adapter.num_banks, self.adapter.num_rows)
            num_points_visited += get_latest_heatmap_values_descending(hm_last, xform, 0, last_weight_idx, banks, tables,
                                                                       rows, values, self.adapter.num_banks, self.adapter.num_rows)

            # Fill in the blanks with 0 (should be configurable)
            hm_first[np.isnan(hm_first)] = 0
            hm_last[np.isnan(hm_last)] = 0

            hm = hm_last - hm_first

            # Log the duration. Note how many points we covered to fill in the heatmap. This number could aproach 0% or be
            # as high as 200% if a cell is not touched during the given interval.
            duration = time.time() - t_start
            logger.debug('computing 2d diff heatmap over {} rows took {} s and visited {} points ({:.2f}%))'
                         .format(last_row - first_row + 1, duration, num_points_visited, 100.0 * num_points_visited / (last_weight_idx - first_weight_idx + 1)))

            return hm

    def _generate_2d_last_heatmap(self, hm, first_row, last_row, stat_col, xform, use_bins):
        t_start = time.time()

        first_weight_idx, last_weight_idx = self.adapter.get_weight_indices(first_row, last_row)

        if first_weight_idx > last_weight_idx:
            return hm  # Skip. no need to compute anything

        # Work from the first weight position upward until each cell has been touched
        banks = self.adapter.pdf_wupdates.bank.values
        tables = self.adapter.pdf_wupdates.table.values
        rows = self.adapter.pdf_wupdates.row.values
        values = self.adapter.pdf_wupdates[stat_col].values

        if use_bins:
            hm = last_values(self.coalesced_bins_right[(stat_col, xform)], 0, last_weight_idx, xform,
                             self.wupdate_bin_size,
                             banks, tables, rows, values, self.adapter.num_banks, self.adapter.num_rows, len(hm))

            # Fill in the blanks with 0 (should be configurable)
            hm[np.isnan(hm)] = 0

            duration = time.time() - t_start
            logger.debug('computing 2d last heatmap over {} rows took {} s'
                         .format(last_weight_idx - first_weight_idx + 1, duration))

            return hm

        else:
            hm.fill(np.nan)

            num_points_visited = 0
            num_points_visited += get_latest_heatmap_values_descending(hm, xform, 0,
                                                                       last_weight_idx, banks, tables,
                                                                       rows, values, self.adapter.num_banks,
                                                                       self.adapter.num_rows)

            # Fill in the blanks with 0 (should be configurable)
            hm[np.isnan(hm)] = 0

            # Log the duration. Note how many points we covered to fill in the heatmap. This number could approach 0% or
            # be as high as 200% if a cell is not touched during the given interval.
            duration = time.time() - t_start
            logger.debug('computing 2d last heatmap over {} rows took {} s and visited {} points ({:.2f}%))'
                         .format(last_row - first_row + 1, duration, num_points_visited,
                                 100.0 * num_points_visited / (last_row - first_row + 1)))

            return hm

    def _generate_2d_first_heatmap(self, hm, first_row, last_row, stat_col, xform, use_bins):
        t_start = time.time()

        first_weight_idx, last_weight_idx = self.adapter.get_weight_indices(first_row, last_row)

        if first_weight_idx > last_weight_idx:
            return hm # Skip. no need to compute anything

        # Work from the first weight position upward until each cell has been touched
        banks = self.adapter.pdf_wupdates.bank.values
        tables = self.adapter.pdf_wupdates.table.values
        rows = self.adapter.pdf_wupdates.row.values
        values = self.adapter.pdf_wupdates[stat_col].values

        if use_bins:
            # Using 'last' values here because we want everything up to `first_row`
            hm = last_values(self.coalesced_bins_right[(stat_col, xform)], 0, first_weight_idx - 1, xform,
                             self.wupdate_bin_size,
                             banks, tables, rows, values, self.adapter.num_banks, self.adapter.num_rows, len(hm))

            # Fill in the blanks with 0 (should be configurable)
            hm[np.isnan(hm)] = 0

            duration = time.time() - t_start
            logger.debug('computing 2d first heatmap over {} rows took {} s'
                         .format(last_weight_idx - first_weight_idx + 1, duration))

            return hm

        else:
            hm.fill(np.nan)

            num_points_visited = 0
            num_points_visited += get_latest_heatmap_values_descending(hm, xform, 0,
                                                                       first_weight_idx - 1, banks, tables,
                                                                       rows, values, self.adapter.num_banks,
                                                                       self.adapter.num_rows)

            # Fill in the blanks with 0 (should be configurable)
            hm[np.isnan(hm)] = 0

            # Log the duration. Note how many points we covered to fill in the heatmap. This number could approach 0% or
            # be as high as 200% if a cell is not touched during the given interval.
            duration = time.time() - t_start
            logger.debug('computing 2d first heatmap over {} rows took {} s and visited {} points ({:.2f}%))'
                         .format(last_weight_idx - first_weight_idx + 1, duration, num_points_visited,
                                 100.0 * num_points_visited / (last_row - first_row + 1)))

            return hm

    # Invokes generate_2d_matrix but also calculates the (table,bank) means and the row means
    def generate_2d_heatmap_with_profiles(self, first_unit, last_unit, units, stat_col, allow_bins=True, allow_threads=False, branch_predictor_filtering={}, mode=MODE_SUM):
        matrix = self.generate_2d_heatmap(first_unit, last_unit, units, stat_col, allow_bins, allow_threads, branch_predictor_filtering, mode)
        matrix = matrix.astype(dtype='f4', copy=False)

        column_means = matrix.sum(axis=1).astype(dtype='f4', copy=False) / self.adapter.hm_height
        row_means = matrix.sum(axis=0).astype(dtype='f4', copy=False) / self.adapter.hm_width

        return matrix, column_means, row_means

    # Invokes generate_2d_matrix_with_profiles but also calculates a handful of stats
    def generate_2d_heatmap_with_profiles_and_stats(self, first_unit, last_unit, units, stat_col, allow_bins=True, allow_threads=False, branch_predictor_filtering={}, mode=MODE_SUM):
        matrix, column_means, row_means = self.generate_2d_heatmap_with_profiles(first_unit, last_unit, units, stat_col, allow_bins, allow_threads, branch_predictor_filtering, mode)

        column_stddevs = matrix.std(axis=1).astype(dtype='f4', copy=False)
        column_mins = np.amin(matrix, axis=1).astype(dtype='f4', copy=False)
        column_maxs = np.amax(matrix, axis=1).astype(dtype='f4', copy=False)
        column_medians = np.median(matrix, axis=1).astype(dtype='f4', copy=False)

        return matrix, column_means, row_means, column_stddevs, column_mins, column_maxs, column_medians


# Calculate the latest value for every cell in the heatmap starting from `first_weight_index` up to `last_weight_index`
# (inclusive), stopping once all cells in the heatmap have been populated or `last_weight_index` is reached.
@numba.jit(nopython=True, nogil=True)
def get_latest_heatmap_values_ascending(hm, xform, first_weight_index, last_weight_index, banks, tables, rows, values, num_banks, num_rows):
    num_cells = len(hm)
    i = first_weight_index
    num_cells_assigned = 0
    while i <= last_weight_index:
        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)
        if np.isnan(hm[location]):
            hm[location] = xform(values[i])
            num_cells_assigned += 1
            if num_cells_assigned == num_cells:
                break  # Done with all cells

        i += 1

    return i - first_weight_index


# Calculate the latest value for every cell in the heatmap starting from `last_weight_index` down to
# `first_weight_index` (inclusive) stopping once all cells in the heatmap have been populated or `last_weight_index` is
# reached.
@numba.jit(nopython=True, nogil=True)
def get_latest_heatmap_values_descending(hm, xform, first_weight_index, last_weight_index, banks, tables, rows, values, num_banks, num_rows):
    num_cells = len(hm)
    i = last_weight_index
    num_cells_assigned = 0
    while i >= first_weight_index:
        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)
        if np.isnan(hm[location]):
            hm[location] = xform(values[i])
            num_cells_assigned += 1
            if num_cells_assigned == num_cells:
                break  # Done with all cells

        i -= 1

    return last_weight_index - i

# Fill a list of bins with rightward coalescing. This walks bins left to right and within each bin starts from the last
# weight update event and populates each heatmap cell currently containing a nan with the value from that event. Once
# the start of the current bin is reached, any nans remaining are filled by values from the bin before (left) the
# current bin. This allows an algorithm that is looking for the LAST event on all heatmap cells to walk a few events and
# then use all the data from a bin upon reaching a bin boundary.
#
# Note that this function takes a list of bins, which will be deprecated by numba but there is no replacement yet.
@numba.jit(nopython=True, nogil=True)
def generate_coalesced_bins_right(bins, xform, num_bins, bin_size, num_wupdates, banks, tables, rows, values, num_banks, num_rows):
    bin_idx = 0
    while bin_idx < num_bins:
        hm = bins[bin_idx]

        generate_coalesced_bin_right(hm, bin_idx, num_wupdates, xform, tables, banks, rows, values, num_banks, num_rows, bin_size)

        bin_idx += 1


@numba.jit(nopython=True, nogil=True)
def generate_coalesced_bin_right(hm, bin_idx, num_wupdates, xform, tables, banks, rows, values, num_banks, num_rows, bin_size):
    # Get the last item in the bin
    i = min((bin_idx + 1) * bin_size - 1, num_wupdates - 1)

    # Work from last item in the bin to the first
    while i >= bin_idx * bin_size:
        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)
        if np.isnan(hm[location]):
            hm[location] = xform(values[i])

        i -= 1


# Calculate the last values of the heatmap in a given range [first_idx, last_idx]. This iterates weight update events
# descending by index and uses right-coalesced bins as an optimization to skip large ranges of individual points.
@numba.jit(nopython=True, nogil=True)
def last_values(r_bins, first_idx, last_idx, xform, bin_size, banks, tables, rows, values, num_banks, num_rows, hm_size):
    num_filled = 0

    hm = np.empty(hm_size)
    hm.fill(np.nan)

    i = last_idx
    while i >= first_idx:

        if num_filled == hm_size:
            break  # All values filled

        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)

        if (i + 1) % bin_size == 0:  # Just hit the edge of a bin moving left
            # Check if start endpoint is out of this range
            bin_idx = ((i + 1) // bin_size) - 1

            if first_idx < bin_idx * bin_size:
                # This bin is totally within our range
                bin = r_bins[bin_idx]
                mask = np.isnan(hm)
                hm[mask] = bin[mask]

                # Re-do count.
                # NO easy wait to see what change other chan doing a mask on the bin after its already masked.
                num_filled = (~np.isnan(hm)).sum()

                i = (bin_idx * bin_size) - 1  # Final item in previous bin
                continue

        if np.isnan(hm[location]):
            hm[location] = xform(values[i])
            num_filled += 1

        i -= 1

    return hm
