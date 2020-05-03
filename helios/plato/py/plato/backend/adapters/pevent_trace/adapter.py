from plato.backend.units import Units

from plato.backend.datasources.pevent_trace.datasource import PeventDataSource
from ..general_trace.adapter import GeneralTraceAdapter
import numpy as np

# Example adapter for a random pseudo-data-source
class PeventTraceAdapter(GeneralTraceAdapter):

    statsForUnits = {Units.CYCLES: PeventDataSource.cyclesStat}

    def __init__(self, data_source: PeventDataSource):
        super().__init__(data_source)

    # Override: We know this trace has trn_idx and cycles columns
    def get_stat_for_units(self, units):
        if units in PeventTraceAdapter.statsForUnits:
            return PeventTraceAdapter.statsForUnits[units]

        return super().get_stat_for_units(units)

    def get_points(self, first, last, units, stat_cols, max_points_per_series):
        '''
        just get all the points for these stat_cols since they're traces
        '''
        # TODO no downsampling yet
        # TODO no maximum points yet, this is to be used for histograms
        returnArrayList = []

        for stat in stat_cols:
            currentColumn = self.pdf[stat]

            firstIndex = np.searchsorted(currentColumn, first, side="left")
            lastIndex = max(np.searchsorted(currentColumn, last, side="left") - 1, firstIndex)

            returnArrayList.append(currentColumn[firstIndex:lastIndex])

        return returnArrayList
