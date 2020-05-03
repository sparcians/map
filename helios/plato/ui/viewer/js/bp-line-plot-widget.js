
class BpLinePlotWidget extends SimpleLinePlotWidget {
    constructor(el, id) {
        super(el, id)

        // Initial values
        this._stat_cols = ['yout', 'correct']
    }

    // Does this widget support branch trace filtering
    supports_branch_trace_filtering() {
        return true
    }

    // Override to deal with stats having {table} variables in them for branch predictor use
    on_add_stat(new_stat_name) {
        try {
            // Expand the stats to the data-set if they have wildcards
            const new_stats = []
            const var_name = parse_stat_variable(new_stat_name)
            if (var_name != null) {
                // Create a stat for all substitutions of the variable
                // TODO: This adds stats immediately but if reloading with new data wildcard substitutions may be different and we'll need a way to reload them
                const subs = this.data_source.get_substitutions(var_name)
                for (const sub of subs) {
                    new_stats.push(new_stat_name.replace('{' + var_name + '}', sub))
                }
            } else {
                new_stats.push(new_stat_name)
            }

            this._add_new_stats(new_stats) // superclass

            $(this.element).effect('highlight') // positive feedback from the drag/drop
        } catch (error) {
            time_error(`Error adding stat to widget: ${error.message} ${error.stack}`)
            $(this.element).effect('shake')
        }
    }
}
BpLinePlotWidget.typename = 'bp-line-plot'
BpLinePlotWidget.description = 'A simple line plot from branch predictor trace data'
BpLinePlotWidget.processor_type = Processors.SHP_LINE_PLOT
BpLinePlotWidget.data_type = DataTypes.SHP_TRAINING_TRACE
