# Check that the heatmap coalescing algorithm works properly

import logging
from matplotlib import pyplot as plt
from os import path
import math
import numpy as np

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.stat_expression import NoTransform

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.processors.branch_training_heatmap.adapter import BranchTrainingHeatMapAdapter
from plato.backend.processors.branch_training_heatmap.generator import *


if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__), 'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]

# Log everything debug
logging.basicConfig(level=logging.DEBUG)


# Demonstrate the coalescing of heatmap values into lefward and rightward bins that can be used to more quickly
# calculate the first and last value for a given heatmap cell in a range of weight-update events, each containing
# a bank, table, and row coordinate as well as a value.

# Heatmap geometry
num_banks = 1
num_tables = 2
num_rows = 3


# Data
#
# Display (reshape + transpose):
#        tables->
#  rows  0 3
#     |  1 4
#     v  2 5
#
banks  = np.array([0, 0,  0, 0,  0, 0,  0, 0,  0])
tables = np.array([0, 0,  0, 1,  0, 0,  0, 1,  0])
rows   = np.array([0, 0,  0, 0,  1, 1,  2, 1,  1])
values = np.array([0, 1,  2, 3,  4, 5,  6, 7,  8])

num_wupdates = len(banks)
bin_size = 2
num_bins = math.ceil(num_wupdates / bin_size)

hm_size = num_banks * num_tables * num_rows


# RIGHT COALESCING
r_bins = [np.empty(hm_size), np.empty(hm_size), np.empty(hm_size), np.empty(hm_size), np.empty(hm_size)]
for bin in r_bins:
    bin.fill(np.nan)
xform = NoTransform

generate_coalesced_bins_right(r_bins, xform, num_bins, bin_size, num_wupdates, banks, tables, rows, values, num_banks, num_rows)

print('RIGHTWARD')
print(r_bins)

# Show bins (transpose to diagram in comment above
for bin in r_bins:
    print(bin.reshape(num_tables, num_rows).transpose())

assert(np.allclose([[1.,     np.nan],
                    [np.nan, np.nan],
                    [np.nan, np.nan]],
                    r_bins[0].reshape(num_tables, num_rows).transpose(), equal_nan=True))

assert(np.allclose([[2.,     3.],
                    [np.nan, np.nan],
                    [np.nan, np.nan]],
                    r_bins[1].reshape(num_tables, num_rows).transpose(), equal_nan=True))

assert(np.allclose([[np.nan, np.nan],
                    [5.,     np.nan],
                    [np.nan, np.nan]],
                    r_bins[2].reshape(num_tables, num_rows).transpose(), equal_nan=True))

assert(np.allclose([[np.nan, np.nan],
                    [np.nan, 7.],
                    [6.,     np.nan]],
                    r_bins[3].reshape(num_tables, num_rows).transpose(), equal_nan=True))

assert(np.allclose([[np.nan, np.nan],
                    [8.,     np.nan],
                    [np.nan, np.nan]],
                    r_bins[4].reshape(num_tables, num_rows).transpose(), equal_nan=True))


# Run some queries
#
# "LAST" values search [0,7] inclusive
# 1. read event 7 to hm
# 2. read event 6 to hm
# 3. event 5 crossed bin boundary
#     a. choose bin floor(5/2) => 2
#     b. fill hm nans with values from bins[2]
# DONE
#

hm = last_values(r_bins, 0, 7, xform, bin_size, banks, tables, rows, values, num_banks, num_rows, hm_size)
print('\nlast values: [0,7]')
print(hm.reshape(num_tables, num_rows).transpose())
assert(np.allclose([2., 5., 6.,
                    3., 7., np.nan,],
                    hm,
                    equal_nan=True))

hm = last_values(r_bins, 0, 8, xform, bin_size, banks, tables, rows, values, num_banks, num_rows, hm_size)
print('\nlast values: [0,8]')
print(hm.reshape(num_tables, num_rows).transpose())
assert(np.allclose([2., 8., 6.,
                    3., 7., np.nan,],
                    hm,
                    equal_nan=True))

# Also test values within a single bin only!
hm = last_values(r_bins, 6, 7, xform, bin_size, banks, tables, rows, values, num_banks, num_rows, hm_size)
print('\nlast values: [6,7]')
print(hm.reshape(num_tables, num_rows).transpose())
assert(np.allclose([np.nan, np.nan, 6.,
                    np.nan, 7.,     np.nan,],
                    hm,
                    equal_nan=True))


import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
