import dask
import logging
import numpy as np
import numba
import sys

from plato.backend.stat_expression import NoTransform

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.common import logtime

logger = logging.getLogger("plato.backend.processors.branchTrainingHeatmap")

# Produces data requested in terms of one table (e.g. branch index) but looks up data in another
# table (e.g. branch weight updates).
# This class is essentially stateless and can be shared by multiple consumers
class BranchTrainingHeatMapAdapter(BranchTrainingTraceAdapter):

    @logtime(logger)
    def __init__(self, data_source: BranchTrainingDatasource):
        super().__init__(data_source)

    @property
    def num_heatmap_elements(self):
        return self.hm_width * self.hm_height

    # Make a flat (1d) heatmap with appropriate dimensions filled with zeros
    def make_zero_flat_heatmap(self):
        return np.zeros(shape=(self.num_heatmap_elements,))

    # Make a flat (1d) heatmap with appropriate dimensions filled with NaNs
    def make_nan_flat_heatmap(self):
        hm = np.empty(shape=(self.num_heatmap_elements,))
        hm.fill(np.nan)
        return hm

    # Reshape a flat heatmap into a 2d array (height,width)
    def reshape_flat_heatmap(self, flat_hm):
        return flat_hm.reshape(self.hm_height, self.hm_width)

    # Get the weight indices associated with a pair of branch indices
    def get_weight_indices(self, first_branch, last_branch):
        # Map branches to weights first_branch:last_branch
        if last_branch >= self.num_events - 1:  # last_branch is inclusive. If last_branch is last event, use weights through end of data
            first_weight = int(self.pdf_brns.latest_weight_update_index.loc[first_branch])
            last_weight = len(self.pdf_wupdates) - 1
        else:
            # end_weight should be just before the weight corresponding to branch after `last_branch`
            first_weight, end_weight = self.pdf_brns.latest_weight_update_index.loc[[first_branch, last_branch + 1]]
            last_weight = end_weight - 1

        return first_weight, last_weight

    # Return the ranges of weight table update entries needed to satify apply_range on various (first,last) pairs.
    # Feed this output batch_get_weight_buckets
    def batch_pre_apply_range(self, pairs):
        query = []
        result_map = []
        for first_branch, last_branch in pairs:
            # Map branches to weights first_branch:last_branch
            query.append(first_branch)
            if last_branch >= self.num_events - 1:  # last_branch is inclusive. If last_branch is last event, use weights through end of data
                first_weight = -len(query)
                last_weight = len(self.ddf_wupdates) - 1
            else:
                # end_weight should be just before the weight corresponding to branch after `last_branch`
                first_weight = -len(query)
                query.append(last_branch + 1)
                last_weight = -len(query)

            result_map.append([first_weight, last_weight])

        # Run computation in batch
        # Warning: This does work
        results = self.ddf_brns.latest_weight_update_index.loc[query].compute()
        for p in result_map:
            if p[0] < 0:
                idx = -p[0] - 1
                p[0] = results.iloc[idx]
            if p[1] < 0:
                idx = -p[1] - 1
                p[1] = results.iloc[idx] - 1  # make inclusive

        return result_map

    # TODO: This can be strung together with batch_pre_apply_range, though the computations probably can't be combined.
    # Pass each resulting frame to batch_apply_range as `wupdate_frame`
    # NOTE: Testing has shown that manually doing this computation in parallel has no net performance benefit.
    def batch_get_weight_buckets(self, wupdate_ranges):
        computations = []
        for first_weight, last_weight in wupdate_ranges:
            # TODO: skip empty ranges (e.g. last < first)
            computations.append(self.ddf_wupdates.loc[first_weight:last_weight])

        # Run computation in batch
        # Warning: This does work
        results = dask.compute(computations)
        return results[0]  # Returns the list of values

    def batch_apply_range_to_heatmap(self, first_weight, last_weight, wupdate_frame, stat_column_name, flat_hm,
                          value_transform_func=NoTransform):
        # TODO: This vector slicing might need to be done before splitting work into threads

        if (first_weight > last_weight):
            return  # Skip. no need to compute anything

        # Sum up weight rows using numba. This is stupid-fast.
        # First, we have to extract numpy arrays so numba knows how to iterate over them.
        # Then we can call the function to process.
        branch_indices = wupdate_frame.loc[:, 'branch_index'].values
        tables = wupdate_frame.loc[:, 'table'].values
        rows = wupdate_frame.loc[:, 'row'].values
        banks = wupdate_frame.loc[:, 'bank'].values
        stats = wupdate_frame.loc[:, stat_column_name].values

        # Sanity-check size
        wupdate_count = 1 + last_weight - first_weight
        assert (len(wupdate_frame) == wupdate_count), \
            'weight slice was wrong: {} vs {}'.format(len(wupdate_frame), wupdate_count)

        add_wupdates_to_heatmap(branch_indices, tables, rows, banks, stats, wupdate_count, calc_location, self.num_banks, self.num_rows,
                                value_transform_func, flat_hm, None)

    # Take a range of branch training `events` by index and populate a flat (1d) heatmap
    # Try not to use threads
    def apply_range_to_heatmap(self, first_branch, last_branch, stat_column_name, flat_hm,
                               value_transform_func=NoTransform, filter_mask=None):

        assert (first_branch >= 0), first_branch
        assert (last_branch >= 0), last_branch

        first_weight, last_weight = self.get_weight_indices(first_branch, last_branch)

        if first_weight > last_weight:
            return  # Skip. no need to compute anything

        # Weights in the relevant range
        # Get this chunk of data into memory so we can operate on it with jitted python code
        # With jitted iteratoin code this may be a contributor to speed even though it is pretty fast
        wupdate_bucket = self.pdf_wupdates.loc[first_weight:last_weight]

        # Sum up weight rows using numba. This is stupid-fast.
        # First, we have to extract things into numpy arrays so numba knows how to use them.
        # Then we can call the function to process
        branch_indices = wupdate_bucket.loc[:, 'branch_index'].values
        tables = wupdate_bucket.loc[:, 'table'].values
        rows = wupdate_bucket.loc[:, 'row'].values
        banks = wupdate_bucket.loc[:, 'bank'].values
        stats = wupdate_bucket.loc[:, stat_column_name].values

        # Sanity-check size
        wupdate_count = 1 + last_weight - first_weight
        assert (len(wupdate_bucket) == wupdate_count), \
            'weight slice was wrong: {} vs {}'.format(len(wupdate_bucket), wupdate_count)

        add_wupdates_to_heatmap(branch_indices, tables, rows, banks, stats, wupdate_count, calc_location, self.num_banks,
                                self.num_rows, value_transform_func, flat_hm, filter_mask)


# Numpy array column inputs:
#   branch_indices: numpy array with branch index of each weight update row
#   tables: numpy array with table values
#   banks: numpy array with bank values
#   rows: numpy array with row values
#   stats: numpy array with the chosen stat value
#   wupdate_count: number of rows to reach (should match array lengths)
#   hm: flat 1d numpy array representing heatmap. Should be size to support the 'calc_location' function
#   full_branch_mask: A branch mask (or None) representing the entire range of branch indices (not just the values from
#                     0 to wupdate_count)
@numba.jit(nopython=True, nogil=True)
def add_wupdates_to_heatmap(branch_indices, tables, rows, banks, stats, wupdate_count, calc_location, num_banks, num_rows, stat_xform,
                            hm, full_branch_mask):
    for i in range(0, wupdate_count):
        if full_branch_mask is not None:
            if full_branch_mask[branch_indices[i]] == 0:
                continue

        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)

        # Logic for stat (accumulate absolute value or not)
        hm[location] += stat_xform(stats[i])

# Calculate the index of the a given shp heatmap coordinate in the 1d heatmap array
@numba.jit(nopython=True, nogil=True)
def calc_location(table, bank, row, num_banks, num_rows):
    return ((table * num_banks) + bank) * num_rows + row
