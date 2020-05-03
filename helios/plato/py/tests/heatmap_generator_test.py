import logging
from matplotlib import pyplot as plt
from os import path
import numpy as np
import time

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.processors.branch_training_heatmap.adapter import BranchTrainingHeatMapAdapter
from plato.backend.processors.branch_training_heatmap.generator import *


if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__), 'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]

# Log everything debug
logging.basicConfig(level=logging.DEBUG)


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
print('\nstats', branch_hm_data.stats)

#######################################
# Testing for BranchTrainingHeatMapAdapter
#######################################

# Plot a single heatmap
def plot_heatmap(hm):
    plt.figure()
    plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')


bphma = BranchTrainingHeatMapAdapter(branch_hm_data)
sys.stdout.flush()
print('\nnum events', bphma.num_events)

print('\nchecking overall range')
sys.stdout.flush()
hm = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(0, len(branch_hm_data.ddf_branch_training_events), 'd_weight', hm)
print('\nhm sum', hm.sum())
sys.stdout.flush()

print('\nchecking first range')
sys.stdout.flush()
hm1 = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(0, len(branch_hm_data.ddf_branch_training_events) // 2 - 1, 'd_weight', hm1)
print('hm1 sum', hm1.sum())

print('\nchecking second range')
sys.stdout.flush()
hm2 = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(len(branch_hm_data.ddf_branch_training_events) // 2, len(branch_hm_data.ddf_branch_training_events) - 1, 'd_weight', hm2)
print('hm2 sum', hm2.sum())

print('hm1+2 sum', hm1.sum()+hm2.sum())
sys.stdout.flush()
assert(hm1.sum()+hm2.sum() == hm.sum()), '{} != {}'.format(hm1.sum()+hm2.sum(), hm.sum())

assert(np.allclose(hm1 + hm2, hm)) # Check equivalence

print(bphma.reshape_flat_heatmap(hm))
plot_heatmap(bphma.reshape_flat_heatmap(hm))



#######################################
# Testing BranchTrainingHeatMapGenerator
#######################################

# Note: for a very large trace, bin size needs to go up. This is just a test size
bin_size = 10000
if bphma.num_events > 1000000:
    bin_size = 200000

bphmg = BranchTrainingHeatMapGenerator(bphma, ['thrash_1','d_weight','new_weight'], bin_size)

assert(bphmg._check_bin_sum('d_weight') == hm.sum()), '{} != {}'.format(bphmg._check_bin_sum('d_weight'),hm.sum())

print('Branch training events:', len(branch_hm_data.ddf_branch_training_events))
print('Num bins', bphmg.num_bins)
print('Bin size', bin_size)

hm3 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
assert(hm3.sum() == hm.sum())
plot_heatmap(hm3)

hm4 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)//2 - 1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
hm5 = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)//2, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
assert(hm4.sum() + hm5.sum() == hm.sum())
assert(np.allclose((hm4 + hm5).flatten(), hm)) # Check equivalence

hm6 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)//3 - 1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
hm7 = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)//3, len(branch_hm_data.ddf_branch_training_events)*2//3-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
hm8 = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)*2//3, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight', allow_bins=False)
assert(hm6.sum() + hm7.sum() + hm8.sum() == hm.sum())
assert(np.allclose((hm6 + hm7 + hm8).flatten(), hm)) # Check equivalence


print('\nSlice with bins - all')
hm9 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight')
assert(hm9.sum() == hm.sum()), '{} != {}'.format(hm9.sum(), hm.sum())
plot_heatmap(hm9)

print('\nSlice with bins - all excluding first and last point')
hm10 = bphmg.generate_2d_heatmap(1, len(branch_hm_data.ddf_branch_training_events)-2, Units.BRANCH_TRAINING_INDEX, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(1, len(branch_hm_data.ddf_branch_training_events)-2, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip last bin')
hm10 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-2, Units.BRANCH_TRAINING_INDEX, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-2, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip first bin')
hm10 = bphmg.generate_2d_heatmap(1, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(1, len(branch_hm_data.ddf_branch_training_events)-1, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip last bin for real')
hm10 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-500, Units.BRANCH_TRAINING_INDEX, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-500, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - few points from first bin only')
hm10 = bphmg.generate_2d_heatmap(0, 1000, Units.BRANCH_TRAINING_INDEX, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range_to_heatmap(0, 1000, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

sys.stdout.flush()

t = time.time()
hm_timed = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight')
print('Elapsed: {} s',format(time.time() - t))

# Test a corner-case that used to fail
bphmg.generate_2d_heatmap(13037534, 24462853, Units.BRANCH_TRAINING_INDEX, 'thrash_1')

# Test conversion
print(bphma.num_events, 'events')
f64 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'thrash_1')
f32 = f64.astype(dtype='f4')
assert(np.allclose(f64, f32))

# Test plotting the profiles
hm11 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'thrash_1', allow_bins=False)
hm12,table_means,row_means = bphmg.generate_2d_heatmap_with_profiles(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'thrash_1')
assert(np.allclose(hm11,hm12))

plot_heatmap(hm12)
plt.title('thrash_1')

plt.figure()
plt.plot(np.arange(len(table_means)), table_means, label='table means')
plt.legend()

plt.figure()
plt.plot(np.arange(len(row_means)), row_means, label='row means')
plt.legend()


# Test access count
hm13 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'write', allow_bins=False)
plot_heatmap(hm13)
plt.title('writes')



# Test the first/last values against the baseline

fnb = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                                Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=False, mode=MODE_FIRST)
lnb = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                                Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=False, mode=MODE_LAST)

fb = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                               Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=True, mode=MODE_FIRST)
lb = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                               Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=True, mode=MODE_LAST)

assert(np.allclose(fb, fnb, equal_nan=True))
assert(np.allclose(lb, lnb, equal_nan=True))


# Test diff heatmap against the baseline for the first few events. This is proving that the binning optimization works

hm14 = bphmg.generate_2d_heatmap(0, 2, Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=False, mode=MODE_DIFF)
plot_heatmap(hm14)
plt.title('diff of first few new_weight (no bins)')

hm15 = bphmg.generate_2d_heatmap(0, 2, Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=True, mode=MODE_DIFF)
plot_heatmap(hm15)
plt.title('diff of first few new_weight (bins)')

assert(np.allclose(hm14, hm15, equal_nan=True))


# Test the diff-mode against the baseline for a few events at the end. This is proving that the binning optimization works

hm16 = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                                 Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=False, mode=MODE_DIFF)
plot_heatmap(hm16)
plt.title('diff of last few new_weight (no bins)')

hm17 = bphmg.generate_2d_heatmap(len(branch_hm_data.ddf_branch_training_events)-3, len(branch_hm_data.ddf_branch_training_events)-1,
                                 Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=True, mode=MODE_DIFF)
plot_heatmap(hm17)
plt.title('diff of last few new_weight (bins)')

assert(np.allclose(hm16, hm17, equal_nan=True))


# Test the diff-mode against the baseline for all the events. This is proving that the binning optimization works

hm18 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1,
                                 Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=False, mode=MODE_DIFF)
plot_heatmap(hm18)
plt.title('diff of ALL new_weight (no bins)')

hm19 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1,
                                 Units.BRANCH_TRAINING_INDEX, 'new_weight', allow_bins=True, mode=MODE_DIFF)
plot_heatmap(hm19)
plt.title('diff of ALL new_weight (bins)')

assert(np.allclose(hm18, hm19, equal_nan=True))


# Show all the images (blocking)
sys.stdout.flush()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
