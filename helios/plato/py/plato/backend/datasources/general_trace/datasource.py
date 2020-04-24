import pandas


# Datasource for general traces
class GeneralTraceDataSource:

    # Create the datasource
    def __init__(self, filename):
        self.filename = filename

    def __str__(self):
        return '{}({})'.format(self.__class__.__name__, self.filename)

    def pdf(self) -> pandas.DataFrame:
        raise NotImplementedError('GeneralTraceDataSource must provide a pdf implementation that returns a pandas dataframe')

