from matplotlib import pyplot as plt
from os import path

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
bphmg = BranchTrainingHeatMapGenerator(bphma, ['thrash_1', 'd_weight'], 300000)

hm = bphmg.generate_2d_heatmap(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX, 'd_weight')
print(hm)
print(hm.sum())

plt.figure()
plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))