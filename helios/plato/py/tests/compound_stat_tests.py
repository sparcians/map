from matplotlib import pyplot as plt
from os import path
import numpy as np
import time

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.stat_expression import *
from plato.backend.units import Units

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.processors.branch_training_heatmap.adapter import BranchTrainingHeatMapAdapter
from plato.backend.processors.branch_training_heatmap.generator import BranchTrainingHeatMapGenerator

from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.branch_training_line_plot.generator import BranchTrainingLinePlotGenerator

if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__),'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


# Test out the xform objects
NoTransform(np.array([1,2,3]))
AbsValueTransform(np.array([1,-2,3]))


def plot_heatmap(hm):
    plt.figure()
    plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')



# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
print('\nstats', branch_hm_data.stats)


print('HEATMAP TESTING')
bphma = BranchTrainingHeatMapAdapter(branch_hm_data)
print('\nnum events', bphma.num_events)

bin_size = 200000
bphmg = BranchTrainingHeatMapGenerator(bphma, ['table[{table}].d_weight_mag'], bin_size)

#hm, table_means, row_means = bphmg.generate_2d_heatmap_with_profiles(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'table[{table}].d_weight_mag')
#
## Run again for performancel
#hm, table_means, row_means = bphmg.generate_2d_heatmap_with_profiles(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'table[{table}].d_weight_mag')
#
#print('max {} at {}'.format(hm.max(), hm.argmax()))
#print('min {} at {}'.format(hm.min(), hm.argmin()))
#
#plot_heatmap(hm)
#plt.title('table[{table}].d_weight_mag')




print('LINEPLOT TESTING')
bphma = BranchTrainingTraceAdapter(branch_hm_data)
bplpg = BranchTrainingLinePlotGenerator(bphma)

a = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1', 'correct'], max_points_per_series=10000)
b = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['correct', 'table[0].thrash_1'], max_points_per_series=10000)

# Use a compound stat and make sure things are laid out correctly
c = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['branch_target_distance', 'correct', 'table[0].thrash_1'], max_points_per_series=10000)
assert (c.shape[0] == 4)

# Replace last data with useful data to plot
c = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['pc', 'tgt', 'branch_target_distance'], max_points_per_series=10000)

# Use a per-table compound stat
d = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].d_weight_mag', 'table[15].d_weight_mag', 'correct', 'table[0].thrash_1'], max_points_per_series=10000)

plt.figure()
assert (a.shape[0] == 3)
plt.plot(a[0, :], a[1, :], label='0.thrash1')
plt.plot(a[0, :], a[2, :], label='correct')
plt.legend()

plt.figure()
assert (b.shape[0] == 3)
plt.plot(b[0, :], b[1, :], label='correct')
plt.plot(b[0, :], b[2, :], label='0.thrash_1')
plt.legend()

assert (c.shape[0] == 4)
plt.figure()
plt.plot(c[0, :], c[1, :], label='pc')
plt.plot(c[0, :], c[2, :], label='tgt')
plt.legend()

plt.figure()
plt.plot(c[0, :], c[3, :], label='pc_tgt distance')
plt.legend()

assert (d.shape[0] == 5)
plt.figure()
plt.plot(d[0, :], d[1, :], label='0.d_weight_mag')
plt.plot(d[0, :], d[2, :], label='15.d_weight_mag')
plt.legend()

plt.figure()
plt.plot(d[0, :], d[3, :], label='correct')
plt.plot(d[0, :], d[4, :], label='0.thrash')
plt.legend()

#import pdb
#pdb.set_trace()

# Show all the images (blocking)
sys.stdout.flush()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
