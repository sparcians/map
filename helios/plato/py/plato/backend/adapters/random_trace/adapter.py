from plato.backend.units import Units

from plato.backend.datasources.random_trace.datasource import RandomTraceDataSource
from ..general_trace.adapter import GeneralTraceAdapter


# Example adapter for a random pseudo-data-source
class RandomTraceAdapter(GeneralTraceAdapter):

    def __init__(self, data_source: RandomTraceDataSource):
        super().__init__(data_source)

    # Override: We know this trace has trn_idx and cycles columns
    def get_stat_for_units(self, units):
        if units == Units.BRANCH_TRAINING_INDEX:
            return 'trn_idx'
        elif units == Units.CYCLES:
            return 'cycles'

        return super().get_stat_for_units(units)
