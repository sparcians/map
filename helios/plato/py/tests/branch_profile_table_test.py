from matplotlib import pyplot as plt
from os import path
import numpy as np
import time

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.branch_training_profile.generator import BranchTrainingProfileHtmlGenerator


if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__), 'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
bptta = BranchTrainingTraceAdapter(branch_hm_data)
bptg = BranchTrainingProfileHtmlGenerator(bptta)

print('Run 0')
html = bptg.generate_table(10891253, 59415429, Units.CYCLES,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 1')
html = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 2')
html = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 3')
html = bptg.generate_table(12030, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], sort_col='nmispredicts', max_rows=17)
print('Run 4')
html = bptg.generate_table(12030, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], sort_col='nmispredicts', max_rows=17, geometry_filter={'table': 0, 'bank': 0})

print(html)
print(len(html))
