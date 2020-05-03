import numba
import numpy as np


@numba.jit(nopython=True, nogil=True)
def NoTransform(value_in):
    return value_in


@numba.jit(nopython=True, nogil=True)
def AbsValueTransform(value_in):
    return np.abs(value_in)


# Interpret stats, turning any compound stats into individual stats.
# Takes a list of stat names and a map of all compound stats' stat-names to their stat structure.
# Returns a list of tuples [ (raw_stat_name1, Transform1), (raw_stat_name2, Transform2), ... ]
def interpret_compound_stats(stat_names, compound_stats_map):
    real_stat_cols = []
    for stat_name in stat_names:
        if stat_name not in compound_stats_map:
            real_stat_cols.append((stat_name, NoTransform))
        else:
            stat = compound_stats_map[stat_name]

            # Break out the components
            compound = stat['compound']
            xform = AbsValueTransform if compound.get('a_abs', False) else NoTransform
            real_stat_cols.append((compound['a'], xform))
            if compound.get('b', None) is not None:
                assert(compound.get('op', None) is not None), 'Compound stat {} missing op'.format(stat_name, stat)
                xform = AbsValueTransform if compound.get('b_abs', False) else NoTransform
                real_stat_cols.append((compound['b'], xform))

    return real_stat_cols


# Takes a stat name, and a map of all compound stats' stat-names to their stat structure. Also takes
# inputs a and b which should be numpy arrays or some other structure which can accept broadcasted component-wise
# arithmetic operators (e.g. */+-). a and b should have been populated as prescribed by interpret_compound_stats(...).
# Returns the appropriate combination of a and b.
def assemble_compound_stat(stat_name, compound_stats_map, a, b=None):
    if stat_name not in compound_stats_map:
        return a

    # Get the compound info
    compound = compound_stats_map[stat_name]['compound']

    if compound.get('b', None) is None:
        return a

    assert(b is not None), 'compound stat {} requires a and b inputs, but b is None'.format(stat_name)

    op_name = compound['op']
    op = {
        '*': np.multiply,
        '/': np.divide,
        '+': np.add,
        '-': np.subtract,
    }[op_name]
    return op(a, b)


#{
#'name': 'abs_weight_change',
#'type': STAT_TYPE_SUMMABLE,
#'compound': {
#    'a': 'd_weight',
#    'b': None,
#    'op': None,
#    'a_abs': True,
#}