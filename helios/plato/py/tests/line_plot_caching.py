from matplotlib import pyplot as plt
import numpy as np
from os import path
import time


import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.common import NRUCache
from plato.backend.units import Units
from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.branch_training_line_plot.generator import BranchTrainingLinePlotGenerator

if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__),'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


NRUCache.EXPIRATION_DEFAULT_MS = 2000  # Shorten to 5s before creating any adapters

# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
print('List of stats are: ', branch_hm_data.stats)
print('List of stats (static) are: ', BranchTrainingDatasource.get_source_information(filename)['stats'])

# Constructor adapter + generator
ad = BranchTrainingTraceAdapter(branch_hm_data)
lpg = BranchTrainingLinePlotGenerator(ad)


print ('Testing 2 runs that were once broken due to mask & downsampling interacting badly')
kwargs = {'stat_cols': ["correct"], 'max_points_per_series': 5000, 'branch_predictor_filtering':{"addresses": [
                          {"address": {"type": "Address", "addr": "0x6370448c12"}, "include": True, "enabled": True}],
                                                    "classes": {}}}
_ = lpg.generate_lines(0, 44979982, Units.BRANCH_TRAINING_INDEX, **kwargs)
_ = lpg.generate_lines(0, 44979982, Units.BRANCH_TRAINING_INDEX, **kwargs)
_ = lpg.generate_lines(0, 44979982, Units.BRANCH_TRAINING_INDEX, **kwargs)




print('\nTesting Caching using training-index as the time stat')
stat_cols = ['correct']  # ['table[0].thrash_1', 'correct']
runx = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=20000)
run0 = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)
run1 = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)
run2 = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)

assert (np.allclose(run0, run1)), 'Expected first run (uncached) would be as fast as next'


# Sleep for cache expiration amount
time.sleep(6)

# Only refresh training index (previously used as the time stat)
print('\nThis should expire the "correct" stat col')
_ = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['trn_idx'], max_points_per_series=10000)

# Sleep for cache expiration amount
time.sleep(6)

print('\nThis should remove the "correct" stat col')
_ = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['trn_idx'], max_points_per_series=10000)


print('\nThis should be slow again because the "correct" we needed is out of the cache')
_ = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)


print('\nPiecing 2 downsampled plots together to make one')
piece1 = lpg.generate_lines(len(branch_hm_data.ddf_branch_training_events) / 4,
                       len(branch_hm_data.ddf_branch_training_events) / 4 * 2, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)
piece2 = lpg.generate_lines(len(branch_hm_data.ddf_branch_training_events) / 4 * 2,
                       len(branch_hm_data.ddf_branch_training_events) / 4 * 3, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=stat_cols, max_points_per_series=10000)

print('\nRunning downsampling using the per-table stats. THis should be slow then get fast after initial caching')
thrash1 = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1'], max_points_per_series=10000)
thrash2 = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1'], max_points_per_series=10000)
thrash3 = lpg.generate_lines(len(branch_hm_data.ddf_branch_training_events)/4, len(branch_hm_data.ddf_branch_training_events)/4*3, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1'], max_points_per_series=10000)

print('\nTesting no-downsampling. This should be slower to calculate and draw')
thrashALL = lpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1'], max_points_per_series=0)

print('1', run0.shape)
print('2', run1.shape)
print('3', run2.shape)

plt.figure()
plt.title('0,1,2 should be identical while detailed series is noisier')
plt.plot(runx[0, :], runx[1, :], label='detailed')
plt.plot(run0[0, :], run0[1, :], label='0')
plt.plot(run1[0, :], run1[1, :], label='1')
plt.plot(run2[0, :], run2[1, :], label='2')
plt.legend()

plt.figure()
plt.title('x-comparison - should be single diagonal line')
plt.plot(run0[0, :], run0[0, :], label='0')
plt.plot(run1[0, :], run1[0, :], label='1')
plt.plot(run2[0, :], run2[0, :], label='2')
plt.legend()

plt.figure()
plt.title('pieces - 1 and 2 should roughly overlay all, but will not match exactly. 1 and 2 should meet at a single point')
plt.plot(run0[0, :], run0[1, :], label='all')  # original, whole dataset
plt.plot(piece1[0, :], piece1[1, :], label='1')  # downsampled pieces
plt.plot(piece2[0, :], piece2[1, :], label='2')
plt.legend()

plt.figure()
plt.title('thrashing - 1&2 series should be identical. 3 should be in the middle and will NOT overlay perfectly')
plt.plot(thrash1[0, :], thrash1[1, :], label='1')  # downsampled thrash stat
plt.plot(thrash2[0, :], thrash2[1, :], label='2')
plt.plot(thrash3[0, :], thrash3[1, :], label='3')

# Do not plot this... it is super slow
#plt.plot(thrashALL[0, :], thrashALL[1, :], label='ALL')

plt.legend()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))