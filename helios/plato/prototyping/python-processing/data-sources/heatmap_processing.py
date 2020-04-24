import dask
import dask.array as da
import dask.dataframe as dd
import dask.diagnostics
import h5py
import numpy as np
import pandas as pd
import math
import numba
import sys
import time
import pandas

import matplotlib.pyplot as plt

from multiprocessing.pool import ThreadPool

# Load the data
FILENAME = sys.argv[1]

class BranchTrainingDatasource:

    def __init__(self, filename):
        # Load using h5py (pandas/dask read_hdf5 does NOT support compression) and read everything into memory.
        # Ideally we could do this keeping things on disk, but that is slower right now. This can be added as an
        # optimization later

        hf = h5py.File(FILENAME, 'r') # Must not force-close. Dask retains a pointer

        # This is a loading process that does not require re-indexing.
        # Using dask dataframes from_array/records causes indexing trouble which requires a slow set_index to fix.
        # The downside of this is that everything needs to be realized in memory. This is fine for server-class
        # machines but doing this on a desktop with single-digit ram can make it unresponsive for big enough datasets
        np_a = hf['training_events'][()] # WARNING: Load into memory now
        pdf_a = pandas.DataFrame(np_a)
        self.ddf_branch_training_events = dd.from_pandas(pdf_a, chunksize=50000)

        np_b = hf['weight_updates'][()] # WARNING: Load into memory now
        pdf_b = pandas.DataFrame(np_b)
        self.ddf_weight_update_events = dd.from_pandas(pdf_b, chunksize=50000)


branch_hm_data = BranchTrainingDatasource(FILENAME)


# Sort by original index (SUPER SLOW)
#ddf_branch_training_events = ddf[0].set_index('trn_idx', sorted=True)
#ddf_weight_update_events = ddf[1].set_index('wup_idx', sorted=True)
#print ('Done with set_index')
#sys.stdout.flush()

print(branch_hm_data.ddf_branch_training_events)
print(branch_hm_data.ddf_weight_update_events)

# Plot weights for reference
index,y = dask.compute(branch_hm_data.ddf_weight_update_events.index, branch_hm_data.ddf_weight_update_events.d_weight.cumsum().compute())

# Plotly
#trace0 = go.Scattergl(x=index,y=y) 
#data = [trace0]
#plotly.offline.iplot(data)

plt.figure()
plt.plot(index,y)
#plt.show()



@numba.jit(nopython=True, nogil=True)
def NoTransform(value_in):
    return value_in

@numba.jit(nopython=True, nogil=True)
def AbsValueTransform(value_in):
    return abs(value_in)


# tables: numpy array with table values
# banks: numpy array with bank values
# rows: numpy array with row values
# stats: numpy array with the chosen stat value
# wupdate_count: number of rows to reach (should match array lengths)
# hm: flat 1d numpy array representing heatmap. Should be size to support the 'calc_location' function
@numba.jit(nopython=True, nogil=True)
def add_wupdates_to_heatmap(tables, rows, banks, stats, wupdate_count, calc_location, num_banks, num_rows, stat_xform, binref):
    for i in range(0, wupdate_count):
        location = calc_location(tables[i], banks[i], rows[i], num_banks, num_rows)
        
        # Logic for stat (accumulate absolute value or not)
        binref[location] += stat_xform(stats[i])

@numba.jit(nopython=True, nogil=True)
def calc_location(table, bank, row, num_banks, num_rows):
    return ((table * num_banks) + bank) * num_rows + row


# Produces data requested in terms of one table (e.g. branch index) but looks up data in another
# table (e.g. branch weight updates).
# This class is essentially stateless and can be shared by multiple consumers
class BranchTrainingHeatMapAdapter:

    def __init__(self, data_source: BranchTrainingHeatmapDatasource):
        self.ddf_brns = data_source.ddf_branch_training_events
        self.ddf_wupdates = data_source.ddf_weight_update_events

        # Calculate the heatmap dimensions
        # WARNING: work
        # TODO: store this in data tables when writing file
        table_min, table_max, row_min, row_max, bank_min, bank_max = \
            dask.compute(
                self.ddf_wupdates.table.min(), self.ddf_wupdates.table.max(),
                self.ddf_wupdates.row.min(), self.ddf_wupdates.row.max(),
                self.ddf_wupdates.bank.min(), self.ddf_wupdates.bank.max()
                )
    
        self.num_rows = 1 + row_max - row_min
        self.hm_width = self.num_rows
        self.num_banks = (1 + bank_max - bank_min)
        self.hm_height = self.num_banks * (1 + table_max - table_min)

        # Cache this for performance
        self._num_events = len(self.ddf_brns) # WARNING: may do work

    @property
    def num_events(self):
        return self._num_events

    @property
    def num_heatmap_elements(self):
        return self.hm_width * self.hm_height

    # Make a flat (1d) heatmap with appropriate dimensions
    def make_zero_flat_heatmap(self):
        return np.zeros(shape=(self.num_heatmap_elements,))

    # Reshape a flat heatmap into a 2d array (height,width)
    def reshape_flat_heatmap(self, flat_hm):
        return flat_hm.reshape(self.hm_height,self.hm_width)

    # Return the ranges of weight table update entries needed to satify apply_range on various (first,last) pairs.
    # Feed this output batch_get_weight_buckets
    def batch_pre_apply_range(self, pairs):
        query = []
        result_map = []
        for first_branch, last_branch in pairs:
            # Map branches to weights first_branch:last_branch
            query.append(first_branch)
            if last_branch >= self.num_events - 1: # last_branch is inclusive. If last_branch is last event, use weights through end of data
                first_weight = -len(query)
                last_weight = len(self.ddf_wupdates) - 1
            else:
                # end_weight should be just before the weight corresponding to branch after `last_branch`
                first_weight = -len(query)
                query.append(last_branch + 1)
                last_weight = -len(query)

            result_map.append([first_weight, last_weight])
        
        # Run computation in batch
        # Warning: This does work
        results = self.ddf_brns.latest_weight_update_index.loc[query].compute()
        for p in result_map:
            if p[0] < 0:
                idx = -p[0]-1
                p[0] = results.iloc[idx]
            if p[1] < 0:
                idx = -p[1]-1
                p[1] = results.iloc[idx] - 1 # make inclusive

        return result_map

    # TODO: This can be strung together with batch_pre_apply_range, though the computations probably can't be combined.
    # Pass each resulting frame to batch_apply_range as `wupdate_frame`
    # NOTE: Testing has shown that manually doing this computation in parallel has no net performance benefit.
    def batch_get_weight_buckets(self, wupdate_ranges):
        computations = []
        for first_weight,last_weight in wupdate_ranges:
            # TODO: skip empty ranges (e.g. last < first)
            computations.append(self.ddf_wupdates.loc[first_weight:last_weight])

        ## DEBUG: visualize work (this takes time and isn't really useful)
        #dask.visualize(computations, filename='test.svg')

        # Run computation in batch
        # Warning: This does work
        results = dask.compute(computations)
        return results[0] # Returns the list of values


    def batch_apply_range(self, first_weight, last_weight, wupdate_frame, stat_column_name, flat_hm, value_transform_func=NoTransform):
        # TODO: This vector slicing might need to be done before splitting work into threads

        if (first_weight > last_weight):
            return # Skip. no need to compute anything

        # Sum up weight rows using numba. This is stupid-fast.
        # First, we have to extract numpy arrays so numba knows how to iterate over them.
        # Then we can call the function to process.
        tables = wupdate_frame.loc[:,'table'].values
        rows = wupdate_frame.loc[:,'row'].values
        banks = wupdate_frame.loc[:,'bank'].values
        stats = wupdate_frame.loc[:,stat_column_name].values

        # Sanity-check size
        wupdate_count = 1 + last_weight - first_weight
        assert (len(wupdate_frame) == wupdate_count), \
            'weight slice was wrong: {} vs {}'.format(len(wupdate_bucket), wupdate_count)
        
        add_wupdates_to_heatmap(tables, rows, banks, stats, wupdate_count, calc_location, self.num_banks, self.num_rows, value_transform_func, flat_hm)


    
    # Take a range of branch training `events` by index and populate a flat (1d) heatmap
    def apply_range(self, first_branch, last_branch, stat_column_name, flat_hm, value_transform_func=NoTransform):

        assert(first_branch >= 0), first_branch
        assert(last_branch >= 0), last_branch

        # Map branches to weights first_branch:last_branch
        if last_branch >= self.num_events - 1: # last_branch is inclusive. If last_branch is last event, use weights through end of data
            first_weight = int(self.ddf_brns.latest_weight_update_index.loc[first_branch].compute())
            last_weight = len(self.ddf_wupdates) - 1
        else:
            # end_weight should be just before the weight corresponding to branch after `last_branch`
            first_weight, end_weight = self.ddf_brns.latest_weight_update_index.loc[[first_branch,last_branch+1]].compute()
            last_weight = end_weight - 1

        if (first_weight > last_weight):
            return # Skip. no need to compute anything

        # Weights in the relevant range
        wupdate_bucket_q = self.ddf_wupdates.loc[first_weight:last_weight]
       
        # Get this chunk of data into memory so we can operate on it with jitted python code
        # With jitted iteratoin code this may be a contributor to speed even though it is pretty fast
        wupdate_bucket = wupdate_bucket_q.compute() # WARNING: work
        
        # Sum up weight rows using numba. This is stupid-fast.
        # First, we have to extract things into numpy arrays so numba knows how to use them.
        # Then we can call the function to process
        tables = wupdate_bucket.loc[:,'table'].values
        rows = wupdate_bucket.loc[:,'row'].values
        banks = wupdate_bucket.loc[:,'bank'].values
        stats = wupdate_bucket.loc[:,stat_column_name].values

        # Sanity-check size
        wupdate_count = 1 + last_weight - first_weight
        assert (len(wupdate_bucket) == wupdate_count), \
            'weight slice was wrong: {} vs {}'.format(len(wupdate_bucket), wupdate_count)
        
        add_wupdates_to_heatmap(tables, rows, banks, stats, wupdate_count, calc_location, self.num_banks, self.num_rows, value_transform_func, flat_hm)



# Tests for BranchTrainingHeatMapAdapter
bphma = BranchTrainingHeatMapAdapter(branch_hm_data)
sys.stdout.flush()
print('\nnum events', bphma.num_events)

print('\nchecking overall range')
sys.stdout.flush()
hm = bphma.make_zero_flat_heatmap()
bphma.apply_range(0, len(ddf_branch_training_events), 'd_weight', hm)
print('\nhm sum', hm.sum())
sys.stdout.flush()

print('\nchecking first range')
sys.stdout.flush()
hm1 = bphma.make_zero_flat_heatmap()
bphma.apply_range(0, len(ddf_branch_training_events) // 2 - 1, 'd_weight', hm1)
print('hm1 sum', hm1.sum())

print('\nchecking second range')
sys.stdout.flush()
hm2 = bphma.make_zero_flat_heatmap()
bphma.apply_range(len(ddf_branch_training_events) // 2, len(ddf_branch_training_events) - 1, 'd_weight', hm2)
print('hm2 sum', hm2.sum())

print('hm1+2 sum', hm1.sum()+hm2.sum())
sys.stdout.flush()
assert(hm1.sum()+hm2.sum() == hm.sum()), '{} != {}'.format(hm1.sum()+hm2.sum(), hm.sum())

# Plot a single heatmap
def plot_heatmap(hm):
    plt.figure()
    plt.imshow(hm, cmap='hot', interpolation='nearest', aspect='auto')

    #trace0 = go.Heatmap(x=np.arange(0,hm_width),y=np.arange(0,hm_height), z=hm) # Need to realize the data to plot it!
    #data = [trace0]
    #plotly.offline.iplot(data)

print(bphma.reshape_flat_heatmap(hm))
plot_heatmap(bphma.reshape_flat_heatmap(hm))
#plt.show()



# Object that can generate a heatmap and performs some binning to optimize creation of on-demand heatmaps
# Uses an adapters to read raw data and populate heatmap. Adapter also provides transform from the
# event-dimension of this heatmap (e.g. branches) to some underlying dimension (e.g. weight updates) so
# that sampling can be done in that dimension.
class HeatMapGenerator:
    def __init__(self, adapter, stat_columns, bin_size=200000):
        self.adapter = adapter
        self.stat_columns = stat_columns
        self.bin_size = bin_size

        self.num_bins = math.ceil(adapter.num_events / self.bin_size)
        self.bins = {}

        # Divide into bins with even branch counts. Will look up the corresponiding weight-update event table row numbers
        # referenced by the branch training events table and store the information needed to generate a heatmap for each bin.
        apply_range_inputs = self.__compute_adapted_indices()

        for stat_col in stat_columns:
            print('Generating {} bins for {} to hold {} events'.format(self.num_bins, stat_col, self.adapter.num_events))
            sys.stdout.flush()
            self.bins[stat_col] = self.__generate_bins(apply_range_inputs, stat_col)

    def get_bins(self, stat_column_name):
        return self.bins[stat_column_name]

    def __compute_adapted_indices(self):
        t = time.time()

        # Start branch index of each bin
        bin_start_list = range(0, self.adapter.num_events, self.bin_size)
        assert(len(bin_start_list) == self.num_bins)

        # Figure out ranges of weight updates based on training event table
        # NOTE: This is the slow part. Actually counting within each bucket (later) is fast
        branch_ranges_incl = list(map(lambda r: (r, r+self.bin_size-1), bin_start_list))
        weight_ranges_incl = self.adapter.batch_pre_apply_range(branch_ranges_incl)

        duration = time.time() - t
        print('Generating {} table indices for bins took {} s'.format(self.num_bins, duration))
        sys.stdout.flush()


        t = time.time()

        weight_frames = self.adapter.batch_get_weight_buckets(weight_ranges_incl)
        
        # Construct num_bins tuples, each containing a heatmap range, the weight-data frame, and the bin index to
        # populate for that heatmap
        bin_indices = range(0, len(bin_start_list))
        apply_range_inputs = list(zip(weight_ranges_incl, weight_frames, bin_indices))

        # Log timing
        duration = time.time() - t
        print('Getting {} table weight buckets for bins took {} s'.format(self.num_bins, duration))
        sys.stdout.flush()

        return apply_range_inputs

    def __generate_bins(self, apply_range_inputs, stat_col):
        t = time.time()
        bins = np.zeros(shape=(self.num_bins,self.adapter.num_heatmap_elements))

        # make_bin using pre-computed data-frame
        def populate_bins(items):
            for item in items:
                # Unpack work
                bin_range = item[0]
                data_frame = item[1]
                bin_idx = item[2]

                # Weight update event indices
                first = bin_range[0]
                last = bin_range[1]

                # Fill the heatmap
                self.adapter.batch_apply_range(first, last, data_frame, stat_col, bins[bin_idx,:], value_transform_func=NoTransform)


        # NOTE: Multiprocessing doesn't work here - there is too much data to transfer. It is possible that a
        #       numpy memmap solution will be better and can run in a multiprocess environment. Someone should try that.
        if 1:
            # Partition the heatmap 'apply_range' inputs into thread work groups and use a pool to generate bins
            NUM_THREADS = 6
            def partition(l, chunk_size):
                for i in range(0, len(l), chunk_size):
                    yield l[i:i+chunk_size]
            partitioned_apply_range_inputs = list(partition(apply_range_inputs, math.ceil(len(apply_range_inputs) / NUM_THREADS)))
            
            assert(len(partitioned_apply_range_inputs) == NUM_THREADS), '{} != {}'.format(len(partitioned_apply_range_inputs),NUM_THREADS)

            pool = ThreadPool()
            pool.map(populate_bins, partitioned_apply_range_inputs)
            pool.close()
            pool.join()
        else:
            # Fallback non-threaded mode (debug only)
            populate_bins(apply_range_inputs)

        # Log timing
        duration = time.time() - t
        print('Generating {} bins for {} took {} s'.format(self.num_bins, stat_col, duration))
        sys.stdout.flush()
            
        return bins

    def _check_bin_sum(self, stat_col):
        bins = self.bins[stat_col]
        cs = 0
        for b in bins:
            cs += b.sum()
        return cs

    # 2d Heatmap over range: begin (incusive) to end (exclusive)
    # where these are indices of BRANCHES (from ddf_branches)
    # `allow_bins` lets the heatmap rely on the `bins` optimization. Disable this only for debugging
    def generate_2d_heatmap(self, first, last, stat_col, allow_bins=True):
        tStart = time.time()

        assert(first >= 0)
        assert(last >= first)
        assert(last < self.adapter.num_events)

        start_bin_idx = None
        end_bin_idx = None
      
        if allow_bins:
            for bin_idx in range(self.num_bins):
                if first <= bin_idx * self.bin_size:
                    # Need to compute from first to bin_idx*self.bin_size-1 using raw points
                    start_bin_idx = bin_idx
                    break

            #print ('Checking for last={} / {}'.format(last, self.adapters.num_events))
            for bin_idx in range(start_bin_idx, self.num_bins):
                if (bin_idx+1) * self.bin_size > last and last < self.adapter.num_events-1:
                    # Need to stop before this bin compute from bin_idx*self.bin_size to last using raw points
                    end_bin_idx = bin_idx
                    break
            else:
                end_bin_idx = self.num_bins

            if ((start_bin_idx is not None ) != (end_bin_idx is not None)):
                print('NOTE: one bin index is None but the other is not: {} vs {}'.format(start_bin_idx, end_bin_idx))

            #print('Bin indices=', start_bin_idx, end_bin_idx, ' of', self.num_bins)
            
        # Create the empty flat heatmap to make adding bins simple
        hm = self.adapter.make_zero_flat_heatmap()

        if start_bin_idx is not None and end_bin_idx is not None:
            # Get data from the bins
            bins = self.bins[stat_col]
            for bin_idx in range(start_bin_idx, end_bin_idx):
                #print('Adding bin {} (brn {} to {})'.format(bin_idx, bin_idx*self.bin_size, (bin_idx+1)*self.bin_size))
                hm += bins[bin_idx] # Vectorized add
                #print('  hm=', hm.sum())


            # Get raw data at the endpoints
            if start_bin_idx > 0:
                #print ('Adding before')
                self.adapter.apply_range(first, start_bin_idx * self.bin_size - 1, stat_col, hm)
                #print('  hm=', hm.sum())

            if end_bin_idx < self.num_bins:
                #print ('Adding after')
                self.adapter.apply_range(end_bin_idx * self.bin_size, last, stat_col, hm)
                #print('  hm=', hm.sum())
        else:
            self.adapter.apply_range(first, last, stat_col, hm)

        duration = time.time() - tStart
        print('computing 2d heatmap (allow_bins={}) over {} branches took {} s'.format(allow_bins, last-first+1, duration))
                
        # Convert flat heatmap to a matrix
        return self.adapter.reshape_flat_heatmap(hm)
        
        # TODO: Optimize this to only include changes at the endpoints from last update rather than re-iterating


#import cProfile
#cProfile.run("HeatMapGenerator(bphma, ['thrash_1','d_weight'], 10000)", sort='tottime')

# Test HeatMapGenerator
bphmg = HeatMapGenerator(bphma, ['thrash_1','d_weight'], 10000)
assert(bphmg._check_bin_sum('d_weight') == hm.sum()), '{} != {}'.format(bphmg._check_bin_sum('d_weight'),hm.sum())

print('Branch training events:', len(ddf_branch_training_events))
bphmg.num_bins

hm3 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)-1, 'd_weight', allow_bins=False)
assert(hm3.sum() == hm.sum())
plot_heatmap(hm3)

hm4 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)//2 - 1, 'd_weight', allow_bins=False)
hm5 = bphmg.generate_2d_heatmap(len(ddf_branch_training_events)//2, len(ddf_branch_training_events)-1, 'd_weight', allow_bins=False)
assert(hm4.sum() + hm5.sum() == hm.sum())

hm6 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)//3 - 1, 'd_weight', allow_bins=False)
hm7 = bphmg.generate_2d_heatmap(len(ddf_branch_training_events)//3, len(ddf_branch_training_events)*2//3-1, 'd_weight', allow_bins=False)
hm8 = bphmg.generate_2d_heatmap(len(ddf_branch_training_events)*2//3, len(ddf_branch_training_events)-1, 'd_weight', allow_bins=False)
assert(hm6.sum() + hm7.sum() + hm8.sum() == hm.sum())


print('\nSlice with bins - all')
hm9 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)-1, 'd_weight')
assert(hm9.sum() == hm.sum()), '{} != {}'.format(hm9.sum(), hm.sum())
plot_heatmap(hm9)

print('\nSlice with bins - all excluding first and last point')
hm10 = bphmg.generate_2d_heatmap(1, len(ddf_branch_training_events)-2, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range(1, len(ddf_branch_training_events)-2, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip last bin')
hm10 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)-2, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range(0, len(ddf_branch_training_events)-2, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip first bin')
hm10 = bphmg.generate_2d_heatmap(1, len(ddf_branch_training_events)-1, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range(1, len(ddf_branch_training_events)-1, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - skip last bin for real')
hm10 = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)-500, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range(0, len(ddf_branch_training_events)-500, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

print('\nSlice with bins - few points from first bin only')
hm10 = bphmg.generate_2d_heatmap(0, 1000, 'd_weight')
hm10ref = bphma.make_zero_flat_heatmap()
bphma.apply_range(0, 1000, 'd_weight', hm10ref)
assert(hm10.sum() == hm10ref.sum()), '{} != {}'.format(hm10.sum(), hm10ref.sum())

sys.stdout.flush()


t = time.time()
hm_timed = bphmg.generate_2d_heatmap(0, len(ddf_branch_training_events)-1, 'd_weight')
print('Elapsed: {} s',format(time.time() - t))



# Show all the images (blocking)
sys.stdout.flush()
plt.show()