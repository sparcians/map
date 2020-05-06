# Reads a specific old version of branch traces needed for branch predictor analysis

import numpy as np
import pandas as pd
import h5py
import sys

import dask
import dask.array as da
import dask.dataframe as dd

# Load a hdf5 file to pandas data frames for branch training events and weight events
# (branch events, weight events)
# NOTE: does not support lzf compression. Fails on such files
def load_hdf5_to_pandas(filename):
    a = pd.read_hdf(filename, 'training_events')
    b = pd.read_hdf(filename, 'weight_updates')
    return (a,b)

# Load to dask dataframe. Note that this is lazy so its basically instant.
def load_hdf5_to_dask_df(filename):
    hf = h5py.File(filename, 'r') # Must not force-close. Dask retains a pointer

    print('Reading training events')
    sys.stdout.flush()
    x = hf["training_events"]

    # This is incredibly slow and expensive because it copies.... even using from_records
    print('Transforming training events')
    sys.stdout.flush()
    a = dd.from_array(x)
    #a = da.from_array(x, chunks=(100000,))

    print('Reading weight updates')
    sys.stdout.flush()
    x = hf['weight_updates']

    # This is incredibly slow and expensive because it copies.... even using from_records
    print('Transforming weight events')
    sys.stdout.flush()
    b = dd.from_array(x)
    #b = da.from_array(x, chunks=(100000,))

    return (a,b)


# Load a hdf5 file into pandas using h5py.
def load_hdf5_to_h5py(filename):
    with h5py.File(filename, 'r') as hf:
        print('Reading training events')
        sys.stdout.flush()
        x = hf["training_events"]

        # This is pretty slow
        print('Transforming training events')
        sys.stdout.flush()
        a = pd.DataFrame.from_records(x)

        print('Reading weight updates')
        sys.stdout.flush()
        x = hf['weight_updates']

        # This is pretty slow
        print('Transforming weight events')
        sys.stdout.flush()
        b = pd.DataFrame.from_records(x)

        return (a,b)


if __name__ == '__main__':
    import sys

    # Load a dask dataframe (ideally a big one) and play with some computations
    filename = sys.argv[1]
    branches, weight_events = load_hdf5_to_dask_df(filename)
    print(branches)
    print(weight_events)
    sys.stdout.flush()

    print('computing')
    sys.stdout.flush()

    ##branches = branches.compute() # Get the data?

    print('computed')
    sys.stdout.flush()

    print(branches[branches.my_first_weight_update_index != -1].loc[:,'my_first_weight_update_index'].head(20, npartitions=-1))

    # This defers the compute
    a = branches[branches.yout==1][:]['yout'].sum()
    print(a.compute())
    sys.stdout.flush()

    a = branches[branches.yout==1][:]['bias_at_training'].sum()
    print(a.compute())
    sys.stdout.flush()

    # This does the work
    ##ac = a.compute()
    ##print(ac)