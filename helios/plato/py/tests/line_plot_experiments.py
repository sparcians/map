from matplotlib import pyplot as plt
import numpy as np
from os import path

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units
from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.branch_training_line_plot.generator import BranchTrainingLinePlotGenerator

if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__),'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
print('List of stats are: ', branch_hm_data.stats)
print('List of stats (static) are: ', BranchTrainingDatasource.get_source_information(filename)['stats'])

# Constructor adapter + generator
bphma = BranchTrainingTraceAdapter(branch_hm_data)
bplpg = BranchTrainingLinePlotGenerator(bphma)

print('cycles')
tgt_range_1k = bplpg.generate_lines(20550,27778637, Units.CYCLES,
                                    ['correct'], max_points_per_series=5000)


print('instructions (only works when the trace has instructions as a column)')
try:
    tgt_range_1k = bplpg.generate_lines(4000, 9716220, Units.INSTRUCTIONS,
                                        ['correct'], max_points_per_series=5000)
except:
    print('\nFAILED INSTRUCTIONS TEST\n')
    pass



print('trn_idx')
tgt_range_1k = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                    ['trn_idx', 'yout'], max_points_per_series=1000)


# Make sure we can use some retrieve some fields that are internally used to link tables (prevent double-requests)
kwargs = {'stat_cols': ['yout', 'correct', 'table[0].table', 'table[1].table', 'table[0].thrash_1', 'table[1].thrash_1'],
          'max_points_per_series': 5000, 'units': 'training-index'}
bplpg.generate_lines(0, 100, **kwargs)


# Test that sampling looks like the real data at various levels
table_cols = ['table[0].thrash_1']
whole_range_2k = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=2000)

whole_range_50k = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=50000)


print('TGT 1k')
tgt_range_1k = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=1000)

print('TGT 2k')
tgt_range_2k = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=2000)

print('TGT 10k')
tgt_range_10k = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=2000)

print('TGT 50k')
tgt_range_50k = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=50000)

print('TGT 200k')
tgt_range_200k = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=200000)


plt.figure()
plt.title('corner case')
for i in range(1):
    plt.plot(whole_range_2k[0, :],  whole_range_2k[i + 1, :],  label='whole_range_2k')
    plt.plot(whole_range_50k[0, :], whole_range_50k[i + 1, :], label='whole_range_50k')

    plt.plot(tgt_range_1k[0, :],  tgt_range_1k[i + 1, :], label='tgt_range_2k')
    plt.plot(tgt_range_2k[0, :], tgt_range_2k[i + 1, :], label='tgt_range_2k')
    plt.plot(tgt_range_10k[0, :], tgt_range_10k[i + 1, :], label='tgt_range_2k')
    plt.plot(tgt_range_50k[0, :], tgt_range_50k[i + 1, :],  label='tgt_range_50k')
    plt.plot(tgt_range_200k[0, :], tgt_range_200k[i + 1, :], label='tgt_range_50k')

plt.legend()


# Two downsampled regions beside eachother. Note the overlap of 1 unit to ensure no gap
h1 = bplpg.generate_lines(0, 10000000, Units.BRANCH_TRAINING_INDEX,
                          ['table[0].thrash_1', 'yout'], max_points_per_series=50000)
h2 = bplpg.generate_lines(10000000, 20000000, Units.BRANCH_TRAINING_INDEX,
                          ['table[0].thrash_1', 'yout'], max_points_per_series=50000)

plt.figure()
plt.plot(h1[0, :], h1[1, :], label='h1')
plt.plot(h2[0, :], h2[1, :], label='h2')
plt.legend()

plt.figure()
plt.plot(h1[0, :], h1[2, :], label='yout h1')
plt.plot(h2[0, :], h2[2, :], label='yout h2')
plt.legend()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))