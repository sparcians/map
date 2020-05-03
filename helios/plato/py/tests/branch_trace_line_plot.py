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



def test_gen():
    # Check that no stats results in a 1 row data set
    tmp = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events)-1, Units.BRANCH_TRAINING_INDEX,
                               [], max_points_per_series=0)
    assert (tmp.shape[0] == 1)


    # Check that intervals add up correctly
    a = bplpg.generate_lines(0, 99, Units.BRANCH_TRAINING_INDEX,
                             ['bias_at_training', 'correct', 'yout'], max_points_per_series=0)

    b = bplpg.generate_lines(100, 199, Units.BRANCH_TRAINING_INDEX,
                             ['bias_at_training', 'correct', 'yout'], max_points_per_series=0)

    c = bplpg.generate_lines(0, 199, Units.BRANCH_TRAINING_INDEX,
                             ['bias_at_training', 'correct', 'yout'], max_points_per_series=0)

    assert (np.allclose(np.hstack((a, b)), c))


def test_sampling():
    # Generate some traces to plot
    def gen(max_points=0):
        return bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                    ['bias_at_training', 'correct', 'table[0].thrash_1', 'yout'], max_points_per_series=max_points)


    #Next, hook the client up to the time-series for this stuff and eliminate the weight-update-table iteration because it is slow!

    # Invoke this twice just to let the jit warm up
    gen()
    values = gen()

    sampled_values = gen(1000)

    x = values[0, :]
    # Use: y = values[n, :]  # where n > 0


    if values.shape[1] < 1000000:
        plt.figure()
        plt.plot(x, values[1, :], label='bias_at_training')
        plt.plot(bphma.pdf_brns.trn_idx, bphma.pdf_brns.bias_at_training)
        plt.legend()

        plt.figure()
        plt.plot(x, values[2, :], label='correct')
        plt.plot(bphma.pdf_brns.trn_idx, bphma.pdf_brns.correct)
        plt.legend()

        plt.figure()
        plt.plot(x, values[3, :], label='table 0 thrash')
        plt.plot(sampled_values[0, :], sampled_values[3, :])
        plt.legend()

        plt.figure()
        plt.plot(x, values[4, :], label='yout')
        plt.plot(bphma.pdf_brns.trn_idx, bphma.pdf_brns.yout, label='yout-raw')
        plt.plot(sampled_values[0, :], sampled_values[4, :], label='yout-sampled')
        plt.legend()

def test_per_table():
    # Do some per-table figures
    table_cols = list(map(lambda n: 'table[{}].thrash_1'.format(n), range(bphma.num_tables)))
    values = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                        table_cols, max_points_per_series=0)

    sampled_values = bplpg.generate_lines(0, len(branch_hm_data.ddf_branch_training_events) - 1, Units.BRANCH_TRAINING_INDEX,
                                        table_cols, max_points_per_series=2000)



    print('Sampling corner-case')
    values = bplpg.generate_lines(6701695, 16461207, Units.BRANCH_TRAINING_INDEX,
                                        table_cols, max_points_per_series=10000)


    if values.shape[1] < 1000000:
        plt.figure()
        plt.title('corner case')
        ##for i,c in enumerate(table_cols):
        for i in range(1):
            plt.plot(values[0, :], values[i+1, :], label='thrash_1')
            plt.plot(sampled_values[0, :], sampled_values[i+1, :], label='sampled thrash_1')

        plt.legend()


# Run interactive and show plots
if __name__ == '__main__':
    test_gen()
    test_per_table()
    test_sampling()

    import os
    print(bool(int(os.environ.get('BLOCKING_SHOW', 1))))
    plt.show(block=bool(int(os.environ.get('BLOCKING_SHOW', 1))))
