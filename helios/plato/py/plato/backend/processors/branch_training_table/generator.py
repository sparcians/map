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


# Formatting helpers

# Note that jitting this slows down, even at 1000 rows
def taken_bool_to_char(tkn):
    if (tkn):
        return 'T'
    else:
        return 'N'


# This cannot be jitted
def weight_to_signed_string(v: int):
    if v < 0:
        return str(v)
    else:
        return '+' + str(v)


# Get a css hex color for a given SHP weight. Color is based on correctness of weight (green for correct,
# red for false) and interpolation between an unsaturated color for low-magnitude weights and a saturated
# color for high-magnitude weights. colorSaturationWeight controls the absolute value weight at which the
# color will be fully saturated.
# note: numba will not work here: @numba.jit(nopython=True, nogil=True)
def color_for_weight(correct, weight, color_saturation_weight):
    if correct:
        c = interp_color((240,255,240), (0,255,0), min(1,(abs(weight)/color_saturation_weight)))
        return color_to_hex(c)
    else:
        c = interp_color((255,240,240), (255,0,0), min(1,(abs(weight)/color_saturation_weight)))
        return color_to_hex(c)


# Take the branch data and move appropriately to stat/weight tables for each branch at index 'j'.
# In pure python these accesses would take about 90% of the total compute time.
@numba.jit(nopython=True, nogil=True, locals={'wsum':numba.int64, 'updates':numba.uint32})
def extract_weight_info(j, j_end, table_weights, table_stats, num_stats, wupdate_table, wupdate_lookup_weight,
                        wupdate_stats, wupdate_branch_indices, target_branch_index):
    wsum = 0
    updates = 0
    while j < j_end:
        # If we were pointed to a weight update referring to a branch that was not our target, stop.
        if wupdate_branch_indices[j] != target_branch_index:
            break

        w = wupdate_lookup_weight[j]
        wsum += w

        # Grab table weight for this branch
        table = wupdate_table[j]
        table_weights[table] = w

        # Grab stat vals for this branch
        c = 0
        while c < num_stats:
            table_stats[c, table] = wupdate_stats[j, c]
            c += 1

        j += 1
        updates += 1

    return wsum, updates


# Generates an html table containing information about a history of branches
class BranchTrainingListHtmlGenerator:
    WEIGHT_COLOR_SATURATION = 150

    def __init__(self, adapter: Union[BranchTrainingTraceAdapter]):
        self.adapter = adapter

    # See BranchTrainingTraceAdapter.get_points for explanation
    # geometry_filter lists specific row, column, and table (or None for any of these) that should be allowed during
    #                 filtering. None values for row/table/bank imply all items are allowed.
    def generate_table(self, first_unit, last_unit, units, stat_cols, max_rows=1000, branch_predictor_filtering={}, show_only_mispredicts=False, geometry_filter=None):
        time_start = time.time()

        unit_stat = self.adapter.get_stat_for_units(units)
        first_row, last_row = self.adapter.lookup_rows_for_values(first_unit, last_unit, unit_stat)

        if not (first_row >= 0):
            raise ValueError('first/last not in range of data')
        if not (last_row >= first_row):
            raise ValueError('first/last not in range of data')

        # Override some branch_predictor_filtering settings
        branch_predictor_filtering['shp_table'] = geometry_filter

        brnfilt = make_branch_filter(branch_predictor_filtering)

        brns = self.adapter.pdf_brns
        wupdates = self.adapter.pdf_wupdates

        weight_headers = '<th>0</th><th colspan={} style="text-align:center;">... weights ...</th><th>{}</th>' \
                         .format(self.adapter.num_tables - 2, self.adapter.num_tables - 1)

        stat_headers = ''
        for stat in stat_cols:
            stat_headers += '<th>0</th><th colspan={} style="text-align:center;">... stat: {} ...</th><th>{}</th>' \
                            .format(self.adapter.num_tables - 2, stat, self.adapter.num_tables - 1)

        # CSS styles here are stored in plato client
        htmlio = StringIO()

        # The id=pc column is significant for the client
        htmlio.write('''<table class="bp-list-table" cellpadding=0 cellspacing=0><thead>
<th>instr</th><th>index</th><th>cycle</th><th id="pc">address</th><th>class</th><th>target</th><th>tkn</th><th>pred</th><th>yout</th><th>bias</th>
<th>wsum</th>{weight_headers}{stat_headers}
</thead><tbody>\n'''.format(weight_headers=weight_headers, stat_headers=stat_headers))

        table_weights = np.empty(shape=(self.adapter.num_tables))
        table_stats = np.empty(shape=(len(stat_cols), self.adapter.num_tables,))

        # Extract tables into numpy for numbafication
        wupdate_table = wupdates.table.values
        wupdate_lookup_weight = wupdates.lookup_weight.values
        wupdate_stats = wupdates[stat_cols].values
        wupdate_rows = wupdates.row.values
        wupdate_banks = wupdates.bank.values
        wupdate_branch_indices = wupdates.branch_index.values

        wupdate_stat_extents = np.empty(shape=(2,len(stat_cols)))


        # Get min/max for the chosen stats across all values
        # TODO: This should ideally be done on stats after filtering, which could be done with a mask.
        # TODO: This whole function should probably use a branch filter mask and then just take the first n rows.
        k = 0
        while k < len(stat_cols):
            wupdate_stat_extents[0, k] = np.nanmin(wupdate_stats[:, k])
            wupdate_stat_extents[1, k] = np.nanmax(wupdate_stats[:, k])
            k += 1

        # Compute a color for a stat based on it value in the current range of values known here
        def color_for_stat(v, k):
            lower = wupdate_stat_extents[0, k]
            upper = wupdate_stat_extents[1, k]
            frac = float(v - lower) / (upper - lower)
            c = interp_color((150, 150, 255), (255, 100, 100), frac)
            return color_to_hex(c)

        # Iteration values
        i = first_row
        num_rows = 0
        last_brn_instructions = -1

        # Extract some columns into numpy for the jitted branch filter
        correct_col = brns.correct.values
        if brnfilt is not None:
            i = brnfilt.find_next_row(i, last_row, brns, wupdates, show_only_mispredicts)

        num_recently_filtered = 0  # branches filtered out manually rather than letting the jit code find branches

        while i <= last_row:
            if num_rows >= max_rows:
                break

            brn = brns.iloc[i]

            # Branch attributes
            pc = brn.pc
            indirect = brn.indirect
            target = brn.tgt
            uncond = brn.uncond
            correct = brn.correct

            # Filter on pc/class if appropriate
            if brnfilt is not None:
                if (show_only_mispredicts and bool(correct)) or not brnfilt.accept(i, brns, wupdates):
                    # Skip this branch
                    i += 1

                    # After a few filter misses, search again with the jitted code since it is faster (but switching can
                    # cost more if every branch is valid)
                    num_recently_filtered += 1
                    if num_recently_filtered > 5:
                        i = brnfilt.find_next_row(i, last_row, brns, wupdates, show_only_mispredicts)
                        num_recently_filtered = 0

                    continue
            elif show_only_mispredicts:
                if show_only_mispredicts and bool(correct):
                    # Skip to the next mispredict (numba required for performance since mispredicts are sometimes rare)
                    i = find_next_mispredict(i + 1, last_row, correct_col)
                    continue

            taken = brn.taken
            pred = (brn.taken == correct)
            bclass = render_branch_class(brn.dynamic_state, indirect, uncond)
            yout = brn.yout
            bias = brn.bias_at_lookup

            # Skip correct branches. This is done in numba via find_next_row above
            # TODO: Do this search in numba code as part of branch filter for performance
            if show_only_mispredicts and bool(correct):
                print('Got a branch we should not have!')
                i += 1
                continue

            weight_cells = StringIO()
            stat_cells = StringIO()

            table_weights.fill(0)
            table_stats.fill(0)

            w = int(brn.latest_weight_update_index)
            w_end = min(len(brns) - 1, int(brn.latest_weight_update_index) + self.adapter.num_tables)

            wsum, updates = extract_weight_info(w, w_end, table_weights, table_stats, len(stat_cols), wupdate_table, wupdate_lookup_weight, wupdate_stats, wupdate_branch_indices, brn.trn_idx)

            if updates > 0:
                # Render weights
                t = 0
                while t < self.adapter.num_tables:
                    weight = int(table_weights[t])
                    row = wupdate_rows[w + t]
                    bank = wupdate_banks[w + t]
                    c = color_for_weight((weight >= 0) == taken, weight, self.WEIGHT_COLOR_SATURATION)
                    edge_attributes = 'class="right-border"' if t == self.adapter.num_tables - 1 else ''
                    weight_cells.write('<td style="background-color:' + c + ';" table="' + str(t) +
                                       '" row="' + str(row) + '" + bank="' + str(bank) + '" ' +
                                       edge_attributes + ' >')
                    weight_cells.write(weight_to_signed_string(int(table_weights[t])))
                    weight_cells.write('</td>')
                    t += 1

                # Render stat vals
                k = 0
                while k < len(stat_cols):
                    t = 0
                    while t < self.adapter.num_tables:
                        s = int(table_stats[k, t])
                        c = color_for_stat(s, k)
                        row = wupdate_rows[w+t]
                        bank = wupdate_banks[w+t]
                        edge_attributes = 'class="right-border"' if t == self.adapter.num_tables - 1 else ''
                        weight_cells.write('<td style="background-color:' + c + ';" table="' + str(t) +
                                           '" row="' + str(row) + '" + bank="' + str(bank) + '" ' +
                                           edge_attributes + ' >')
                        weight_cells.write(weight_to_signed_string(s))
                        weight_cells.write('</td>')

                        t += 1
                    k += 1

                wsum_color = color_for_weight((wsum >= 0) == taken, wsum, self.WEIGHT_COLOR_SATURATION)
                wsum_str = weight_to_signed_string(int(wsum))
                weight_cells = weight_cells.getvalue()
                stat_cells = stat_cells.getvalue()
            else:
                wsum_color = '#ffffff'
                weight_cells = '<td colspan={}>no weights available</td>'.format(self.adapter.num_tables)
                stat_cells = '<td colspan={}>no stats available</td>'.format(self.adapter.num_tables * len(stat_cols))
                wsum_str = '?'

            if correct:
                pred_class = 'bp-list-table-correct'
            else:
                pred_class = 'bp-list-table-incorrect'

            yout_color = color_for_weight((yout >= 0) == taken, yout, self.WEIGHT_COLOR_SATURATION)
            bias_color = color_for_weight((bias >= 0) == taken, bias, self.WEIGHT_COLOR_SATURATION)

            if brn.instructions != last_brn_instructions:
                tr_class = 'class="bp-list-table-first-ubranch"'
                instr = int(brn.instructions)
            else:
                tr_class = ''
                instr = ''
            last_brn_instructions = brn.instructions

            htmlio.write('<tr {tr_class}><td>{instr}</td><td>{index}</td><td>{cycle}</td><td class="bp-list-table-addr">{pc}</td><td>{bclass}</td>'
                         .format(tr_class=tr_class,
                                 instr=instr,
                                 index=int(i),
                                 cycle=int(brn.cycle),
                                 pc=hex(int(brn.pc)),
                                 bclass=bclass))
            htmlio.write('<td class="bp-list-table-addr">{target}</td><td>{taken}</td><td class="{pred_class}">{pred}</td>'
                         .format(target=hex(int(brn.tgt)),
                                 taken=taken_bool_to_char(taken),
                                 pred=taken_bool_to_char(pred),
                                 pred_class=pred_class))
            htmlio.write('<td style="background-color: {yout_color};">{yout}</td>'
                         .format(yout=weight_to_signed_string(int(yout)),
                                 yout_color=yout_color))
            htmlio.write('<td style="background-color: {bias_color};">{bias}</td>'
                         .format(bias=weight_to_signed_string(int(bias)),
                                 bias_color=bias_color))
            htmlio.write('<td style="background-color: {wsum_color}; class="right-border">{wsum}</td>'
                         .format(wsum=wsum_str,
                                 wsum_color=wsum_color))
            htmlio.write(weight_cells)
            htmlio.write(stat_cells)

            htmlio.write('</tr>\n')
            num_rows += 1

            i += 1

        htmlio.write('</tbody></table>')

        html = htmlio.getvalue()

        duration = time.time() - time_start
        print('computing table over up to {} rows for {} stats took {} s' \
              .format(max_rows, len(stat_cols), duration))

        return html

