
class SimDbLinePlotWidget extends SimpleLinePlotWidget {
    constructor(el, id) {
        super(el, id)

        // Initial values
        this._stat_cols = []
    }
}
SimDbLinePlotWidget.typename = 'sim-line-plot'
SimDbLinePlotWidget.description = 'A simple line plot from simdb data'
SimDbLinePlotWidget.processor_type = Processors.SIMDB_LINE_PLOT
SimDbLinePlotWidget.data_type = DataTypes.SIMDB
