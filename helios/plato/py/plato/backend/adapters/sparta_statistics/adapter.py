from plato.backend.units import Units

from plato.backend.datasources.sparta_statistics.datasource import SpartaDataSource
from ..general_trace.adapter import GeneralTraceAdapter


# Example adapter for a random pseudo-data-source
class SpartaStatisticsAdapter(GeneralTraceAdapter):

    statsForUnits = {Units.CYCLES: SpartaDataSource.cyclesStat}

    def __init__(self, data_source: SpartaDataSource):
        super().__init__(data_source)

    # Override: We know this trace has trn_idx and cycles columns
    def get_stat_for_units(self, units):
        if units in SpartaStatisticsAdapter.statsForUnits:
            return SpartaStatisticsAdapter.statsForUnits[units]

        return super().get_stat_for_units(units)
