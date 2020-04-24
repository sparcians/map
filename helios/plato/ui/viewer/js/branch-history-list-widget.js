// A branch history table widget
class BranchHistoryTableWidget extends BranchHtmlWidgetBase {
    constructor(el, id) {
        super(el, id, 500)

        this._stat = 'thrash_1'
        this._mispreds_only = false

        const option_name = this.widget_id.toString() + '-bp-trace-show-mispreds'

        const mispred_filters = $(`
            <div class="widget-title-control-group" style="float:right;">
                Show:
                <input type="radio" value="mispredicts only" name="${option_name}" id="${this.widget_id.toString()}-bp-trace-mispred-only" />
                <label for="${this.widget_id.toString()}-bp-trace-mispred-only">mispred ubrn only</label>
                <input type="radio" value="all branches" name="${option_name}" id="${this.widget_id.toString()}-bp-trace-all"/>
                <label for="${this.widget_id.toString()}-bp-trace-all">all ubranches</label>
            </div>`)
        this.header_area.append(mispred_filters)

        // Add geometry filter controls for this widget only
        this.geometry_filters = new GeometryFilterControls(this.header_area)
        this.geometry_filters.onchange = () => {
            this.viewer.refresh_widget(this, {force_no_sync: true})
        }

        this.header_area.find(`input[name='${option_name}']`).checkboxradio({
            icon: false
        })

        this._update_options()

        // Register this change event after _update_options so it does not react to setting initial values.
        this.header_area.find(`input[name='${option_name}']`).change((e) => {
            if (e.target.id.search('mispred-only') >= 0) {
                this._mispreds_only = true
            } else {
                this._mispreds_only = false
            }

            this.viewer.refresh_widget(this, {force_no_sync: true})
        })
    }

    get_config_data() {
        const parent = super.get_config_data()
        const child = {
            stat_cols: [this._stat],
            mispreds_only: this._mispreds_only,
            geometry_filter: this.geometry_filters.get_config_data(),
        }

        // TODO bring back when RHEL's browsers support ES6
        //return {...parent, ...child}
        return Object.assign({}, parent, child)
    }

    apply_config_data(d) {
        super.apply_config_data(d)

        const stat_cols = d.stat_cols
        if (typeof stat_cols == 'undefined')
            this._stat = 'thrash_1'
        else
            this._stat = stat_cols[0]
        this._mispreds_only = typeof d.mispreds_only == 'undefined' ? false : d.mispreds_only

        this.geometry_filters.apply_config_data(d.geometry_filter || {})

        this._update_options()
    }

    _update_options() {
        if (this._mispreds_only) {
            this.header_area.find('#' + this.widget_id.toString() + '-bp-trace-mispred-only').prop('checked', true).change()
        } else {
            this.header_area.find('#' + this.widget_id.toString() + '-bp-trace-all').prop('checked', true).change()
        }
    }

    // Support filtering by branch using the plato bp filter settings
    supports_branch_trace_filtering() {
        return true
    }

    get_request_kwargs() {
        return { stat_cols: [this._stat],
                 max_rows: 1000, // How many rows to grab in the table
                 show_only_mispredicts: this._mispreds_only,
                 geometry_filter: {
                    row: this.geometry_filters.row,
                    table: this.geometry_filters.table,
                    bank: this.geometry_filters.bank,
                 },
                }
    }

    on_clear_stats() {
        this._stat = null
    }

    on_add_stat(new_stat_name) {
        // Check that the stat is valid. The heatmap can only accept stats with a {table} variable.
        // TODO: Base this check on stat meta-data instead of the name.
        if (parse_stat_variable(new_stat_name) != 'table') {
            this.viewer.show_error_popup('Invalid stat for BranchHistoryTableWidget', 'BranchHistoryTableWidget widget requires a stat with a table variable (e.g. table[t]/d_weight)')
            $(this.element).effect('shake')
            return
        }

        try {
            // TODO: Most of this logic is common with the HeatMapWidget too: refactor it
            // Heatmap requests use non-standard stat names because a per-table branch predictor stat is assumed.
            // Drop the "table[...]/" part of the string.
            const fixed_stat_name = new_stat_name.split(STAT_NAME_SEPARATOR)[1]
            this._stat = fixed_stat_name

            // Ask for a refresh to redraw with the new stat (this is probably redundant with the _reassign_data)
            this.viewer.refresh_widget(this, {force_no_sync: true})

            $(this.element).effect('highlight') // positive feedback from the drag/drop
        } catch (error) {
            time_error(`Error adding stat to widget: ${error.message} ${error.stack}`)
            $(this.element).effect('shake')
        }
    }

    on_render() {
        super.on_render()

        const table = this.table_area.find('table')
        if (table.length == 0)
            return // no data yet

        // Attach titles and links to the cells where appropriate
        // WARNING: This takes a while to do and then to for the browser to deal with after. This is a good reason to
        //          keep this widget manual-update-only.
        const tds = table.find('td[bank]')
        for (const _td of tds) {
            const td = $(_td)
            const bank = td.attr('bank')
            const table = td.attr('table')
            const row = td.attr('row')
            td.attr('title', `bank ${bank}, table ${table}, row ${row}`)

            if (td.find('a').length == 0) {
                const a = $(`<a href="javascript:void(0);" class="stealth-link">${td.html()}</a>`)
                td.html(a)
                a.on('click', () => { this._select_cell(bank, table, row) })
            }
        }
    }

    on_global_filter_configuration_changed() {
        // We should require a manual update to refresh. Refreshing here would prevent us from viewing 2 different
        // filters at once at the moment.
        // this.viewer.refresh_widget(this, {force_no_sync: true})
    }

    _select_cell(bank, table, row) {
        this.geometry_filters.apply_config_data({bank: bank, table: table, row: row})

        this.viewer.refresh_widget(this, {force_no_sync: true})
    }
}

BranchHistoryTableWidget.typename = 'bp-trace-table'
BranchHistoryTableWidget.description = 'Branch prediction history as a table'
BranchHistoryTableWidget.processor_type = Processors.SHP_BRANCH_LIST
BranchHistoryTableWidget.data_type = DataTypes.SHP_TRAINING_TRACE
