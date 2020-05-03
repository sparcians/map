from matplotlib import pyplot as plt
import numpy as np
from os import path
import time


import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.common import NRUCache
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
print('List of stats are: ', branch_hm_data.stats)
print('List of stats (static) are: ', BranchTrainingDatasource.get_source_information(filename)['stats'])

bphma = BranchTrainingHeatMapAdapter(branch_hm_data)

bin_size = 200000
bphmg = BranchTrainingHeatMapGenerator(bphma, ['table'], bin_size)

first=11666499
last=30323763
kwargs={"stat_col":"table","allow_bins":True,"allow_threads":False,"units":"training-index",
        "branch_predictor_filtering":{"addresses":[{"address":{"type":"Address","addr":"0x6370448c12"},"include":True,"enabled":True}],"classes":{}}}

first=101124724
last=119101931
kwargs = {"stat_col":"thrash_1","allow_bins":True,"allow_threads":False,"units":"cycles",
          "branch_predictor_filtering":{"addresses":[{"address":{"type":"Address","addr":"0x6370448c12"},"include":True,"enabled":True}],"classes":{}}}

vals = bphmg.generate_2d_heatmap_with_profiles(first, last, **kwargs)
