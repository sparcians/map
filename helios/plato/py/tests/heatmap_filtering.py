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


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
bphma = BranchTrainingHeatMapAdapter(branch_hm_data)

bin_size = 10000
if bphma.num_events > 1000000:
    bin_size = 200000

bphmg = BranchTrainingHeatMapGenerator(bphma, ['thrash_1','d_weight'], bin_size)

# Plot a single heatmap
def plot_heatmap(hm):
    plt.figure()
    plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')

    #trace0 = go.Heatmap(x=np.arange(0,hm_width),y=np.arange(0,hm_height), z=hm) # Need to realize the data to plot it!
    #data = [trace0]
    #plotly.offline.iplot(data)


# Run with NO filtering (allows bins)
h1 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1')

# 2nd (for timing)
bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1')


# Run with NO filtering (no bins)
h2 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1', allow_bins=False)

# 2nd (for timing)
bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1', allow_bins=False)


# Test branch filtering
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
                      ##"directness": "direct",
                      ##"conditionality": "unconditional"
                  }
                 }

h3 = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1', branch_predictor_filtering=branch_filter)

# 2nd run for better timings after jitting
bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events) - 1,
                          Units.BRANCH_TRAINING_INDEX, 'thrash_1', branch_predictor_filtering=branch_filter)

assert (np.allclose(h1, h2))
assert (not np.allclose(h1, h3))

plot_heatmap(h1)
plot_heatmap(h2)
plot_heatmap(h3)


# Show all the images (blocking)
sys.stdout.flush()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))