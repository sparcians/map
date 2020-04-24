from matplotlib import pyplot as plt
from os import path
import numpy as np
import time

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource
from plato.backend.adapters.branch_training_trace.adapter import BranchTrainingTraceAdapter
from plato.backend.processors.branch_training_table.generator import BranchTrainingListHtmlGenerator


if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__), 'test-branch-training-trace.hdf5')
else:
    filename = sys.argv[1]


# Load source data
branch_hm_data = BranchTrainingDatasource(filename)
bptta = BranchTrainingTraceAdapter(branch_hm_data)
bptg = BranchTrainingListHtmlGenerator(bptta)

branch_filter = {"addresses":
                     [],
                  "classes": {},
                 }

print('Run 1 (shp table cells: bank 0)')
html_bank0 = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=2, geometry_filter={"bank":0})
print(html_bank0)

print('Run 2 (shp table cells: bank 1)')
html_bank1 = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=2, geometry_filter={"bank":1})
print(html_bank1)

assert(html_bank0 == html_bank1), 'These should be the same because the each branch tends to access the same banks'



branch_filter = {"addresses":
                     [],
                  "classes": {},
                 }

print('Run 3 (shp table cells: bank 0, table 1)')
html_bank0 = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=2, geometry_filter={"bank":0, "table": 1})
print(html_bank0)

print('Run 4 (shp table cells: bank 1, table 1)')
html_bank1 = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=2, geometry_filter={"bank":1, "table": 1})
print(html_bank1)

assert(html_bank0 != html_bank1), 'These should be difference because the bank accessed by table 1 differs'


print('Run 4 (step 1 in error case)')
first = 13292644
last = first
kwargs = {'stat_cols': ['thrash_1'], 'max_rows': 1000, 'show_only_mispredicts': False,
          'units': 'training-index',
          'branch_predictor_filtering': {'addresses': [{'address': {'type': 'Address', 'addr': '0x2bfe18fd6a'}, 'include': True, 'enabled': True}],
                                         'targets': [],
                                         'classes': {}, }}
html = bptg.generate_table(first, last, **kwargs)
print(html)

print('Run 6 (step 2 in error case)')
first = 13292644
last = first
kwargs = {'stat_cols': ['thrash_1'], 'max_rows': 1000, 'show_only_mispredicts': False,
          'geometry_filter': {'row': None, 'table': 0, 'bank': 0},
          'units': 'training-index',
          'branch_predictor_filtering': {'addresses': [{'address': {'type': 'Address', 'addr': '0x2bfe18fd6a'}, 'include': True, 'enabled': True}],
                                         'targets': [],
                                         'classes': {}, }}
html = bptg.generate_table(first, last, **kwargs)
print(html)
