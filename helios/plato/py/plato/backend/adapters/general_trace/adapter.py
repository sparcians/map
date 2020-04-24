import numpy as np
import numba
import math
import time

from plato.backend.units import Units
from plato.backend.common import NRUCache
from plato.backend.stat_expression import NoTransform

from plato.backend.datasources.general_trace.datasource import GeneralTraceDataSource


# Adapter for GeneralTraceDataSource. Assumes a pandas dataframe is available.
# Performs Units->stat mapping, and provides get_points to return downsampled time-series data
class GeneralTraceAdapter:

    def __init__(self, data_source: GeneralTraceDataSource):
        self.data_source = data_source

        # Common trace stuff
        self.pdf = data_source.pdf()

        # Cache this for performance
        self._num_events = len(self.pdf)

        self._stats = list(self.pdf.columns)

        # Cache of already downsampled data. This limit means that if we have this many line plots being shown at once
        # by a client, things will start to get slow.
        self._downsample_cache = NRUCache(64)

    # Common method counting the number of events in the the trace
    @property
    def num_events(self):
        return self._num_events

    # Determine the stat in this datasource associated with the selected unit type
    # Override as necessary. Columns might have different names in different datasets.
    # Can use this function as a fallback.
    def get_stat_for_units(self, units):
        raise ValueError('Units {} not supported by this adapter'.format(units))

    # Override this as necessary to do simpler lookups (e.g. if rows are known to be the index of this trace)
    def lookup_rows_for_values(self, first, last, unit_stat):
        # Must find start/end cycles manually. This search assumes sorted values and is pretty fast
        # TODO: Later we might replace this with an index
        first_row = np.searchsorted(self.pdf[unit_stat], first, side='left')
        last_row = np.searchsorted(self.pdf[unit_stat], last, side='left')
        if last_row == len(self.pdf) or self.pdf.iloc[last_row][unit_stat] > last:
            last_row -= 1  # Correct for the found row always being >= last

        return int(first_row), int(last_row)  # Remove these from numpy

    # Convert a range in a specific units to a first and last row index in the dataset.
    def range_to_rows(self, first, last, units):
        first = int(first)
        last = int(last)

        unit_stat = self.get_stat_for_units(units)

        first_row, last_row = self.lookup_rows_for_values(first, last, unit_stat)

        # Check bounds
        min_idx = 0
        max_idx = len(self.pdf) - 1
        if first_row > max_idx or last_row < min_idx:
            raise ValueError('Cannot make a request that is totally out of bounds: {}-{} vs bounds {}-{}'
                             .format(first, last, min_idx, max_idx))

        # Constrain bounds
        first_row = max(first_row, min_idx)
        last_row = min(last_row, max_idx)

        # Double check that the first/last row match back up reasonably
        assert (self.pdf.iloc[first_row][unit_stat] >= first),\
                '{} {} < {}'.format(unit_stat, self.pdf.iloc[first_row][unit_stat], first)
        assert (self.pdf.iloc[last_row][unit_stat] <= last),\
                '{} {} > {}'.format(unit_stat, self.pdf.iloc[last_row][unit_stat], last)

        return first_row, last_row, unit_stat

    # Calculate how much downsampling is needed to get under max_points_per_series
    def calc_downsampling(self, num_points, max_points_per_series):
        num_points = int(num_points)  # force to integer

        if max_points_per_series <= 0:
            return 0, num_points

        if max_points_per_series >= num_points:
            return 0, num_points

        # Need to downsample in this case
        # TODO: should pre-preform downsampling at factors of 10 or so in constructor and return those points.
        downsample = calc_downsample(num_points, max_points_per_series)
        num_points = math.ceil(num_points / downsample)
        return downsample, num_points

    # Align endpoints for downsampling that contains consistent values even when changing endpoints.
    # If downsampling began at an arbitrary row, shifting by 1 could yield drastically different results. This is a
    # problem when sliding data in a client plot.
    def align_for_downsampling(self, first_row_idx, last_row_idx, num_points, downsample):
        if downsample == 0:
            return first_row_idx, last_row_idx, num_points

        # Move start toward 0 until aligned
        while (first_row_idx % downsample) != 0 and first_row_idx > 0:
            first_row_idx -= 1

        while ((last_row_idx + 1) % downsample) != 0 and last_row_idx < self._num_events - 1:
            last_row_idx += 1

        num_points = math.ceil((last_row_idx - first_row_idx + 1) / downsample)

        return first_row_idx, last_row_idx, num_points

    # Calculate how much downsampling is needed to get under max_points_per_series while also aligning the endpoints
    # to the downsample value such overlapping parts of different downsampled ranges will look the same if the
    # downsample amount is the same.
    def calc_aligned_downsampling(self, first_row_idx, last_row_idx, max_points_per_series):
        downsample, num_points = self.calc_downsampling(last_row_idx - first_row_idx + 1, max_points_per_series)

        if downsample > 0:
            first_row_idx, last_row_idx, num_points = self.align_for_downsampling(first_row_idx, last_row_idx, num_points, downsample)

        return downsample, num_points, first_row_idx, last_row_idx

    # Implementation of get_points that actually copies data to an output matrix, performing downsampling if requested
    # TODO: WARNING: This must be made aware of units when downsampling (same with the BranchTrainingTraceAdapter)
    #       so that downsampling steps in terms of n units... not in terms of n row indices. The downsampling is not
    #       'broken' as it is, but it is possible to get 2 point at the same unit-time with different values.
    # If mask is set, the points copied must only be for indices where mask==1. The resulting `out` array should expect
    # rows sizes equal to the number of ones in the mask.
    def _copy_points(self, first_row_idx, last_row_idx, time_stat, stat_to_row_map, downsample, out: np.ndarray, mask: np.array = None, cache=True):
        values = self.pdf.iloc[first_row_idx:last_row_idx + 1]

        if type(stat_to_row_map) == dict:
            stat_to_row_items = stat_to_row_map.items()
        else:
            stat_to_row_items = stat_to_row_map

        # Copy branch training event values into correct rows
        if downsample == 0:
            if mask is not None:
                out[0, :] = values[time_stat][mask == 1]  # Copy units into correct row

                # Copy each row into the final row
                for col, (i, xform) in stat_to_row_items:
                    out[i, :] = xform(values[col][mask == 1].values)

            else:
                out[0, :] = values[time_stat]  # Copy units into correct row

                # Copy each row into the final row
                for col, (i, xform) in stat_to_row_items:
                    out[i, :] = xform(values[col].values)

        else:
            if mask is not None:
                # TODO: Support caching this result. This will require the full mask to be passed down into this
                #       function, not just the relevant range though.

                # TODO: Determine if this separate masking step is faster than integrating the masking into the jitted function
                downsample_copy_first(values[time_stat][mask == 1].values, out[0, :], downsample)  # Copy units into the correct row

                # Copy each row unto the final row performing downsampling
                for col, (i, xform) in stat_to_row_items:
                    downsample_copy_mean(xform(values[col][mask == 1].values), out[i, :], downsample)

            else:
                ds_first, ds_last = self.convert_to_downsample_indices(first_row_idx, last_row_idx, downsample)

                # Copy the time row to the final array while performing downsamping or use cache
                self.__cached(out[0, :], values, time_stat, ds_first, ds_last, downsample, downsample_copy_first, NoTransform, cache)

                # Copy each row unto the final array while performing downsampling or use cache
                for col, (i, xform) in stat_to_row_items:
                    self.__cached(out[i, :], values, col, ds_first, ds_last, downsample, downsample_copy_mean, xform, cache)

    # Given real indices and a level of downsampling, convert to indices in the downsampled array assuming the entire
    # raw dataset is downsampled. If first_row_idx=0, the resulting first index will be 0.
    def convert_to_downsample_indices(self, first_row_idx, last_row_idx, downsample):
        if downsample == 0:
            downsample = 1
        ds_first = first_row_idx / downsample
        if math.floor(ds_first) != ds_first:
            raise ValueError(
                'first_row_idx {} was not a multiple of downsample. This should not happen here' \
                .format(first_row_idx))
        ds_first = math.floor(ds_first)
        ds_last = math.floor(last_row_idx / downsample)

        return ds_first, ds_last

    # Copy values from the dataset column `stat_col` to the `out` array while downsampling or retrieving already
    # downsampled data from the cache
    def __cached(self, out, values, stat_col, ds_first, ds_last, downsample, method, xform, cache):
        if not cache:
            method(values[stat_col].values, out, downsample)
            return

        k = (stat_col, downsample, xform)  # Must key on the type of transform
        if k in self._downsample_cache:
            ##print('Using cached value : ', stat_col, '', downsample, ' ', ds_first, ds_last)
            v = self._downsample_cache[k]
            out[:] = v[ds_first: ds_last + 1]
            return

        # Cache the entire downsampled column
        ##print('Caching value : ', stat_col, '', downsample, ' ', ds_first, ds_last)
        vals = xform(self.pdf[stat_col].values)  # Warning: this is a tad expensive because we apply the transform to every value in the array before downsampling
        v = np.zeros(shape=(math.ceil(vals.shape[0] / downsample)))
        method(vals, v, downsample)
        self._downsample_cache[k] = v

        # Slice out the part of this column that we care about
        out[:] = v[ds_first: ds_last + 1]

    # Get a numpy list of points over a range with given `units`: `first` to `last` (inclusive)
    #
    # If any stat in stat_col is not allowed, throws an exception.
    #
    # Returns a single 2d numpy array of data.
    # Rows are each requested stat_col and columns are points over time.
    # First row is always the time units requested.
    #
    # For example: choosing stat cols ['A', 'B', 'C'] with units 'X' results in a 2d numpy array
    #
    # [ X units array  ]
    # [ A values array ]
    # [ B values array ]
    # [ C values array ]
    #
    # If no stats are given, returns just a units array.
    #
    # To enable auto-downsampling use max_points_per_series > 0. This will force down-sampling.
    #
    # Subclasses will override this to provide more elaborate statistic implementations
    def get_points(self, first, last, units, stat_cols, max_points_per_series, output_meta={}):
        first_row_idx, last_row_idx, unit_stat = self.range_to_rows(first, last, units)

        # Check stat cols are valid map their names to indices in the final array based on order
        stat_to_row = {}  # stat => series#, xform
        for i, stat in enumerate(stat_cols):
            if stat in self._stats:
                stat_to_row[stat] = (i + 1, NoTransform)  # row 0 is always 'time' units
            else:
                raise KeyError('No stat {} in this datasource {}. Known stats are '
                               .format(stat, self.data_source, self._stats))

        downsample, num_points, first_row_idx, last_row_idx, = \
            self.calc_aligned_downsampling(first_row_idx, last_row_idx, max_points_per_series)

        if 'downsampling' in output_meta:
            output_meta['downsampling'] = downsample

        # Final array ordered by original order of stats. Row 0 is extra row is for 'units' series
        arr = np.zeros(shape=(len(stat_cols) + 1, num_points))

        # Generate the downsampled pure trace events from the main dataframe
        self._copy_points(first_row_idx, last_row_idx, unit_stat, stat_to_row, downsample, arr)

        return arr

# Determine necessary amount of downsampling for data-set
# Note that we're using powers of 2 because it will improve the usefulness of caching results to have fewer downsample
# levels.
# TODO: Just use arithmetic for this
@numba.jit(nopython=True, nogil=True)
def calc_downsample(num_points, max_points):
    downsample = 2
    while num_points // downsample >= max_points:
        downsample *= 2
    return downsample

# Downsample while copying by taking the first value of each 'sample'.
# This is a purely index-based downsample that does not consider any attributes of the items being downsampled
# The resulting number of points should be floor(len(src)/downsample)
@numba.jit(nopython=True, nogil=True)
def downsample_copy_first(src, dest, downsample):
    if downsample < 1:
        raise ValueError('Downsample=0 not supported in downsample_copy_first')
    d = 0
    r = 0
    dr = 0
    l = len(src)
    while r < l:
        if d == 0:
            dest[dr] = src[r]
        d += 1
        if d == downsample:
            dr += 1
            d = 0
        r += 1

# Downsample while copying by taking the mean value of each 'sample'
# This is a purely index-based downsample
@numba.jit(nopython=True, nogil=True)
def downsample_copy_mean(src, dest, downsample):
    if downsample < 1:
        raise ValueError('Downsample=0 not supported in downsample_copy_mean')
    d = 0
    r = 0
    dr = 0
    l = len(src)
    while r < l:
        dest[dr] += src[r]
        d += 1
        if d == downsample:
            dest[dr] /= downsample  # take mean
            dr += 1
            d = 0
        r += 1
    if d != 0:
        dest[dr] /= d  # take mean
