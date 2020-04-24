# Some common utilities for tools that process branch predictor training traces

import time
import numpy as np
import numba
import json

from plato.backend.common import *


# Render a branch class string from values in a shp branch trace.
def render_branch_class(dynamic_state, indirect, uncond):
    s = ''
    if indirect:
        s += 'i'
    else:
        s += 'd'

    if uncond:
        s += 'u'
    else:
        s += 'c'

    if dynamic_state == 0:
        s += ' '
    elif dynamic_state == 1:
        s += ' AT'
    elif dynamic_state == 2:
        s += ' ANT'
    elif dynamic_state == 3:
        s += ' TNT'  # Probably always TNT since this just just SHP-visible branches
    else:
        s += ' ??'

    return s


# Find the next mispredict row at or after `i` meeting the filter criteria (i.e. accept(...))
@numba.jit(nopython=True, nogil=True)
def find_next_mispredict(i, last_i, correct_col):
    while i <= last_i:

        if correct_col[i] == False:
            return i

        i += 1

    return last_i + 1


# Given a pandas dataframe and a filter object, populate a mask vector from the branches in row `first_row_idx` to
# `last_row_idx` inclusive. Mask values will be of 1 or 0 depending on whether they satisfy filt.apply(...).
# Returns the mask and the number of 1s in the mask.
def make_mask(first_row_idx, last_row_idx, pdf, filt):
    # Columns as numpy arrays
    pc_col = pdf.pc.values
    target_col = pdf.tgt.values
    indirect_col = pdf.indirect.values
    uncond_col = pdf.uncond.values

    filt_mask = np.empty(shape=(last_row_idx - first_row_idx + 1), dtype=np.bool)

    npoints = filt.make_mask(first_row_idx, last_row_idx, pc_col, target_col, indirect_col, uncond_col, filt_mask)

    return filt_mask, npoints


# Expand the dict structure describing a filter. This must be coordinated with the client-side
def make_branch_filter(cfg):
    if cfg == {}:
        return None

    no_filtering = True
    no_addrs = True

    # Defaults
    incl_addrs = set()  # {addr, ...}
    excl_addrs = set()
    incl_masks = []  # [(addr, mask), ...]
    excl_masks = []
    incl_target_addrs = set()  # {addr, ...}
    excl_target_addrs = set()
    incl_target_masks = []  # [(addr, mask), ...]
    excl_target_masks = []
    accept_conditional = True
    accept_unconditional = True
    accept_direct = True
    accept_indirect = True
    incl_rows = set()
    incl_tables = set()
    incl_banks = set()

    if 'shp_table' in cfg:
        coords = cfg['shp_table']
        if coords is not None:
            if 'row' in coords:
                if coords['row'] is None:
                    pass
                else:
                    no_filtering = False
                    if type(coords['row']) == list:
                        for item in coords['row']:
                            incl_rows.add(item)
                    else:
                        incl_rows.add(coords['row'])
            if 'table' in coords:
                if coords['table'] is None:
                    pass
                else:
                    no_filtering = False
                    if type(coords['table']) == list:
                        for item in coords['table']:
                            incl_tables.add(item)
                    else:
                        incl_tables.add(coords['table'])
            if 'bank' in coords:
                if coords['bank'] is None:
                    pass
                else:
                    no_filtering = False
                    if type(coords['bank']) == list:
                        for item in coords['bank']:
                            incl_banks.add(item)
                    else:
                        incl_banks.add(coords['bank'])

    if 'classes' in cfg:
        classes = cfg['classes']
        if 'conditionality' in classes:
            no_filtering = False
            if classes['conditionality'] == 'conditional':
                accept_unconditional = False
            elif classes['conditionality'] == 'unconditional':
                accept_conditional = False

        if 'directness' in classes:
            no_filtering = False
            if classes['directness'] == 'direct':
                accept_indirect = False
            elif classes['directness'] == 'indirect':
                accept_direct = False

    if 'addresses' in cfg:
        for entry in cfg['addresses']:
            no_filtering = False
            if 'enabled' not in entry or entry['enabled'] is True:
                addr_loc = entry['address']
                no_addrs = False
                if addr_loc['type'] == 'Address':
                    address = int(addr_loc['addr'], 16)
                    if entry['include'] is True:
                        incl_addrs.add(address)
                    else:
                        excl_addrs.add(address)
                elif addr_loc['type'] == 'MaskedAddress':
                    address = int(addr_loc['addr'], 16)
                    mask = int(addr_loc['mask'], 16)
                    if entry['include'] is True:
                        incl_masks.append((address, mask))
                    else:
                        excl_masks.append((address, mask))
                else:
                    raise ValueError('Do not know how to support filter entry address type:' + str(addr_loc['type']))

    if 'targets' in cfg:
        for entry in cfg['targets']:
            no_filtering = False
            if 'enabled' not in entry or entry['enabled'] is True:
                addr_loc = entry['address']
                no_addrs = False
                if addr_loc['type'] == 'Address':
                    address = int(addr_loc['addr'], 16)
                    if entry['include'] is True:
                        incl_target_addrs.add(address)
                    else:
                        excl_target_addrs.add(address)
                elif addr_loc['type'] == 'MaskedAddress':
                    address = int(addr_loc['addr'], 16)
                    mask = int(addr_loc['mask'], 16)
                    if entry['include'] is True:
                        incl_target_masks.append((address, mask))
                    else:
                        excl_target_masks.append((address, mask))
                else:
                    raise ValueError('Do not know how to support filter entry address type:' + str(addr_loc['type']))

    if no_filtering:
        return None  # No filter needed

    # Convert things to numpy array to make numba happy because thats the only data structure it knows how to work with
    # quickly without nightmarish hacking.

    incl_addrs = np.array(list(incl_addrs), dtype=np.uint64)
    excl_addrs = np.array(list(excl_addrs), dtype=np.uint64)
    incl_masks = np.array(incl_masks, ndmin=2, dtype=np.uint64)
    excl_masks = np.array(excl_masks, ndmin=2, dtype=np.uint64)

    incl_target_addrs = np.array(list(incl_target_addrs), dtype=np.uint64)
    excl_target_addrs = np.array(list(excl_target_addrs), dtype=np.uint64)
    incl_target_masks = np.array(incl_target_masks, ndmin=2, dtype=np.uint64)
    excl_target_masks = np.array(excl_target_masks, ndmin=2, dtype=np.uint64)

    incl_rows = np.array(list(incl_rows), dtype=np.uint32)
    incl_tables = np.array(list(incl_tables), dtype=np.uint32)
    incl_banks = np.array(list(incl_banks), dtype=np.uint32)

    # Calculate the hash
    # This is expensive but its the only practical hash without implementing a running hash of all the fields by hand
    hash_code = hash(json.dumps(cfg, sort_keys=True))

    filt = BranchFilter(no_filtering, no_addrs,
                        incl_addrs, excl_addrs, incl_masks, excl_masks,
                        incl_target_addrs, excl_target_addrs, incl_target_masks, excl_target_masks,
                        accept_conditional, accept_unconditional, accept_direct, accept_indirect,
                        incl_banks, incl_tables, incl_rows)

    return BranchFilterWrapper(filt, hash_code)


# Filtering information from a dict, usually supplied by client.
# This class builds the information into an object that is easier and faster to examine when iterating items.
# IMPORTANT: BranchFilterWrapper.__eq__ must be updated to include any fields added here.
spec = [
    ('_no_filtering', numba.boolean),
    ('_no_addrs', numba.boolean),
    ('incl_addrs', numba.uint64[:]),
    ('excl_addrs', numba.uint64[:]),
    ('incl_masks', numba.uint64[:, :]),
    ('excl_masks', numba.uint64[:, :]),
    ('incl_target_addrs', numba.uint64[:]),
    ('excl_target_addrs', numba.uint64[:]),
    ('incl_target_masks', numba.uint64[:, :]),
    ('excl_target_masks', numba.uint64[:, :]),
    ('incl_banks', numba.uint32[:]),
    ('incl_tables', numba.uint32[:]),
    ('incl_rows', numba.uint32[:]),
    ('accept_conditional', numba.boolean),
    ('accept_unconditional', numba.boolean),
    ('accept_direct', numba.boolean),
    ('accept_indirect', numba.boolean),
    ('_any_incl', numba.boolean),
    ('_any_excl', numba.boolean),
]
@numba.jitclass(spec)
class BranchFilter:

    def __init__(self, no_filtering, no_addrs,
                 incl_addrs, excl_addrs, incl_masks, excl_masks,
                 incl_target_addrs, excl_target_addrs, incl_target_masks, excl_target_masks,
                 accept_conditional, accept_unconditional, accept_direct, accept_indirect,
                 incl_banks, incl_tables, incl_rows):
        self._no_filtering = no_filtering
        self._no_addrs = no_addrs
        self.incl_addrs = incl_addrs
        self.excl_addrs = excl_addrs
        self.incl_masks = incl_masks
        self.excl_masks = excl_masks
        self.incl_target_addrs = incl_target_addrs
        self.excl_target_addrs = excl_target_addrs
        self.incl_target_masks = incl_target_masks
        self.excl_target_masks = excl_target_masks
        self.incl_banks = incl_banks
        self.incl_tables = incl_tables
        self.incl_rows = incl_rows
        self.accept_conditional = accept_conditional
        self.accept_unconditional = accept_unconditional
        self.accept_direct = accept_direct
        self.accept_indirect = accept_indirect

        self._any_incl = len(self.incl_addrs) > 0 or len(self.incl_target_addrs) > 0 or len(self.incl_masks) > 0 or len(self.incl_target_masks) > 0
        self._any_excl = len(self.excl_addrs) > 0 or len(self.excl_target_addrs) > 0 or len(self.excl_masks) > 0 or len(self.excl_target_masks) > 0

    # Find the next row at or after `i` meeting the filter criteria (i.e. accept(...))
    def _find_next_row(self, i, last_i, correct_col, pc_col, target_col, indirect_col, uncond_col,
                       wupdate_col,
                       brn_col, bank_col, table_col, row_col,
                       only_mispredicts=False):

        # Coerce types for input to get_geo_cols
        i = np.uint64(i)
        last_i = np.uint64(last_i)

        while i <= last_i:

            # If only showing mispredicts, skip this
            if only_mispredicts and correct_col[i] == True:
                i += np.uint64(1)
                continue

            idx = int(i)

            pc = pc_col[idx]
            target = target_col[idx]
            indirect = indirect_col[idx]
            uncond = uncond_col[idx]

            (banks, tables, rows) = _get_geo_cols(i, wupdate_col, brn_col, bank_col, table_col, row_col)

            # Filter on pc/class if appropriate
            if _accept(self, pc, target, indirect, uncond, banks, tables, rows):
                return i

            i += np.uint64(1)

        return last_i + 1

    # Determine if the given branch information passes the filter
    def _accept(self, pc, target, indirect, uncond, banks, tables, rows):
        return _accept(self, pc, target, indirect, uncond, banks, tables, rows)


# Wraps a branch filter object in a python-friendly object with __hash__ and __eq__ which numba jitclass doesn't support
class BranchFilterWrapper:
    def __init__(self, filt, hash_code):
        self.filt = filt
        self.hash_code = hash_code

    # Equality is important for comparisons
    # This MUST Be updated whenever adding new functional fields to the filter
    def __eq__(self, o):
        if self.filt._no_filtering != o.filt._no_filtering:
            return False
        if self.filt._no_addrs != o.filt._no_addrs:
            return False
        if not np.all(self.filt.incl_addrs == o.filt.incl_addrs):
            return False
        if not np.all(self.filt.excl_addrs == o.filt.excl_addrs):
            return False
        if not np.all(self.filt.incl_masks == o.filt.incl_masks):
            return False
        if not np.all(self.filt.excl_masks == o.filt.excl_masks):
            return False
        if not np.all(self.filt.incl_banks == o.filt.incl_banks):
            return False
        if not np.all(self.filt.incl_tables == o.filt.incl_tables):
            return False
        if not np.all(self.filt.incl_rows == o.filt.incl_rows):
            return False
        if self.filt.accept_conditional != o.filt.accept_conditional:
            return False
        if self.filt.accept_unconditional != o.filt.accept_unconditional:
            return False
        if self.filt.accept_direct != o.filt.accept_direct:
            return False
        if self.filt.accept_indirect != o.filt.accept_indirect:
            return False
        if self.filt._any_incl != o.filt._any_incl:
            return False
        if self.filt._any_excl != o.filt._any_excl:
            return False

        return True

    def __hash__(self):
        return self.hash_code

    # Find the next row meeting the filter starting at first_row_idx and ending at last_row_idx (if not found earlier)
    def find_next_row(self, first_row_idx, last_row_idx, pdf_brns, pdf_wupdates, only_mispredicts=False):
        correct_col = pdf_brns.correct.values
        pc_col = pdf_brns.pc.values
        target_col = pdf_brns.tgt.values
        indirect_col = pdf_brns.indirect.values
        uncond_col = pdf_brns.uncond.values

        wupdate_col = pdf_brns.latest_weight_update_index.values

        brn_col = pdf_wupdates.branch_index.values
        bank_col = pdf_wupdates.bank.values
        table_col = pdf_wupdates.table.values
        row_col = pdf_wupdates.row.values

        i = self.filt._find_next_row(first_row_idx, last_row_idx, correct_col, pc_col, target_col, indirect_col, uncond_col,
                                     wupdate_col,
                                     brn_col, bank_col, table_col, row_col,
                                     only_mispredicts)

        return int(i)  # Cast back to int for Python-friendliness

    # Make a mask for the selected range of rows.
    #  first_row_idx first row index to include in mask (will be placed at index 0)
    #  last_row_idx  final row index to include in the mask
    #  pdf_brns      pandas dataframe of branches
    #  filt_mask_out filter mask to be populated. Must be at least last_row_idx-first_row_idx+1 items in length.
    def make_mask(self, first_row_idx, last_row_idx, pdf_brns, pdf_wupdates):
        pc_col = pdf_brns.pc.values
        target_col = pdf_brns.tgt.values
        indirect_col = pdf_brns.indirect.values
        uncond_col = pdf_brns.uncond.values

        wupdate_col = pdf_brns.latest_weight_update_index.values

        brn_col = pdf_wupdates.branch_index.values
        bank_col = pdf_wupdates.bank.values
        table_col = pdf_wupdates.table.values
        row_col = pdf_wupdates.row.values

        filt_mask = np.empty(shape=(last_row_idx - first_row_idx + 1), dtype=np.bool)

        npoints = _make_mask(self.filt, first_row_idx, last_row_idx, pc_col, target_col, indirect_col, uncond_col,
                             wupdate_col,
                             brn_col, bank_col, table_col, row_col,
                             filt_mask)

        return filt_mask, npoints

    # Test for acceptance.
    # Returns True if accepted.
    def accept(self, brn_idx, pdf_brns, pdf_wupdates):
        brn = pdf_brns.iloc[brn_idx]

        wupdate_col = pdf_brns.latest_weight_update_index.values

        brn_col = pdf_wupdates.branch_index.values
        bank_col = pdf_wupdates.bank.values
        table_col = pdf_wupdates.table.values
        row_col = pdf_wupdates.row.values

        (banks, tables, rows) = _get_geo_cols(brn_idx, wupdate_col, brn_col, bank_col, table_col, row_col)

        return _accept(self.filt, brn.pc, brn.tgt, brn.indirect, brn.uncond, banks, tables, rows)


# Accept or reject a branch based on the filter
# Note that this being a standalone function is noticeably faster than being a member of the BranchFilter class because
# of jitclass performance limitations.
@numba.jit(numba.boolean(BranchFilter.class_type.instance_type, numba.uint64, numba.uint64,
                         numba.boolean, numba.boolean, numba.int32[:], numba.int32[:], numba.int32[:]),
           nopython=True, nogil=True)
def _accept(self, pc, target, indirect, uncond, banks, tables, rows):
    if self._no_filtering:  # early out
        return True

    # Filtering on banks,tables,rows works by finding any one weight update that matches all the filter criteria.
    # i.e.: at least 1 single index across `banks`, `tables`, and `rows` columns must match all filter criteria.
    #
    # Note: assuming banks,rows,tables all the same length because they should be.
    j = 0
    while j < len(tables):
        if len(self.incl_banks) > 0:
            i = 0
            while i < len(self.incl_banks):
                if self.incl_banks[i] == banks[j]:
                    break  # Match
                i += 1
            else:
                j += 1
                continue

        if len(self.incl_tables) > 0:
            i = 0
            while i < len(self.incl_tables):
                if self.incl_tables[i] == tables[j]:
                    break  # Match
                i += 1
            else:
                j += 1
                continue

        if len(self.incl_rows) > 0:
            i = 0
            while i < len(self.incl_rows):
                if self.incl_rows[i] == rows[j]:
                    break  # Match
                i += 1
            else:
                j += 1
                continue

        # No issues with the filtering for this index j
        break

    else:
        # Failed to match
        return False

    if indirect:
        if not self.accept_indirect:
            return False
    else:
        if not self.accept_direct:
            return False

    if uncond:
        if not self.accept_unconditional:
            return False
    else:
        if not self.accept_conditional:
            return False

    if not self._no_addrs:
        # If inclusive addresses or masks set, must be among them
        if self._any_incl:
            # Require an address match or a masked match!
            matched = False

            if not matched:
                i = 0
                while i < len(self.incl_addrs):
                    if pc == self.incl_addrs[i]:
                        matched = True
                        break
                    i += 1

            if not matched:
                i = 0
                while i < len(self.incl_target_addrs):
                    if target == self.incl_target_addrs[i]:
                        matched = True
                        break
                    i += 1

            if not matched:
                if self.incl_masks.shape[1] > 0:
                    i = 0
                    while i < self.incl_masks.shape[0]:
                        addr = self.incl_masks[i, 0]
                        mask = self.incl_masks[i, 1]
                        if pc & mask == addr:
                            matched = True
                            break
                        i += 1

            if not matched:
                if self.incl_target_masks.shape[1] > 0:
                    i = 0
                    while i < self.incl_target_masks.shape[0]:
                        addr = self.incl_target_masks[i, 0]
                        mask = self.incl_target_masks[i, 1]
                        if target & mask == addr:
                            matched = True
                            break
                        i += 1

            if not matched:
                return False

            if self._any_excl:
                # Reject any address matches
                i = 0
                while i < len(self.excl_addrs):
                    if pc == self.excl_addrs[i]:
                        return False
                    i += 1

                i = 0
                while i < len(self.excl_target_addrs):
                    if target == self.excl_target_addrs[i]:
                        return False
                    i += 1

                # Reject any masked matches
                i = 0
                if self.excl_masks.shape[1] > 0:
                    while i < self.excl_masks.shape[0]:
                        addr = self.excl_masks[i, 0]
                        mask = self.excl_masks[i, 1]
                        if pc & mask == addr:
                            return False
                        i += 1

                i = 0
                if self.excl_target_masks.shape[1] > 0:
                    while i < self.excl_target_masks.shape[0]:
                        addr = self.excl_target_masks[i, 0]
                        mask = self.excl_target_masks[i, 1]
                        if target & mask == addr:
                            return False
                        i += 1

    return True


@numba.jit(nopython=True, nogil=True, fastmath=True)
#@numba.jit(numba.types.Tuple((numba.int32[:], numba.int32[:], numba.int32[:]))(
#           numba.uint64, numba.int32[:], numba.uint32[:], numba.int32[:], numba.int32[:], numba.int32[:]),
#           nopython=True, nogil=True, fastmath=True)
def _get_geo_cols(i, wupdate_col, brn_col, bank_col, table_col, row_col):
    # Look into the weight updates for coordinates
    j = wupdate_col[i]
    if j >= len(brn_col):
        banks = bank_col[0:0]  # nothing
        tables = table_col[0:0]  # nothing
        rows = row_col[0:0]  # nothing
    else:
        brn = brn_col[j]

        # Calcualate the end of the wight updates [j:k)
        if i == len(wupdate_col) - 1:
            k = len(bank_col)  # no more weight update events to, so end at the length of the bank column
        else:
            k = wupdate_col[i + 1]

        if brn == i:
            # Found weight table updates for this branch
            banks = bank_col[j:k]
            tables = table_col[j:k]
            rows = row_col[j:k]
        else:
            # This weight update (j) belongs to a different branch, so we have no coords
            banks = bank_col[0:0]  # nothing
            tables = table_col[0:0]  # nothing
            rows = row_col[0:0]  # nothing

    return banks, tables, rows


# Make a mask for the branches
# Note that this is pretty slow compared to other types of iteration (still sub-second timings though)
# Note that this is faster than being a member of the above
@numba.jit(numba.uint64(BranchFilter.class_type.instance_type,
                        numba.uint64, numba.uint64,
                        numba.uint64[:], numba.uint64[:], numba.int8[:], numba.int8[:],
                        numba.int32[:],
                        numba.uint32[:], numba.int32[:], numba.int32[:], numba.int32[:],
                        numba.boolean[:]),
           nopython=True, nogil=True, fastmath=True,
           locals={'npoints':numba.uint64, 'i':numba.uint64, 'on':numba.boolean},)
def _make_mask(self, first_row_idx, last_row_idx, pc_col, target_col, indirect_col, uncond_col,
               wupdate_col,
               brn_col, bank_col, table_col, row_col,
               filt_mask_out):
    # Count how many 1s are in the mask
    npoints = 0

    # TODO: for simple masks or address matches, we can construct a numpy filter [pc&mask==address] or [pc=address] to
    #       optimize things. This should be attempted because its probably faster than this numba stuff

    for i in numba.prange(first_row_idx, last_row_idx + 1):
        # Branch attributes
        pc = pc_col[i]
        target = target_col[i]
        indirect = bool(indirect_col[i])
        uncond = bool(uncond_col[i])

        (banks, tables, rows) = _get_geo_cols(i, wupdate_col, brn_col, bank_col, table_col, row_col)

        # Filter on pc/class if appropriate
        on = _accept(self, pc, target, indirect, uncond, banks, tables, rows)
        npoints += on
        filt_mask_out[i - first_row_idx] = on

    return npoints
