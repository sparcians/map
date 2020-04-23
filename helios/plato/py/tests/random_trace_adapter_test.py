from matplotlib import pyplot as plt
import numpy as np
from os import path

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.units import Units
from plato.backend.datasources.random_trace.datasource import RandomTraceDataSource
from plato.backend.adapters.random_trace.adapter import RandomTraceAdapter
from plato.backend.processors.general_line_plot.generator import GeneralTraceLinePlotGenerator

# Load source data
ds = RandomTraceDataSource()

print(ds.pdf())

# Constructor adapter + generator
ad = RandomTraceAdapter(ds)
gen = GeneralTraceLinePlotGenerator(ad)

table_cols = ['trn_idx', 'stat']

series1 = gen.generate_lines(0, ad.num_events - 1, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=10000)
series2 = gen.generate_lines(0, ad.num_events - 1, Units.BRANCH_TRAINING_INDEX,
                                    table_cols, max_points_per_series=2000)

plt.figure()
plt.title('random data (by trn_idx)')
plt.plot(series1[0, :], series1[2, :], label='data1')
plt.plot(series2[0, :], series2[2, :], label='data2')
plt.legend()


# Generate data by CYCLES even though cycles is not the primary key
series1 = gen.generate_lines(0, ad.num_events - 1, Units.CYCLES,
                                    table_cols, max_points_per_series=10000)
series2 = gen.generate_lines(0, ad.num_events - 1, Units.CYCLES,
                                    table_cols, max_points_per_series=2000)

plt.figure()
plt.title('random data (by cycles)')
plt.plot(series1[0, :], series1[2, :], label='data1')
plt.plot(series2[0, :], series2[2, :], label='data2')
plt.legend()

import os
plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))