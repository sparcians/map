from matplotlib import pyplot as plt
import numpy as np
from os import path

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units
from plato.backend.datasources.sparta_statistics.datasource import SpartaDataSource
from plato.backend.adapters.sparta_statistics.adapter import SpartaStatisticsAdapter
from plato.backend.processors.sparta_statistics.generator import SpartaStatsGenerator

if len(sys.argv) == 1:
    filename = path.join(path.dirname(__file__), 'ea243e37-3e1c-4aaf-9b77-b2a4ec8b0e31.db')
else:
    filename = sys.argv[1]


# Load source data
ds = SpartaDataSource(filename)
print('List of stats (instance) are: ', ds.stats)
print('List of stats (static) are: ', SpartaDataSource.get_source_information(filename)['stats'])

# Constructor adapter + generator
ad = SpartaStatisticsAdapter(ds)
gen = SpartaStatsGenerator(ad)

stat_cols = ["top.core0.rob.ReorderBuffer_utilization_weighted_nonzero_avg","top.core0.lsu.tlb.tlb_hits","top.core0.alu0.total_insts_issued"]

d = gen.generate_lines(334535, 1740757, stat_cols=stat_cols, units=Units.CYCLES, max_points_per_series=0)
print(d.shape)
x = d[0, :]
a = d[1, :]
b = d[2, :]

plt.figure()
plt.plot(x, a, label='a')
plt.plot(x, b, label='b')
plt.legend()


# Fixed bug: This range was causing downsampling to stick a x=0 item at the end of the x array because of an off-by-one error
d = gen.generate_lines(334535, 1740757, stat_cols=stat_cols, units=Units.CYCLES, max_points_per_series=5000)
print(d.shape)
x = d[0, :]
a = d[1, :]
b = d[2, :]

plt.figure()
plt.plot(x, a, label='a')
plt.plot(x, b, label='b')
plt.legend()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
