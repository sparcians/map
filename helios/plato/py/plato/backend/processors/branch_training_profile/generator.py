import numpy as np
import numba
import math
import sys
import time

from typing import Union
from io import StringIO

from plato.backend.common import *
from plato.backend.branch_common import *
from plato.backend.units import Units

from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter


# TODO: We should be able to run this in parallel over the dataset for large frames, then merge, then sort as normal
@numba.jit(nogil=True, nopython=True, fastmath=True, locals={'i': numba.int64})
def summarize(first_row,
              last_row,
              out_dtype,
              dtype_len,

              # input column arrays
              trn_idx_col,
              pc_col,
              ds_col,
              indirect_col,
              uncond_col,
              taken_col,
              correct_col,
              tgt_col,
              latest_wupdate_idx_col,
              wupdate_lookup_weight_col,
              wupdate_branch_idx_col,

              # output column indices
              out_pc_idx,
              out_count_idx,
              out_dynamic_state_idx,
              out_indirect_idx,
              out_uncond_idx,
              out_ntargets_idx,
              out_ntaken_idx,
              out_ncorrect_idx,
              out_nweight_correct_idx,
              out_stat_sum_idx,

              local_branch_mask,
              ):
    # Output arrays
    INITIAL_ROWS = 10000  # This has been tested with small values to force resizing
    pc_arr = np.zeros(shape=(INITIAL_ROWS,), dtype=np.uint64)  # Need a PC array to support uint64
    arr = np.zeros(shape=(INITIAL_ROWS, dtype_len),
                   dtype=np.int64)  # All values other than PC (typed as int64 because we need sign)
    rows_used = 0

    # Row dict: map of pc -> row within arr or pc_arr
    rd = dict()

    # Target-set dict: map of pc-> set(target)
    s = np.zeros(shape=32, dtype=np.uint64)
    tsd = {0: s}

    # Iterate data. This is almost the entirety of the time.
    i = first_row
    while i < last_row:
        if len(local_branch_mask) > 0 and local_branch_mask[i - first_row] == 0:
            i += 1
            continue

        pc = pc_col[i]
        if pc not in rd:
            # Handle a new branch

            # Expand the number of rows if needed
            if rows_used == pc_arr.shape[0]:
                a = np.zeros(shape=(int(rows_used * 2),), dtype=np.uint64)
                a[0:pc_arr.shape[0]] = pc_arr
                pc_arr = a

                b = np.zeros(shape=(int(rows_used * 2), dtype_len), dtype=np.int64)
                b[0:arr.shape[0], :] = arr
                arr = b

            indirect = indirect_col[i]

            # Create a set of targets for this new branch
            if indirect:
                tgt_set = np.zeros(shape=32, dtype=np.uint64)
            else:
                # Note that a branch may be discovered as an indirect later.
                # This array can still grow if more are discovered, but it is less wasteful to use 1
                tgt_set = np.zeros(shape=1, dtype=np.uint64)
            tsd[pc] = tgt_set

            # Grab a row for this new branch
            rd[pc] = rows_used
            pc_arr[rows_used] = pc
            row = arr[rows_used, :]
            rows_used += 1

            # Store static state in the branch once (though, this can technically change later)
            row[out_dynamic_state_idx] = ds_col[i]
            row[out_indirect_idx] = indirect
            row[out_uncond_idx] = uncond_col[i]
        else:
            row = arr[rd[pc]]
            tgt_set = tsd[pc]

        # Add new values to existing profile
        row[out_count_idx] += 1  # increment count
        row[out_ntaken_idx] += taken_col[i]
        row[out_ncorrect_idx] += correct_col[i]

        # Scan weights to determine whether the weight sum was correct
        weight_sum = 0
        wupdate_idx = latest_wupdate_idx_col[i]
        brn_idx = trn_idx_col[i]
        while wupdate_idx < len(wupdate_branch_idx_col):
            if wupdate_branch_idx_col[wupdate_idx] != brn_idx:
                break
            weight_sum += wupdate_lookup_weight_col[wupdate_idx]
            wupdate_idx += 1

        row[out_nweight_correct_idx] += (weight_sum >= 0) == correct_col[i]

        # TODO: Calculate the selected stat
        row[out_stat_sum_idx] += 0

        # Add target to "set". Since numba doesn't support sets properly (or uint64), use a np array.
        # Since most branches have relatively few targets if any, this should be quick.
        j = 0
        tgt = tgt_col[i]
        while j < tgt_set.shape[0]:
            if tgt_set[j] == tgt:
                break
            if tgt_set[j] == 0:
                tgt_set[j] = tgt
                break
            j += 1
        else:
            # Expand array and copy new value
            tsd[pc] = np.zeros(shape=len(tgt_set) * 2, dtype=np.uint64)
            tsd[pc][0:len(tgt_set)] = tgt_set
            tsd[pc][len(tgt_set)] = tgt

        i += 1

    return pc_arr, arr, rows_used, tsd


# Take the result of summarize and reshape/unpack/sort. This is very quick compared to iterating all data.
# Some things here cannot be done in numba (like converting array to tuple to assign to recarray)
def flatten(pc_arr, arr, rows_used, target_set_dict, sort_col, sort_descending, out_dtype):
    # Output array based on number of rows actually used
    final = np.zeros(shape=(rows_used), dtype=out_dtype)

    # Copy data from set structure into the array
    j = 0
    while j < rows_used:
        row = arr[j, :]
        final[j] = tuple(row)  # even though row is a np array, still must convert to tuple to assign
        final['pc'][j] = pc_arr[j]

        tgts = target_set_dict[pc_arr[j]]
        final['ntargets'][j] = len(tgts)

        j += 1

    # Fill this row post-compute
    final['nmispredicts'] = final['count'] - final['ncorrect']

    # In-place sort based on count
    sort_col = sort_col if sort_col is not None else 'count'
    sort_cols = [sort_col, 'count'] if sort_col != 'count' else [sort_col]
    final.sort(axis=0, order=sort_cols)

    if sort_descending:
        final = np.flip(final, axis=0)

    return final


# Warning: This is slow. It takes ~5 seconds to do 30 million branches which is below a reasonable 'interactive'
# threshold.
#
# Generates an html table containing a profile about branches
# TODO: This generator should pre-generate buckets for fast profiling of a big data-set.
#       This is particularly complicated for tracking number of unique targets though... as we'd have to store a map to
#       a set of unique targets in each bucket for each branch and merge them when profiling. This makes it difficult to
#       use pure numba.
class BranchTrainingProfileHtmlGenerator:

    # Internal table summary dtype
    dtype = np.dtype([
        ('pc', 'u8'),
        ('count', 'u8'),
        ('dynamic', 'i1'),
        ('indirect', 'b'),
        ('uncond', 'b'),
        ('ntargets', 'u4'),
        ('ntaken', 'u8'),
        ('ncorrect', 'u8'),
        ('nweight_correct', 'u8'),
        ('nmispredicts', 'u8'),
        ('stat_sum', 'f8'),
    ])

    # Get indices of output (make it easy to move around dtype without hardcoding anything except the dtype)
    pc_idx = get_dtype_idx(dtype, 'pc')
    count_idx = get_dtype_idx(dtype, 'count')
    dynamic_state_idx = get_dtype_idx(dtype, 'dynamic')
    indirect_idx = get_dtype_idx(dtype, 'indirect')
    uncond_idx = get_dtype_idx(dtype, 'uncond')
    ntargets_idx = get_dtype_idx(dtype, 'ntargets')
    ntaken_idx = get_dtype_idx(dtype, 'ntaken')
    ncorrect_idx = get_dtype_idx(dtype, 'ncorrect')
    nweight_correct_idx = get_dtype_idx(dtype, 'nweight_correct')
    stat_sum_idx = get_dtype_idx(dtype, 'stat_sum')

    def __init__(self, adapter: Union[BranchTrainingTraceAdapter]):
        self.adapter = adapter

    # See BranchTrainingTraceAdapter.get_points for explanation
    def generate_table(self, first_unit, last_unit, units, stat_cols, sort_col=None, sort_descending=True, max_rows=1000, geometry_filter=None):
        time_start = time.time()

        unit_stat = self.adapter.get_stat_for_units(units)
        first_row, last_row = self.adapter.lookup_rows_for_values(first_unit, last_unit, unit_stat)

        if first_row < 0 or last_row < first_row:
            raise ValueError('first/last not in range of data: {} {} '.format(first_row, last_row))

        brns = self.adapter.pdf_brns
        wupdates = self.adapter.pdf_wupdates

        branch_predictor_filtering = {'shp_table': geometry_filter}
        brnfilt = make_branch_filter(branch_predictor_filtering)

        if brnfilt is not None:
            # Make a branch mask on the selected range only
            # TODO: Cache this
            local_branch_mask, npoints = brnfilt.make_mask(first_row, last_row, brns, wupdates)
            print('Branch filter npoints=', npoints)
        else:
            local_branch_mask = np.empty(shape=0)

        # Invoke summarize with pre-extracted columns so that numba can operate on them efficiently and types are
        # retained. This is fine unless there are lots of columns
        items = summarize(first_row, last_row, self.dtype, len(self.dtype),
                          trn_idx_col=brns.trn_idx.values,
                          pc_col=brns.pc.values,
                          ds_col=brns.dynamic_state.values,
                          indirect_col=brns.indirect.values,
                          uncond_col=brns.uncond.values,
                          taken_col=brns.taken.values,
                          correct_col=brns.correct.values,
                          tgt_col=brns.tgt.values,
                          latest_wupdate_idx_col=brns.latest_weight_update_index.values,
                          wupdate_lookup_weight_col=wupdates.lookup_weight.values,
                          wupdate_branch_idx_col=wupdates.branch_index.values,
                          out_pc_idx=self.pc_idx,
                          out_count_idx=self.count_idx,
                          out_dynamic_state_idx=self.dynamic_state_idx,
                          out_indirect_idx=self.indirect_idx,
                          out_uncond_idx=self.uncond_idx,
                          out_ntargets_idx=self.ntargets_idx,
                          out_ntaken_idx=self.ntaken_idx,
                          out_ncorrect_idx=self.ncorrect_idx,
                          out_nweight_correct_idx=self.nweight_correct_idx,
                          out_stat_sum_idx=self.stat_sum_idx,
                          local_branch_mask=local_branch_mask,
                          )

        # Flatten the items above into their final shape and sort.
        # This is almost instant.
        pc_arr, arr, rows_used, target_set_dict = items
        items = flatten(pc_arr, arr, rows_used, target_set_dict, sort_col, sort_descending, self.dtype)

        # TODO: Should cache this last request so speed future sort/filter changes that don't effect range
        # TODO: Should implement partial updates where cached data is used and new data is added or old data is removed

        # Render the data to table rows. If keeping the rows < 100, this is quick enough.
        htmlio = StringIO()

        # Note that column IDs must match the numpy table column ids since they are used to control sorting.
        # columns without ids are not considered sortable.
        # Note that the client can also depend on some of these column IDs for stylizing the table.
        htmlio.write('''<table class="bp-profile-table" cellpadding=0 cellspacing=2><thead>
                            <th id="count" title="count - including decomposed virtual indirects">count</th>
                            <th id="pc">address</th>
                            <th>class</th>
                            <th id="ntargets" title="number of targets">targets</th>
                            <th>tkn%</th>
                            <th>correct%</th>
                            <th title="times weights themselves made a correct prediction">wghts correct%</th>
                            <th id="nmispredicts">mispreds</th>
                            <th>stat (not implemented)</th>
                        </thead><tbody>\n''')

        num_rows = 0
        for item in items:
            if num_rows >= max_rows:
                break

            count = item['count']
            htmlio.write('<tr>')
            htmlio.write('<td>{}</td>'.format(count))
            htmlio.write('<td>{}</td>'.format(hex(item['pc'])))
            htmlio.write('<td>{}</td>'.format(render_branch_class(item['dynamic'], item['indirect'], item['uncond'])))
            htmlio.write('<td>{}</td>'.format(item['ntargets']))
            htmlio.write('<td>{:0.02f}</td>'.format(100.0 * item['ntaken'] / count))  # taken %
            htmlio.write('<td>{:0.02f}</td>'.format(100.0 * item['ncorrect'] / count))  # overall correct %
            htmlio.write('<td>{:0.02f}</td>'.format(100.0 * item['nweight_correct'] / count))  # Weights correct %
            htmlio.write('<td>{}</td>'.format(int(item['nmispredicts']))) # Number of mispredictions
            htmlio.write('<td>{}</td>'.format('-'))  # Stat # TODO: should be item[self.stat_sum_idx]) but not yet implemented
            htmlio.write('</tr>')
            num_rows += 1

        htmlio.write('''</tbody></table>''')
        html = htmlio.getvalue()


        duration = time.time() - time_start
        print('computing table over up to {} rows for {} instances and {} stats took {} s' \
              .format(max_rows, last_row - first_row + 1, len(stat_cols), duration))

        return html

