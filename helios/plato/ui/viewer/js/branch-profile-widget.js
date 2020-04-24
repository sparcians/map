// Describes a simple branch profile widget

class BranchProfileTableWidget extends BranchHtmlWidgetBase {
    constructor(el, id) {
        super(el, id, 500)

        this._stat = 'thrash_1'
        this._sort_col = 'count'
        this._sort_descending = true

        // Add geometry filter controls for this widget only
        this.geometry_filters = new GeometryFilterControls(this.header_area)
        this.geometry_filters.onchange = () => {
            this.viewer.refresh_widget(this, {force_no_sync: true})
        }
    }

    get_config_data() {
        const parent = super.get_config_data()
        const child = {
            stat_cols: [this._stat],
            sort_col: this._sort_col,
            sort_descending: this._sort_descending,
            geometry_filter: this.geometry_filters.get_config_data(),
        }

        // TODO bring back when RHEL's browsers support ES6
        //return {...parent, ...child}
        return Object.assign({}, parent, child)
    }

    apply_config_data(d) {
        super.apply_config_data(d)

        const stat_cols = d.stat_cols || ['thrash_1']
        this._stat = stat_cols[0]
        const sort_cols = d.sort_col
        if (typeof sort_cols != "undefined" && sort_cols.hasOwnProperty('length') && typeof sort_cols != 'string')
            this._sort_col = sort_cols[0]
        else
            this._sort_col = d.sort_col || 'count'
        this._sort_descending = typeof d.sort_descending == 'undefined' ? true : d.sort_descending

        this.geometry_filters.apply_config_data(d.geometry_filter || {})
    }

    get_request_kwargs() {
        return { stat_cols: [this._stat],
                 sort_col: this._sort_col,
                 sort_descending: this._sort_descending,
                 max_rows: 1000, // How many rows to grab in the table
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
            this.viewer.refresh_widget(this, {force_sync: true})

            $(this.element).effect('highlight') // positive feedback from the drag/drop
        } catch (error) {
            time_error(`Error adding stat to widget: ${error.message} ${error.stack}`)
            $(this.element).effect('shake')
        }
    }

    on_render() {
        super.on_render()

        this._apply_filter_links()
    }

    on_global_filter_configuration_changed() {
        this._apply_filter_links()
    }

    _apply_filter_links() {
        const table = this.table_area.find('table')
        if (table.length == 0)
            return // no data yet


        // Sorting controls
        for (const _th of table.find('th')) {
            const th = $(_th)
            const id = th.prop('id')
            if (id == '') {
                // Not sortable, no id
                continue
            }

            const sort_buttons = th.find('.bp-profile-sort-button')
            if (sort_buttons.length == 0) {
                const sort_asc = $(`<div class="bp-profile-sort-button"><span class="ui-icon ui-icon-triangle-1-n"></span></div>`)
                const sort_des = $(`<div class="bp-profile-sort-button"><span class="ui-icon ui-icon-triangle-1-s"></span></div>`)

                th.append('<br/>')
                th.append(sort_asc)
                th.append(sort_des)

                // Set initial color if sorting with this stat
                if (this._sort_col == id) {
                    if (this._sort_descending)
                        sort_des.addClass('active')
                    else
                        sort_asc.addClass('active')
                }


                // TODO: Sorting can be done locally faster than on the server because the server will recalculate. But
                //       if the server cached intermediate numpy results then the server would be faster and no code here
                //       would have to change.
                sort_asc.off('click').on('click', () => {
                    this._sort_col = id
                    this._sort_descending = false

                    table.find('th').find('.bp-profile-sort-button').removeClass('active')
                    sort_asc.addClass('active')

                    // TODO: This should be a refresh at the current range not the whole range
                    this.viewer.refresh_widget(this, {force_sync: true})
                })
                sort_des.off('click').on('click', () => {
                    this._sort_col = id
                    this._sort_descending = true

                    table.find('th').find('.bp-profile-sort-button').removeClass('active')
                    sort_des.addClass('active')

                    // TODO: This should be a refresh at the current range not the whole range
                    this.viewer.refresh_widget(this, {force_sync: true})
                })
            }

        }


        // Filtering controls
        const pc_col = table.find('th').index(table.find('th#pc'))

        for (const tr of table.find('tr')) {
            const tds = $(tr).find('td')
            if (tds.length == 0)
                continue; // header row

            // Construct a link that will add this branch to the branch predictor filter.
            const td = $(tds[pc_col])

            let anchor = td.find('a')
            if (anchor.length == 0) {
                const pc = td.html()
                anchor = $(`<a href="javascript:void(0);" style="float:right;" pc="${pc}" class="bp-profile-filter-link" title="Add/remove branch branch predictor filter. The branch filtering control panel is on the left stack of panels."></a>`)
                td.append('&nbsp;')
                td.append(anchor)
            }

            const pc = anchor.attr('pc')

            const contains_branch = this.viewer.branch_filt_ctrl.contains_branch(pc)
            const branch_enabled = this.viewer.branch_filt_ctrl.branch_inclusive_and_enabled(pc)
            if (branch_enabled) {
                td.css({'background-color': '#ff6' })
                anchor.html('[-]')
                anchor.off('click').on('click', () => {
                    this.viewer.branch_filt_ctrl.remove(pc)
                    this.viewer.branch_filt_ctrl.flash() // show that something happened over in the bp filter panel
                })
            } else {
                td.css({'background': 'none' })
                anchor.html('&nbsp;+&nbsp;')
                anchor.off('click').on('click', () => {
                    if (contains_branch) {
                        this.viewer.branch_filt_ctrl.include_and_enable_existing(pc)
                    } else {
                        this.viewer.branch_filt_ctrl.add(pc, true) // include
                    }
                    this.viewer.branch_filt_ctrl.flash() // show that something happened over in the bp filter panel
                })
            }
        }
    }
}

BranchProfileTableWidget.typename = 'bp-profile-table'
BranchProfileTableWidget.description = 'Branch predictor branch profiles'
BranchProfileTableWidget.processor_type = Processors.SHP_BRANCH_PROFILE
BranchProfileTableWidget.data_type = DataTypes.SHP_TRAINING_TRACE
