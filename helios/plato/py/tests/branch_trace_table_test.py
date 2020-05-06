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


print('Run 0')
html = bptg.generate_table(54866229, 59415429, Units.CYCLES,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 1')
html = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 2')
html = bptg.generate_table(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=100)
print('Run 3')
html = bptg.generate_table(12030, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], max_rows=17)


# Simple filter on one branch
branch_filter = {"addresses":
                     [{"address":
                           {"type": "Address",
                            "addr": "0x637044c01e"
                            },
                       "include": True
                       }
                      ],
                 "classes": {}
                 }

print('Run 4 (filter simple)')
html = bptg.generate_table(12030, len(branch_hm_data.ddf_branch_training_events)-100, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=17)


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

print('Run 5 (filter incl/excl mask/addr)')
html = bptg.generate_table(568256, len(branch_hm_data.ddf_branch_training_events)-100, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=10)


branch_filter = {"addresses":
                     [],
                  "classes": {
                      "directness": "indirect",
                      "conditionality": "unconditional"
                  }
                 }

print('Run 6 (classes)')
html = bptg.generate_table(568256, len(branch_hm_data.ddf_branch_training_events)-100, Units.BRANCH_TRAINING_INDEX,
                           stat_cols=['d_weight', 'thrash_1'], branch_predictor_filtering=branch_filter, max_rows=5)
print(html)
print(len(html))
