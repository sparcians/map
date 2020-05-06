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

# Get extents:
cycles_range = []
for item in BranchTrainingDatasource.get_source_information(filename)['timeRange']:
    if item['units'] == Units.CYCLES:
        cycles_range = [item['first'], item['last']]


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
bphma = BranchTrainingHeatMapAdapter(branch_hm_data)

# Note: for a very large trace, bin size needs to go up. This is just a test size
bin_size = 10000
if bphma.num_events > 1000000:
    bin_size = 200000

bphmg = BranchTrainingHeatMapGenerator(bphma, ['thrash_1', 'd_weight'], bin_size)

print('Branch training events:', len(branch_hm_data.ddf_branch_training_events))
print('Num bins', bphmg.num_bins)
print('Bin size', bin_size)

# Check that whole range in branches matches the whole range in cycles (bins disabled)
hm1 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
hm2 = bphmg.generate_2d_heatmap(cycles_range[0], cycles_range[1], Units.CYCLES, 'd_weight', allow_bins=False)
assert(np.allclose(hm1, hm2))

# Check that whole range in branches matches the whole range in cycles (bins enabled)
hm3 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=True)
hm4 = bphmg.generate_2d_heatmap(cycles_range[0], cycles_range[1], Units.CYCLES, 'd_weight', allow_bins=True)
assert(np.allclose(hm3, hm4))
assert(np.allclose(hm1, hm4))

# Check that whole range in branches can be sliced in two and put back together
hm5 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=True)
midpoint = cycles_range[0] + (cycles_range[1] - cycles_range[0]) // 2
hm6 = bphmg.generate_2d_heatmap(cycles_range[0], midpoint, Units.CYCLES, 'd_weight', allow_bins=True)
hm7 = bphmg.generate_2d_heatmap(midpoint + 1, cycles_range[1], Units.CYCLES, 'd_weight', allow_bins=True)
assert(hm6.sum() != 0)  # Unlikely to be 0. Just making sure it did something
assert(hm7.sum() != 0)
assert(np.allclose(hm5, hm6 + hm7))

print('Success')