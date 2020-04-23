import hdf5plugin
import h5py
import time
import numpy as np
import numpy.lib.recfunctions as nprecfunctions
import pandas
from dask import dataframe as dd

from plato.backend.common import *
from plato.backend.units import Units
from plato.backend.datasources.general_trace.datasource import GeneralTraceDataSource

logger = logging.getLogger("plato.backend.datasources.branchTrainingHeatmap")


# Datasource for branch training
class BranchTrainingDatasource(GeneralTraceDataSource):

    # Possible names for the tables
    BRANCH_TRAINING_TABLE_NAMES = ['training_events', 'branchpredictor$training_events']
    WEIGHT_UPDATE_TABLE_NAMES = ['weight_updates', 'branchpredictor$weight_updates']

    # Prevent some stats from being forwarded to users
    IGNORE_TRAINING_FIELDS = {'my_first_weight_update_index', 'latest_weight_update_index'}
    IGNORE_WEIGHT_UPDATE_FIELDS = {'branch_index'}

    # Prefix for table-specific stats
    TABLE_STAT_PREFIX = 'table[{{table}}]{}'.format(VARIABLE_STAT_SEPARATOR)

    # Is a given table name (or any potential names) in an open h5py file
    @staticmethod
    def has_table(table_name, hf):
        if isinstance(table_name, list):
            l = table_name
        else:
            l = [table_name]

        for name in l:
            if name in hf:
                return True

        return False

    # Get a table based on its name (or any potential names) from an open h5py file
    @staticmethod
    def get_table(table_name, hf):
        if isinstance(table_name, list):
            l = table_name
        else:
            l = [table_name]

        for name in l:
            if name in hf:
                return hf[name]

        raise KeyError(f'Missing table in list {table_name}. Available tables are: {hf.keys()}')


    # For reference, branch training columns are:
    # ['trn_idx', 'cycle', 'pc', 'correct', 'taken', 'tgt', 'yout',
    #  'bias_at_lookup', 'theta_at_training', 'bias_at_training',
    #  'shpq_weights_found', 'dynamic_state', 'indirect', 'uncond',
    #  'my_first_weight_update_index', 'latest_weight_update_index']
    #
    # and weight update table columns are:
    # ['wup_idx', 'table', 'row', 'bank', 'lookup_weight', 'new_weight',
    #  'd_weight', 'd_unique', 'thrash_1', 'branch_index']

    # See if a given filename can be read by this datasource type
    @staticmethod
    def can_read(filename):
        try:
            with h5py.File(filename, 'r') as hf:
                if not BranchTrainingDatasource.has_table(BranchTrainingDatasource.BRANCH_TRAINING_TABLE_NAMES, hf):
                    return False
                if not BranchTrainingDatasource.has_table(BranchTrainingDatasource.WEIGHT_UPDATE_TABLE_NAMES, hf):
                    return False
        except Exception:
            return False

        return True

    # Reverse compatibility
    @staticmethod
    def get_time_extents(filename):
        return BranchTrainingDatasource.get_source_information(filename)['timeRange']

    # Figure out the start/end points of this thing. Much faster than actually opening this data-source (constructor)
    # Returns [] if no data was available, otherwise a list of dicts, each dict representing one inclusive time range
    # with units.
    @staticmethod
    def get_source_information(filename):
        with h5py.File(filename, 'r') as hf:
            # These are are lazy loads so they are fast (until accessed)
            ds_a = BranchTrainingDatasource.get_table(BranchTrainingDatasource.BRANCH_TRAINING_TABLE_NAMES, hf)
            ds_b = BranchTrainingDatasource.get_table(BranchTrainingDatasource.WEIGHT_UPDATE_TABLE_NAMES, hf)

            if len(ds_a) == 0 or len(ds_b) == 0:
                return {}  # No information available because file is empty, hf

            # Since this is a branch training dataset, get events in terms of branch count and cycles
            # WARNING: When getting data through hdf5, it is *very* important to ask for row FIRST, then column.
            #          Otherwise a copy of the entire column is made.
            first_row = ds_a[0]
            last_row = ds_a[-1]

            # TODO: Would also like to correlate this to INSTRUCTION number for better cross-referencing with
            #       time-series. This would have to be collected in the trace though.
            time_ranges = [{'units': Units.CYCLES,
                            'first': int(first_row['cycle']),
                            'last':  int(last_row['cycle']),
                            },
                           {'units': Units.BRANCH_TRAINING_INDEX,
                            'first': int(first_row['trn_idx']),
                            'last':  int(last_row['trn_idx']),
                            },
                           ]
            if 'instructions' in ds_a.dtype.fields:
                time_ranges.append(
                    {'units': Units.INSTRUCTIONS,
                     'first': int(first_row['instructions']),
                     'last': int(last_row['instructions']),
                     }
                )

            # Read stats from the file. Include a 'write' count in the weight update that is not in the file. When
            # actually loading this dataset we append this column. This code just looks at headers so adding the column
            # is not time-effective.
            # TODO: Stat information should come from meta-data in the file.. not a scan like this which takes time
            brn_stats, wupdate_stats, wildcard_stats, compound_stats = \
                BranchTrainingDatasource._calculate_stats(ds_a, ds_b, extra_weight_update_stats=['write'])
            stats = list(brn_stats.values()) + list(wildcard_stats.values()) + list(compound_stats.values())  # Take values only to reshape as a list

            # Read heatmap shape from rows
            # TODO: Load this from file table instead of scanning everything which takes time. With 1GB+ data, this
            #       takes ~1s, but it can be improved to near-zero.
            data = ds_b[()]  # Warning - reads data
            type_specific = {'shape': {'table': {'min': int(data['table'].min()),
                                                 'max': int(data['table'].max()), },
                                       'row':   {'min': int(data['row'].min()),
                                                 'max': int(data['row'].max()), },
                                       'bank':  {'min': int(data['bank'].min()),
                                                 'max': int(data['bank'].max()), }
                                       }
                             }

            return {
                'timeRange': time_ranges,
                'typeSpecific': type_specific,
                'stats': stats,
            }

    # Common method for calculating stats. Takes h5py datasets or nummpy recarrays
    @staticmethod
    def _calculate_stats(ds_training_events, ds_weight_updates, extra_weight_update_stats=[]):

        # TODO: These should be flagged in metadata on the the data-collection side rather than being defined here
        summable_stats = {# Branch
                          'correct', 'taken', 'shpq_weights_found',

                          # Weight update
                          'd_weight', 'd_unique', 'thrash_1', 'write'
                          }

        attribute_stats = {# Branch
                           'pc', 'tgt', 'yout', 'indirect', 'uncond',
                           'bias_at_lookup', 'theta_at_training', 'bias_at_training',
                           'dynamic_state', 'my_first_weight_update_index', 'latest_weight_update_index',

                           # Weight update
                           'table', 'row', 'bank', 'lookup_weight', 'new_weight', 'branch_index',

                          }

        def make_stat(s):
            stat = {'name': s}

            if s in summable_stats:
                stat['type'] = STAT_TYPE_SUMMABLE
            elif s in attribute_stats:
                stat['type'] = STAT_TYPE_ATTRIBUTE
            else:
                stat['type'] = STAT_TYPE_COUNTER

            return stat

        def make_branch_stat(s):
            stat = make_stat(s)

            stat['sourceSpecific'] = {'association': 'branch'}

            # Warning: this is slow. This data must be pre-stored in the datasource
            #stat['min'] = float(ds_training_events[s].min())
            #stat['max'] = float(ds_training_events[s].max())

            return stat

        def make_wupdate_stat(s):
            stat = make_stat(s)

            stat['sourceSpecific'] = {'association': 'weight'}

            # Warning: this is slow. This data must be pre-stored in the datasource
            #stat['min'] = float(ds_weight_updates[s].min())
            #stat['max'] = float(ds_weight_updates[s].max())

            return stat

        brn_stats = set(filter(lambda s: s not in BranchTrainingDatasource.IGNORE_TRAINING_FIELDS, ds_training_events.dtype.names))
        brn_stats = list(map(make_branch_stat, brn_stats))
        brn_stats = dict(list(map(lambda stat: (stat['name'], stat), brn_stats)))  # Expand to dict

        wupdate_stats = set(filter(lambda s: s not in BranchTrainingDatasource.IGNORE_WEIGHT_UPDATE_FIELDS, ds_weight_updates.dtype.names))
        wupdate_stats = wupdate_stats.union(extra_weight_update_stats)
        wupdate_stats = list(map(make_wupdate_stat, wupdate_stats))
        wupdate_stats = dict(list(map(lambda stat: (stat['name'], stat), wupdate_stats)))  # Expand to dict

        # For now, just advertise table[n] substitution
        def transform_wildcard_stat(s):
            stat = make_wupdate_stat(s)
            stat['name'] = '{}{}'.format(BranchTrainingDatasource.TABLE_STAT_PREFIX, stat['name'])  # Add table prefix w/ variable placeholder to name
            return stat

        wildcard_stats = list(map(transform_wildcard_stat, wupdate_stats))
        wildcard_stats = dict(list(map(lambda stat: (stat['name'], stat), wildcard_stats)))  # Expand to dict

        # TODO: Replace this with the python3 ast module that can parse an equation (e.g. abs(x)/y) and validate support
        #       for the expression chosen.
        compound_stats = [
            # TODO: Support compound stats based on a combination of per-table and per-branch values

            # Make a per-table stat. This can only depend on per-table stats (for now)
            {
                'name': BranchTrainingDatasource.TABLE_STAT_PREFIX + 'd_weight_mag', ##'abs_weight_change',
                'type': STAT_TYPE_SUMMABLE,
                'compound': {
                    'a': 'd_weight',
                    'b': None,
                    'op': None,
                    'a_abs': True,
                },
                'explanation': 'Captures the absolute value of each weight change and adds them up to measure "thrashing"',
            },

            # Make a non-per-table stat
            {
                'name': 'branch_target_distance',
                'type': STAT_TYPE_SUMMABLE,
                'compound': {
                    'a': 'pc',
                    'b': 'tgt',
                    'op': '-',
                },
                'explanation': 'Measures the distance of branch targets from the branches',
            },
        ]
        compound_stats = dict(list(map(lambda stat: (stat['name'], stat), compound_stats)))  # Expand to dict

        return brn_stats, wupdate_stats, wildcard_stats, compound_stats

    # Create the datasource
    @logtime(logger)
    def __init__(self, filename):
        super().__init__(filename)

        # Load using h5py (pandas/dask read_hdf5 does NOT support compression) and read everything into memory.
        # Ideally we could do this keeping things on disk, but that is slower right now. This can be added as an
        # optimization later

        hf = h5py.File(filename, 'r')  # Must never force-close. Dask retains a pointer

        # TODO: Try and get away from dask because the initial loading might take longer than just using pandas
        # This is a loading process that does not require re-indexing.
        # Using dask dataframes from_array/records causes indexing trouble which requires a slow set_index to fix.
        # The downside of this is that everything needs to be realized in memory. This is fine for server-class
        # machines but doing this on a desktop with single-digit ram can make it unresponsive for big enough datasets
        # Note that trying to construct a pandas dataframe without this copy can be incredibly slow (like 5x+).
        ds_a = BranchTrainingDatasource.get_table(BranchTrainingDatasource.BRANCH_TRAINING_TABLE_NAMES, hf)
        np_a = ds_a[()]  # WARNING: Load into memory now
        self.pdf_branch_training_events = pandas.DataFrame(np_a)
        self.ddf_branch_training_events = dd.from_pandas(self.pdf_branch_training_events, chunksize=50000)

        ds_b = BranchTrainingDatasource.get_table(BranchTrainingDatasource.WEIGHT_UPDATE_TABLE_NAMES, hf)
        np_b = ds_b[()]  # WARNING: Load into memory now

        # Modify this data-set to add a 'write' field of all 1s if not present already. This is much easier to do than
        # special-case logic to determine the writes to each stat. The meory cost is worth it.
        if 'write' not in np_b.dtype.fields:
            # This takes 2-3 seconds on the big data-set. That is not ideal and it should be logged with the original
            # data when possible.
            np_b = nprecfunctions.append_fields(np_b, 'write', np.ones(len(np_b)), dtypes=['?'], usemask=False)

        self.pdf_weight_update_events = pandas.DataFrame(np_b)
        self.ddf_weight_update_events = dd.from_pandas(self.pdf_weight_update_events, chunksize=50000)

        # Find and filter stats
        self._brn_stats, self._wupdate_stats, self._wildcard_stats, self._compound_stats = \
            BranchTrainingDatasource._calculate_stats(np_a, np_b)

        # Map to compound stats by the short name (i.e. no 'table[{table}].' prefix)
        self._compound_stats_by_short_name = dict(map(lambda p: (p[0].replace(self.TABLE_STAT_PREFIX,''), p[1]), self._compound_stats.items()))

        # List if stat names that can be addressed
        self._stats = list(self._brn_stats) + list(self._wildcard_stats) + list(self._compound_stats)

    def __str__(self):
        return '{}({})'.format(self.__class__.__name__, self.filename)

    # Simple trace data interface (from GeneralTraceDataSource)
    def pdf(self):
        return self.pdf_branch_training_events

    # Return a list of stats contained in this data-source
    @property
    def stats(self):
        return self._stats
