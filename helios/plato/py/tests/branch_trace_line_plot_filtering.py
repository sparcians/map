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


branch_filter = {"addresses":
                     [
                      {"address":
                           {"type": "Address",
                            "addr": "0x2bfe13ff2e"
                            },
                       "include": False,
                       },
                      {"address":
                           {"type": "MaskedAddress",
                            "addr": "0x2bfe13ff00",
                            "mask": "0xffffffffffffff00"
                            },
                       "include": True,
                       },
                      ],
                  "classes": {
                  }
                 }

print('Testing filtering')
sampled = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1', 'correct'], max_points_per_series=10000)

# 1st filtering run to init jit
_ = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1', 'correct'], branch_predictor_filtering=branch_filter, max_points_per_series=10000)

# 2nd run for accurate timings
filtered = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                               stat_cols=['table[0].thrash_1', 'correct'], branch_predictor_filtering=branch_filter, max_points_per_series=10000)


branch_filter = {"addresses":
                     [
                      {"address":
                           {"type": "MaskedAddress",
                            "addr": "0x2bfe13ff00",
                            "mask": "0xffffffffffffff00"
                            },
                       "include": True,
                       },
                      ],
                  "classes": {}
                 }

# Filter again with a different filter
filtered_2 = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                  stat_cols=['table[0].thrash_1', 'correct'], branch_predictor_filtering=branch_filter, max_points_per_series=10000)


# Check a display issue in the viewer
kwargs = {"stat_cols":["correct"],"max_points_per_series":5000,"units":"training-index",
          "branch_predictor_filtering":{
              "addresses":[{"address":{"type":"Address","addr":"0x6370448c12"},"include":True,"enabled":True}],"classes":{}}}
test_diag = bplpg.generate_lines(0, 33670724, **kwargs)

kwargs = {"stat_cols":["table[0].thrash_1"],"max_points_per_series":5000,"units":"training-index",
          "branch_predictor_filtering":{
              "addresses":[{"address":{"type":"Address","addr":"0x6370448c12"},"include":True,"enabled":True}],"classes":{}}}
test_diag_thrash = bplpg.generate_lines(0, 33670724, **kwargs)

kwargs = {"stat_cols":["correct"],"max_points_per_series":5000,"units":"training-index",
          "branch_predictor_filtering":{
              "addresses":[{"address":{"type":"Address","addr":"0x6370448c12"},"include":True,"enabled":True}],"classes":{}}}
test_diag2 = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, **kwargs)

print('sampled', sampled.shape)
print('filtered', filtered.shape)
print('filtered 2', filtered_2.shape)

plt.figure()
plt.title('corner case')
plt.plot(sampled[0, :], sampled[1, :], label='sampled thrash_1')
plt.plot(filtered[0, :], filtered[1, :], label='filtered thrash_1')
plt.plot(filtered_2[0, :], filtered_2[1, :], label='filtered 2 thrash_1')
plt.legend()

plt.figure()
plt.title('testing diagonal line issue. should be no diagonal line')
plt.plot(test_diag2[0, :], test_diag2[1, :], label='test all')
plt.plot(test_diag[0, :], test_diag[1, :], label='test some')
plt.plot(test_diag_thrash[0, :], test_diag_thrash[1, :], label='test some thrash')
plt.legend()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
