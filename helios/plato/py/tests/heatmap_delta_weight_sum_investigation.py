from matplotlib import pyplot as plt
from os import path
import numpy as np
import time

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.processors.branch_training_heatmap.adapter import BranchTrainingHeatMapAdapter
from plato.backend.processors.branch_training_heatmap.generator import BranchTrainingHeatMapGenerator


if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__),'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


def plot_heatmap(hm):
    plt.figure()
    plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')

# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
print('\nstats', branch_hm_data.stats)
bphma = BranchTrainingHeatMapAdapter(branch_hm_data)
print('\nnum events', bphma.num_events)

bin_size = 200000


bphmg = BranchTrainingHeatMapGenerator(bphma, ['thrash_1', 'd_weight'], bin_size)

hm, table_means, row_means = bphmg.generate_2d_heatmap_with_profiles(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight')

print('max {} at {}'.format(hm.max(), hm.argmax()))
print('min {} at {}'.format(hm.min(), hm.argmin()))

hm, table_means, row_means = bphmg.generate_2d_heatmap_with_profiles(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)

print('max {} at {}'.format(hm.max(), hm.argmax()))
print('min {} at {}'.format(hm.min(), hm.argmin()))

plot_heatmap(hm)
plt.title('d_weight')

# Show all the images (blocking)
sys.stdout.flush()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))