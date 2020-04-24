import dask
import numpy as np
import numba
import math
import time

from plato.backend.common import *
from plato.backend.branch_common import *
from plato.backend.units import Units
from plato.backend.common import NRUCache
from plato.backend.stat_expression import *

from plato.backend.adapters.general_trace.adapter import GeneralTraceAdapter, downsample_copy_mean
from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource


# Produces data requested in terms of one table (e.g. branch index) but looks up data in another
# table (e.g. branch weight updates).
# This class is essentially stateless and can be shared by multiple consumers
class BranchTrainingTraceAdapter(GeneralTraceAdapter):

    def __init__(self, data_source: BranchTrainingDatasource):
        super().__init__(data_source)

        # BP-specific

        # Dask
        self.ddf_brns = data_source.ddf_branch_training_events
        self.ddf_wupdates = data_source.ddf_weight_update_events

        # Pandas (for non-threaded stuff)
        self.pdf_brns = data_source.pdf_branch_training_events
        self.pdf_wupdates = data_source.pdf_weight_update_events

        # Calculate the heatmap dimensions
        # WARNING: time-consuming work. This takes about 0.5s on a 30m row set
        # TODO: store this in data tables when writing file instead of iterating here!
        table_min, table_max, row_min, row_max, bank_min, bank_max = \
            dask.compute(
                self.ddf_wupdates.table.min(), self.ddf_wupdates.table.max(),
                self.ddf_wupdates.row.min(), self.ddf_wupdates.row.max(),
                self.ddf_wupdates.bank.min(), self.ddf_wupdates.bank.max()
                )

        self.num_tables = (1 + table_max - table_min)
        self.num_rows = 1 + row_max - row_min
        self.hm_width = self.num_rows
        self.num_banks = (1 + bank_max - bank_min)
        self.hm_height = self.num_banks * self.num_tables

        self._mask_cache = NRUCache(5)

    # Override: We know this trace has trn_idx and cycles columns
    def get_stat_for_units(self, units):
        if units == Units.BRANCH_TRAINING_INDEX:
            return 'trn_idx'
        elif units == Units.CYCLES:
            return 'cycle'
        elif units == Units.INSTRUCTIONS:
            return 'instructions'

        return super().get_stat_for_units(units)

    # Override: Optimize trn_idx lookup
    def lookup_rows_for_values(self, first, last, unit_stat):
        if unit_stat == 'trn_idx':
            # Optimization for branch-predictor traces: convert training-index to row index because
            # that is known to be the index column of the trace.
            first_row = first
            last_row = last
            return int(first_row), int(last_row)

        return super().lookup_rows_for_values(first, last, unit_stat)

    # Get the range of weight update indices associated with a first branch and a last branch.
    # Returns a range [first,last)
    def weight_update_range_from_branch_indices(self, first_row_idx, last_row_idx):
        first_wupdate_idx = self.pdf_brns.latest_weight_update_index.iloc[first_row_idx]
        if last_row_idx == len(self.pdf_brns) - 1:
            end_wupdate_idx = len(self.pdf_wupdates)
        else:
            end_wupdate_idx = self.pdf_brns.latest_weight_update_index.iloc[last_row_idx + 1]

        return first_wupdate_idx, end_wupdate_idx

    # Return the mask from the cache or None if mask is None
    def _get_whole_mask(self, filt):
        if filt is None:
            return None

        # Make a full mask
        k = filt  # BranchFilterWrapper is fully hashable and comparable so it can be a key in the cache
        if k not in self._mask_cache:
            # Make the entire mask here and now. The will be the last time it is needed
            filt_mask, npoints = filt.make_mask(0, len(self.pdf_brns) - 1, self.pdf_brns, self.pdf_wupdates)
            self._mask_cache[k] = filt_mask

        else:
            filt_mask = self._mask_cache[k]

        return filt_mask

    # Gets points for some simple per-branch stats without any downsampling
    def get_simple_int_points(self, first, last, units, stat_cols, filt=None, cache=True):
        first_row_idx, last_row_idx, unit_stat = self.range_to_rows(first, last, units)

        filt_mask = self._get_whole_mask(filt)

        if filt_mask is not None:
            # Get the appropriate portion of the mask
            filt_mask = filt_mask[first_row_idx: last_row_idx + 1]
            num_points = filt_mask.sum()
        else:
            num_points = last_row_idx - first_row_idx + 1

        # Final array ordered by original order of stats
        arr = np.zeros(shape=(len(stat_cols), num_points), dtype='i8')  # row 0 is extra row is for units

        # Generate the downsampled pure trace events from the main dataframe
        downsample = 0
        stat_to_row_map = dict(map(lambda x: (x[1],(x[0], NoTransform)), enumerate(stat_cols)))
        self._copy_points(first_row_idx, last_row_idx, unit_stat, stat_to_row_map, downsample, arr, filt_mask, cache=cache)

        return arr


    # Overrides the base adapter get_points to support also examining the secondary data set of weight update events
    # Unlike the base class, this also supports stat columns in the form "table[t]/stat" where t is a table number and
    # stat is the name of some stat from the weight-update table in this data-set
    def get_points(self, first, last, units, stat_cols, max_points_per_series, filt=None, cache=True, output_meta={}):
        first_row_idx, last_row_idx, unit_stat = self.range_to_rows(first, last, units)

        # Determine the real stat columns needed to satisfy this
        compound_items = []  # For all stats: (final output index, intermediate index, [real stats], stat_name in compound stats list)
        real_stat_cols = []
        any_multicol_stats = False
        for i, stat in enumerate(stat_cols):
            if stat in self.data_source._compound_stats:
                # This is a match on the compound stat so it must be a non-table-specific stat
                real_stats = interpret_compound_stats([stat], self.data_source._compound_stats)
                real_stats = list(map(lambda x: (x, stat, i), real_stats))
                compound_items.append((i, len(real_stat_cols), real_stats, stat))
                real_stat_cols.extend(real_stats)

                if len(real_stats) > 1:
                    any_multicol_stats = True

                continue

            if VARIABLE_STAT_SEPARATOR in stat:
                # This is a stat with a variable in the name.
                # Parse out the table and variable prefix and the stat name suffix
                table_prefix, stat_name = stat.split(VARIABLE_STAT_SEPARATOR)

                # Look up the stat by its short name
                if stat_name in self.data_source._compound_stats_by_short_name:
                    real_stats = interpret_compound_stats([stat_name], self.data_source._compound_stats_by_short_name)

                    # Apply the same table prefix to the reference stats. We're assuming that they will also be per-table stats since the compound stat is
                    real_stats = [((table_prefix + VARIABLE_STAT_SEPARATOR + name, xform), stat, i) for name,xform in real_stats ]

                    compound_items.append((i, len(real_stat_cols), real_stats, stat_name))

                    real_stat_cols.extend(real_stats)

                    if len(real_stats) > 1:
                        any_multicol_stats = True

                    continue

            # Not a compound stat
            compound_items.append((i, len(real_stat_cols), None, None))
            real_stat_cols.append(((stat, NoTransform), None, i))

        # Check stat col are valid map their names to indices
        train_stats = []  # [(stat, (series#, xform)), ... ]
        weight_stats = []  # [((stat, table), (series#, xform), ... ]
        for i, ((stat, xform), compound_stat_name, original_i) in enumerate(real_stat_cols):
            # TODO: Need to support compound stats that are not per-table here
            if stat in self.data_source._brn_stats:
                train_stats.append((stat, (i + 1, xform)))  # row 0 is always 'time' units
            else:
                if VARIABLE_STAT_SEPARATOR not in stat:
                    raise KeyError('No stat {} in this datasource {}. Known stats are '
                                   .format(stat, self.data_source,self.data_source.stats))

                # This is a stat with a variable in the name.
                # Parse out the table number
                table_str, stat_name = stat.split(VARIABLE_STAT_SEPARATOR)

                if stat_name not in self.data_source._wupdate_stats:
                    raise KeyError('No stat {} in per-table data for datasource {}. Known stats are {}'
                                   .format(stat, self.data_source, self.data_source._wupdate_stats))
                j = table_str.find('table[')
                assert (j >= 0), 'Malformed table-stat prefix: "{}". Should be "table[x]"'.format(table_str)
                k = table_str.find(']')
                assert (k >= 0), 'Malformed table-stat prefix: "{}". Should be "table[x]"'.format(table_str)
                table_num = int(table_str[j + len('table['):k])
                weight_stats.append(((stat_name, table_num), (i + 1, xform)))  # row 0 is always 'time'; units

        first_row_idx = int(first_row_idx)
        last_row_idx = int(last_row_idx)

        npoints = last_row_idx - first_row_idx + 1

        whole_filt_mask = self._get_whole_mask(filt)

        if whole_filt_mask is not None:
            # Calculate number of points in downsampled region to estimate the need for and amount of downsampling
            npoints = whole_filt_mask[first_row_idx: last_row_idx + 1].sum()

        # Determine amount of downsampling needed based on the points in the range from the filter mask (or all points
        # if no mask present).
        downsample, num_points = self.calc_downsampling(npoints, max_points_per_series)

        if 'downsampling' in output_meta:
            output_meta['downsampling'] = downsample

        # Align endpoints for downsampling that is consistent since it begins at downsample-multiples of points
        first_row_idx, last_row_idx, num_points = self.align_for_downsampling(first_row_idx, last_row_idx, num_points, downsample)

        if whole_filt_mask is not None:
            # Get the actual mask for the final aligned range based on the filter now that our endpoints are adjusted
            filt_mask = whole_filt_mask[first_row_idx : last_row_idx + 1]
            num_points = math.ceil(filt_mask.sum() / (max(downsample, 1)))  # Calculate points in downsampled data
        else:
            filt_mask = None

        # Final array ordered by original order of stats
        arr = np.zeros(shape=(len(real_stat_cols) + 1, num_points))  # row 0 is extra row is for units

        # Generate the downsampled pure trace events from the main dataframe
        self._copy_points(first_row_idx, last_row_idx, unit_stat, train_stats, downsample, arr, filt_mask, cache=cache)

        # Collect stats from weight update events based on the rows
        if len(weight_stats) > 0:
            # TODO: get these values and pack them into the same shape as above. Use numba?
            weight_stat_keys = [pair[0] for pair in weight_stats]
            orig_column_names, orig_column_tables = zip(*weight_stat_keys)  # stat names
            reference_columns = ['branch_index', 'table']
            unique_column_names = set(reference_columns + list(orig_column_names))
            columns = list(unique_column_names)
            first_wupdate_idx, end_wupdate_idx = self.weight_update_range_from_branch_indices(first_row_idx, last_row_idx)
            num_wupdates = end_wupdate_idx - first_wupdate_idx

            if not cache or filt_mask is not None:
                # Grab the weight-update arrays in the appropriate range
                weight_values = self.pdf_wupdates.iloc[first_wupdate_idx:end_wupdate_idx]

                # Transform weight update stats this into branch-index space
                # TODO: Should combine coalesce_weight_update_stat_to_branches + downsample_copy_mean into a single
                #       numba-function for efficiency.
                wsa = np.zeros(shape=(len(weight_stat_keys), last_row_idx - first_row_idx + 1))

                # The initial jit compile here is 99.9% of the time in this function.
                # The jitted code is fast so next time it will run quickly.
                arr_bi = weight_values.branch_index.values
                arr_tbl = weight_values.table.values
                for ci, (col_name, table_idx) in enumerate(weight_stat_keys):
                    arr_col = weight_values[col_name].values
                    ws = wsa[ci, :]
                    coalesce_weight_update_stat_to_branches(arr_bi, arr_tbl, arr_col, ws, first_row_idx, num_wupdates, table_idx)

            # Copy weight update event values into the correct rows, masking if necessary
            for i, ((col_name, table_idx), (out_row, xform)) in enumerate(weight_stats):
                if downsample == 0:
                    if filt_mask is not None:
                        arr[out_row, :] = xform(wsa[i, :][filt_mask == 1])
                    else:
                        if cache:
                            ds_first, ds_last = self.convert_to_downsample_indices(first_row_idx, last_row_idx, downsample)
                            self.__cached(arr[out_row, :], table_idx, col_name, ds_first, ds_last, downsample, downsample_copy_mean, xform)
                        else:
                            downsample_copy_mean(xform(wsa[i, :]), arr[out_row, :], downsample)
                else:
                    if filt_mask is not None:
                        # TODO: Support downsampling of filtered values
                        downsample_copy_mean(xform(wsa[i, :][filt_mask == 1]), arr[out_row, :], downsample)
                    else:
                        if cache:
                            ds_first, ds_last = self.convert_to_downsample_indices(first_row_idx, last_row_idx, downsample)
                            self.__cached(arr[out_row, :], table_idx, col_name, ds_first, ds_last, downsample, downsample_copy_mean, xform)
                        else:
                            downsample_copy_mean(xform(wsa[i, :]), arr[out_row, :], downsample)

        # Now that arr is populated, need to combine some rows for compound stats
        if any_multicol_stats:
            reduction = 0
            max_row = 0
            #compound_items  # (final output index, intermediate index, [real stats], stat_name in compound stats list)
            for final_i, intermediate_i, real_stats, compound_stat_name in compound_items:
                # real_stats = [ (x, stat, i)
                assert (final_i + 1 >= max_row)
                max_row = final_i + 1

                if real_stats is None:
                    arr[final_i + 1, :] = arr[intermediate_i + 1, :]
                    assert(final_i <= intermediate_i)
                else:
                    rows = []
                    for n, ((stat, xform), _compound_stat_name, i) in enumerate(real_stats):
                        rows.append(arr[intermediate_i + 1 + n])
                        assert (final_i <= intermediate_i + n)

                    assert (len(real_stats) >= 1)
                    reduction += len(real_stats) - 1
                    arr[final_i + 1, :] = assemble_compound_stat(compound_stat_name, self.data_source._compound_stats, *rows)

            if reduction > 0:
                assert (arr.shape[0] - reduction == max_row + 1), 'Should reduce to the max row + 1'
                arr = arr[:arr.shape[0] - reduction]

        return arr

    # Copy values from the dataset column `stat_col` to the `out` array while downsampling or retrieving already
    # downsampled data from the cache
    def __cached(self, out, table_idx, stat_col, ds_first, ds_last, downsample, method, xform):
        k = (stat_col, table_idx, xform, downsample)
        if k in self._downsample_cache:
            ##print('Using cached per-table value : ', stat_col, '', table_idx, 'ds=', downsample, ' ', ds_first, ds_last)
            v = self._downsample_cache[k]
            out[:] = v[ds_first: ds_last + 1]
            return

        # Cache the entire downsampled column
        ##print('Caching per-table value : ', stat_col, '', table_idx, 'ds=', downsample, ' ', ds_first, ds_last)

        # Transform weight update events into branch space.
        # TODO: This transform should take place at load-time for all stats so operations on them will be simpler and faster
        branch_space_values = np.zeros(shape=(self._num_events))
        coalesce_weight_update_stat_to_branches(self.pdf_wupdates.branch_index.values,
                                                self.pdf_wupdates.table.values,
                                                self.pdf_wupdates[stat_col].values,
                                                branch_space_values, 0, len(self.pdf_wupdates), table_idx)

        if downsample == 0:
            v = xform(branch_space_values)  # Use raw values
        else:
            # Calcualte downsampling for the full length of the dataset for stat_col
            v = np.zeros(shape=(math.ceil(len(self.pdf_brns) / downsample)))
            method(xform(branch_space_values), v, downsample)

        self._downsample_cache[k] = v

        # Slice out the part of this downsampled column that we care about
        out[:] = v[ds_first: ds_last + 1]


# Loop through weight rows by weight-update row index (wi), adding the value for the chosen column from that row to the
# appropriate row in the weight stat array (wsa)
@numba.jit(nopython=True, nogil=True, locals={'wi': numba.int64})
def coalesce_weight_update_stat_to_branches(arr_bi, arr_tbl, arr_col, dest, first_branch_index, num_wupdates, table_idx):
    wi = 0
    while wi < num_wupdates:
        tbl = arr_tbl[wi]
        if tbl == table_idx:
            bi = arr_bi[wi]
            v = arr_col[wi]  # get the value for this column based on index
            dest[bi - first_branch_index] += v  # add the value to the output array based on index
        wi += 1

# Copy data from tables into weight columns
@numba.jit(nopython=True, nogil=True)
def copy_weight_columns(num_wupdates, t2, num_columns, arbi, artbl, wupdates, num_tables):
    tbl = 0
    t = []
    while tbl < num_tables:
        t.append(t2[tbl * num_columns: tbl * num_columns + num_columns, :])
        tbl += 1
    wi = 0
    while wi < num_wupdates:
        bi = arbi[wi]
        tbl = artbl[wi]
        block = t[tbl]
        block[:, bi] += wupdates[wi]  # Should auto-transpose
        wi += 1
