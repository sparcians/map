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

print('All tables thrashing')
kwargs = {"stat_cols": ["table[1].thrash_1","table[2].thrash_1","table[3].thrash_1","table[4].thrash_1","table[5].thrash_1","table[6].thrash_1","table[7].thrash_1","table[8].thrash_1","table[9].thrash_1","table[10].thrash_1","table[11].thrash_1","table[12].thrash_1","table[13].thrash_1","table[14].thrash_1","table[15].thrash_1"],
          "max_points_per_series": 400,
          "units": "training-index"
          }
thrash_tests = bplpg.generate_lines(11947487, 32066000, **kwargs)

plt.figure()
plt.title('table thrashing')
plt.plot(thrash_tests[0, :], thrash_tests[1, :], label='table 0 thrash_1')
plt.plot(thrash_tests[0, :], thrash_tests[2, :], label='table 1 thrash_1')
plt.plot(thrash_tests[0, :], thrash_tests[3, :], label='table 2 thrash_1')
plt.plot(thrash_tests[0, :], thrash_tests[4, :], label='table 3 thrash_1')
plt.plot(thrash_tests[0, :], thrash_tests[5, :], label='table 4 thrash_1')

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))