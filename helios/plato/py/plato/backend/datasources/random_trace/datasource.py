import numpy as np
import pandas

from ..general_trace.datasource import GeneralTraceDataSource


# Example random-number trace pseudo-data-source
class RandomTraceDataSource(GeneralTraceDataSource):

    # Create the datasource
    def __init__(self):
        super().__init__('')

        # Make some random data
        data = np.zeros(shape=10000000, dtype=np.dtype([('trn_idx', 'u8'), ('cycles', 'u8'), ('stat', 'i8')]))

        for i in range(1, len(data)):
            s = data[i - 1][2] + np.random.randint(-3, 4)
            data[i] = (i, i * 4 + np.random.randint(0, 3), s)

        # Construct a dataframe
        self._pdf = pandas.DataFrame(data)

    # Overriding GeneralTraceDataSource to give GeneralTraceAdapter adapter access to raw data
    def pdf(self) -> pandas.DataFrame:
        return self._pdf
