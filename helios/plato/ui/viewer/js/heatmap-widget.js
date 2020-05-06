// Temp Custom color scales for heatmap
const ColorScaleGradientBlueGreenRed = [
    [0.0, 'rgb(180,180,255,0.3)'],
    [0.1, 'rgb(50,90,240,0.3)'],
    [0.2, 'rgb(50,110,220,0.3)'],
    [0.3, 'rgb(50,180,125,0.3)'],
    [0.4, 'rgb(50,190,100,0.3)'],
    [0.5, 'rgb(50,200,20,0.3)'],
    [0.6, 'rgb(100,200,0,0.3)'],
    [0.7, 'rgb(180,200,0,0.5)'],
    [0.8, 'rgb(255,200,0,0.7)'],
    [0.9, 'rgb(255,100,0,0.9)'],
    [1.0, 'rgb(255,0,0,1.0)'],
    ]


HEATMAP_MODE = {
    SUM: 'sum',     // Sum of all values in the range
    DIFF: 'diff',   // Last value minus the first value
    LAST: 'last',   // Last value
    FIRST: 'first', // First value
}

// Shape of the heatmap
class HeatMapShape {
    constructor(ntables, nrows, nbanks) {
        this.ntables = ntables
        this.nrows = nrows
        this.nbanks = nbanks

        this.ydim = ntables * nbanks
        this.xdim = nrows

        this.xValues = [...Array(this.xdim).keys()];
        this.yValues = [...Array(this.ydim).keys()];
    }

    // Get the table and bank for a combined index
    get_bank_and_table(index) {
        return [index % this.nbanks, Math.floor(index / this.nbanks)]
    }

    make_zero_array() {
        const z = []
        for(const y of this.yValues) {
            z.push([])
            const zy = z[z.length - 1]
            for (const x of this.xValues) {
                zy.push(0)
            }
        }
        return z
    }
}

// HeatMap widget. Construct with the DOM div for the heatmap
class HeatMapWidget extends BaseWidget {
    constructor(el, id) {
        super(el, id, 500)
        this.heatmap_div_name = this.widget_id + '-heatmap-div-' + id.toString()
        this.heatmap_table_stats_name = this.widget_id + '-heatmap-table-stats-div' + id.toString()
        this.heatmap_table_chart_name = this.widget_id + '-heatmap-table-chart-div-' + id.toString()
        this.heatmap_row_chart_name = this.widget_id + '-heatmap-row-chart-div-' + id.toString()
        this.heatmap_info_name = this.widget_id + '-heatmap-info-div-' + id.toString()

        // Set up the heatmap grid of plots
        // Note that for containing graphs min-height/width should be set on the divs
        const content = `
            <div style="display:grid; width:100%; height:100%; grid-template-rows:90px 1fr 20px; grid-template-columns: auto 120px 1fr 10px; grid-template-areas: 'top-left top-left top .......' 'far_left left main right' 'far_left left_txt main_txt right_txt';">
              <div id="${this.heatmap_info_name}" class="plato-hm-grid-box plato-hm-top" style="grid-area: top-left; min-height:0px; min-width:0px; padding: 2px;"></div>
              <div id="${this.heatmap_row_chart_name}" class="plato-hm-grid-box plato-hm-top" style="grid-area: top; min-height:0px; min-width:0px;"></div>
              <div id="${this.heatmap_table_stats_name}" class="plato-hm-grid-box plato-hm-top" style="grid-area: far_left; min-height:0px; min-width:0px;"></div>
              <div id="${this.heatmap_table_chart_name}" class="plato-hm-grid-box plato-hm-left" style="grid-area: left; min-height:0px; min-width:0px;"></div>
              <div id="${this.heatmap_div_name}" class="plato-hm-grid-box plato-hm-main" style="grid-area: main; min-height:0px; min-width:0px;"></div>
              <div class="plato-hm-grid-box plato-hm-right" style="grid-area: right;">
                <div id="slider-threshold" style="height:250px; margin-left:2px;">
                  <div id="slider-threshold-handle" class="ui-slider-handle"></div>
                  <div id="slider-threshold-handle2" class="ui-slider-handle"></div>
                </div>
              </div>
              <div style="grid-area: left_txt; text-align: center;">mean value</div>
              <div style="grid-area: main_txt; text-align: center;">rows</div>
              <div style="grid-area: right_txt;"></div>
            </div>
        `
        this.jbody.html(content)
        this.plotly_hm = $(el).find('#' + this.heatmap_div_name)[0]
        this.datarevision = 0

        this.stat_div = this.jbody.find('#' + this.heatmap_table_stats_name)

        // Initial shape
        this.shape = new HeatMapShape(16, 2048, 2)

        // Initial data
        this.zData = this.shape.make_zero_array()
        this.zValues = null
        this.row_values = null

        // Initial settings
        this._stats_collapsed = false

        // Set up the threshold slider
        //$(this.element).find("#slider-threshold").slider({
        //  orientation: "vertical",
        //  range: true,
        //  step: 0.01,
        //  min: 0,
        //  max: 100,
        //  values: [0,100],
        //  slide: function (event, ui) {
        //    let lo = ui.values[0] // $( "#slider-threshold" ).slider( "values", 0 )
        //    let hi = ui.values[1] // $( "#slider-threshold" ).slider( "values", 1 )
        //
        //    //handle.text(lo)
        //    //handle2.text(hi)
        //
        //    //updateThresholdValueDisplay(lo, hi)
        //    //updateHeatmapFromSlider(forceRecalc=false) // reapply threshold to heatmap
        //  }
        //})

        this._heatmap_mode = HEATMAP_MODE.SUM
        this._stat = 'thrash_1'
        this._stat_full_name = 'table[{table}].thrash_1' // This is the full name from the data-source, including a variable placeholder

        this._cell_filter_dlg = new ApplyCellFilterToWidgetsDialog()

        // Generate the layouts after configuration
        this._make_layouts()

        // Heatmap double-click detection
        this._last_click = 0
        this._last_click_timeout = null

    }

    supports_branch_trace_filtering() {
        return true
    }

    get_processor_kwargs() {
        return { stat_columns: [this._stat_full_name] }
    }

    on_clear_stats() {
        this._stat = null
        this._stat_full_name = null
    }

    // got a new stat from the ui.
    on_add_stat(new_stat_name) {
        // Check that the stat is valid. The heatmap can only accept stats with a {table} variable.
        // TODO: Base this check on stat meta-data instead of the name.
        if (parse_stat_variable(new_stat_name) != 'table') {
            this.viewer.show_error_popup('Invalid stat for HeatMap', 'Heatmap widget requires a stat with a table variable (e.g. table[t]/d_weight)')
            $(this.element).effect('shake')
            return
        }

        try {
            // Heatmap requests use non-standard stat names because a per-table branch predictor stat is assumed.
            // Drop the "table[...]/" part of the string.
            const fixed_stat_name = new_stat_name.split(STAT_NAME_SEPARATOR)[1]

            this._assign_stat(fixed_stat_name, new_stat_name)

            $(this.element).effect('highlight') // positive feedback from the drag/drop
        } catch (error) {
            time_error(`Error adding stat to widget: ${error.message} ${error.stack}`)
            $(this.element).effect('shake')
        }
    }

    // Called upon start of widget resize
    on_resize_started() {
    }

    // Called upon end of widget resize
    on_resize_complete() {
        this.render()
    }

    get_config_data() {
        return { stat: this._stat,
                 stat_full_name: this._stat_full_name,
                 table_stats_collapsed: this._stats_collapsed,
                 heatmap_mode: this._heatmap_mode,
                }
    }

    // (Load) Apply a dictionary of configuration data specific to this type of widget
    apply_config_data(d) {

        // Store heatmap mode first because assigning stat can clobber this if the mode is not compatible
        this._heatmap_mode = d.heatmap_mode || HEATMAP_MODE.SUM

        // Note that the full name guess here is a hack to support old layouts.
        // This will also select the appropriate heatmap mode
        this._assign_stat(d.stat, d.stat_full_name || 'table[{table}].' + d.stat)

        if (typeof d.table_stats_collapsed != 'undefined')
            this._stats_collapsed = false
        else
            this._stats_collapsed = d.table_stats_collapsed
    }

    // Called when new data is available to render
    on_update_data(json) {
        if (json == null) {
            this.zValues = null
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            return
        }

        const msg = json.raw_binary_data

        const rows = json.processorSpecific.numRows
        const columns = json.processorSpecific.numCols
        const z_min = json.processorSpecific.zMin
        const z_max = json.processorSpecific.zMax
        const z_data_offset = json.processorSpecific.zBlobOffset

        if (rows == 0 || columns == 0) {
            this.zValues = null
            this.table_values = null
            this.row_values = null
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            return
        }

        if (rows != this.shape.ydim || columns != this.shape.xdim) {
            this.zValues = null
            this.table_values = null
            this.row_values = null
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            this.viewer.show_error_popup('Invalid getData result for HeatMap',
                                         `rows,columns (${rows},${columns}) did not match heatmap shape from processor creation: (${this.shape.ydim}, ${this.shape.xdim})`)
            return
        }

        // Copy over data into display-array
        // TODO: See if this copy can be avoided
        const data = new Float32Array(msg, z_data_offset, rows * columns)
        for (const y of this.shape.yValues) {
            const zy = this.zData[y]
            for (const x of this.shape.xValues) {
                zy[x] = data[y*this.shape.xdim + x]
            }
        }
        this.zValues = this.zData

        const table_vals_offset = json.processorSpecific.tableMeansBlobOffset
        if (typeof table_vals_offset == 'undefined') {
            this.table_means = null
        } else {
            this.table_means = new Float32Array(msg, table_vals_offset, rows)
        }

        const row_vals_offset = json.processorSpecific.rowMeansBlobOffset
        if (typeof table_vals_offset == 'undefined') {
            this.row_values = null
        } else {
            this.row_values = new Float32Array(msg, row_vals_offset, columns)
        }

        const table_stds_offset = json.processorSpecific.tableStdsBlobOffset
        if (typeof table_stds_offset == 'undefined') {
            this.table_stds = null
        } else {
            this.table_stds = new Float32Array(msg, table_stds_offset, rows)
        }

        const table_mins_offset = json.processorSpecific.tableMinsBlobOffset
        if (typeof table_mins_offset == 'undefined') {
            this.table_mins = null
        } else {
            this.table_mins = new Float32Array(msg, table_mins_offset, rows)
        }

        const table_maxs_offset = json.processorSpecific.tableMaxsBlobOffset
        if (typeof table_maxs_offset == 'undefined') {
            this.table_maxs = null
        } else {
            this.table_maxs = new Float32Array(msg, table_maxs_offset, rows)
        }

        const table_medians_offset = json.processorSpecific.tableMediansBlobOffset
        if (typeof table_medians_offset == 'undefined') {
            this.table_medians = null
        } else {
            this.table_medians = new Float32Array(msg, table_medians_offset, rows)
        }

    }

    // Called when needs to re-render
    on_render() {
        this.datarevision++

        if (this.zValues == null) {
            Plotly.purge(this.heatmap_div_name)
            return
        }
        this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)


        // Heatmap table stats (Render this before plotly heatmap since it is variable-size. Then heatmap will know how much space it has to fill)
        // Calculate height because I can't seem to get the table to expand its cells to hit a target percentage height
        // Subtract some space for the footer to match the heatmap and per-table chart labels/title
        if (this._stats_collapsed) {
            this.stat_div.html('') // clear everything

            const el = $(this.element).find('#' + this.heatmap_info_name)
            if (el.find('.expand-collapse-toggle').length == 0) {
                const btn = $(`<div style="position: absolute; bottom: 0px; left:0px;" class="expand-collapse-toggle left" title="show the stats panel"><div class="inside-line-left"></div><span class="ui-icon ui-icon-triangle-1-e"></span></div>`)
                el.append(btn)
                btn.on('click', () => {
                    this._stats_collapsed = false
                    this._make_layouts()
                    viewer.refresh_widget(this, {force_no_sync: true})
                })
            }
        } else {
            const td_height = (this.stat_div.height() - 100) / (this.shape.ntables * this.shape.nbanks)
            let td_height_err = 0

            // Colorize stat columns
            let min_range = null
            let max_range = null
            let std_range = null
            let mean_range = null
            let median_range = null
            let first = true
            for (let t = this.shape.ntables - 1; t >= 0; t--) {
                for (let b = this.shape.nbanks - 1; b >= 0; b--) {
                    const idx = b + this.shape.nbanks * t
                    if (first) {
                        min_range =    [this.table_mins[idx],      this.table_mins[idx]   ]
                        max_range =    [this.table_maxs[idx],      this.table_maxs[idx]   ]
                        std_range =    [this.table_stds[idx],      this.table_stds[idx]   ]
                        mean_range =   [this.table_means[idx],     this.table_means[idx]  ]
                        median_range = [this.table_medians[idx],   this.table_medians[idx]]
                        first = false
                    } else {
                        min_range =    [Math.min(this.table_mins[idx], min_range[0]),       Math.max(this.table_mins[idx], min_range[1])      ]
                        max_range =    [Math.min(this.table_maxs[idx], max_range[0]),       Math.max(this.table_maxs[idx], max_range[1])      ]
                        std_range =    [Math.min(this.table_stds[idx], std_range[0]),       Math.max(this.table_stds[idx], std_range[1])      ]
                        mean_range =   [Math.min(this.table_means[idx], mean_range[0]),     Math.max(this.table_means[idx], mean_range[1])    ]
                        median_range = [Math.min(this.table_medians[idx], median_range[0]), Math.max(this.table_medians[idx], median_range[1])]
                    }
                }
            }

            // Define some methods that spit out a color based on the value and the range associated with that stat
            function make_make_color(stat_range) {
                function make_color(v) {
                    const range = stat_range[1] - stat_range[0]
                    const v2 = v - stat_range[0]
                    const c = interp_color([255,240,240], [255,100,100], Math.max(0,Math.min(1,(v2/range))))
                    return colorToHex(c)
                }
                return make_color
            }

            const get_min_color = make_make_color(min_range)
            const get_max_color = make_make_color(max_range)
            const get_std_color = make_make_color(std_range)
            const get_mean_color = make_make_color(mean_range)
            const get_median_color = make_make_color(median_range)

            // Display values the same way we order values for the heatmap itself
            const table = $(`<table class="bp-heatmap-table-stats" cellpadding=0 cellspacing=0></table>`)
            for (let t = this.shape.ntables - 1; t >= 0; t--) {
                for (let b = this.shape.nbanks - 1; b >= 0; b--) {
                    const idx = b + this.shape.nbanks * t
                    const min = this.table_mins[idx]
                    const max = this.table_maxs[idx]
                    const mean = this.table_means[idx]
                    const std = this.table_stds[idx]
                    const median = this.table_medians[idx]

                    const floor_h = Math.floor(td_height)
                    let h = floor_h
                    td_height_err += td_height - h
                    if (td_height_err >= 1) {
                        td_height_err -= 1
                        h += 1
                    }

                    const min_color = get_min_color(min)
                    const max_color = get_max_color(max)
                    const std_color = get_std_color(std)
                    const mean_color = get_mean_color(mean)
                    const median_color = get_median_color(median)

                    const style = ` font-size: ${floor_h}px; height: ${h}px; `
                    const lbl = `t${t.toString().padStart(2,'0')}b${b}`
                    table.append($(`<tr><td style="${style}">${lbl}</td>
                                        <td style="${style} background-color:${min_color};">${min.toFixed(2)}</td>
                                        <td style="${style} background-color:${max_color};">${max.toFixed(2)}</td>
                                        <td style="${style} background-color:${mean_color};">${mean.toFixed(2)}</td>
                                        <td style="${style} background-color:${median_color};">${median.toFixed(2)}</td>
                                        <td style="${style} background-color:${std_color};">${std.toFixed(2)}</td>
                                    </tr>`))
                }
            }
            table.append($(`<tr><th>table</th><th>min</th><th>max</th><th>mean</th><th>median</th><th>std</th></tr>`))

            this.stat_div.html(table)
            this.stat_div.append($(`<span>per-shp-table stats</span>`))

            const el = $(this.element).find('#' + this.heatmap_info_name)
            if (el.find('.expand-collapse-toggle').length == 0) {
                const btn = $(`<div style="position: absolute; bottom: 0px; left: ${this.stat_div.width()};" class="expand-collapse-toggle right" title="collapse the stats panel"><div class="inside-line-left"></div><span class="ui-icon ui-icon-triangle-1-w"></span></div>`)
                el.append(btn)
                btn.on('click', () => {
                    this._stats_collapsed = true
                    this._make_layouts()
                    this.viewer.refresh_widget(this, {force_no_sync: true})
                })
            }
        }

        // Heatmap grid
        var data = [{
          x: this.shape.xValues,
          y: this.shape.yValues,
          z: this.zValues,
          type: 'heatmap', //'heatmapgl' causes blurring of cells and does not support ygap. use 'heatmap'
          colorscale: ColorScaleGradientBlueGreenRed,
          ygap: 1,
          // xgap: 1, // miss things when zoomed out fully. Looks great when zoomed in though.
          //showscale: false
        }];

        this.layout['datarevision'] = this.datarevision;

        const plot_div = $(this.element).find('#' + this.heatmap_div_name)
        const plot_existed = plot_div.html() != ''

        Plotly.react(this.heatmap_div_name, data, this.layout, {displaylogo: false, modeBarButtonsToRemove: HeatMapWidget.plotly_buttons_to_remove});

        if (!plot_existed) {
            plot_div[0].on('plotly_click', (data) => {

                const t = performance.now()
                const dt = t - this._last_click
                this._last_click = t
                const timeout = 300 // how long user has to double-click (ms)
                if (dt < timeout) {
                    // Double click. Let plotly handle this
                    if (this._last_click_timeout != null) {
                        clearTimeout(this._last_click_timeout)
                        this._last_click_timeout = null
                    }
                } else {
                    const pt = data.points[0]
                    pt.x // row
                    pt.y // bank and table
                    pt.z // stat value

                    // Set a timer to be cancelled if a double-click happens. Otherwise, activate the dialog
                    this._last_click_timeout = setTimeout(() => {
                        if (data.points.length < 0)
                            return

                        const [bank, table] = this.shape.get_bank_and_table(pt.y)
                        const row = pt.x

                        // See which widgets are applicable
                        const filter = (pwidget) => { return pwidget.typename == BranchProfileTableWidget.typename || pwidget.typename == BranchHistoryTableWidget.typename }
                        const applicable_widgets = this.viewer.pwidgets.filter(filter)

                        if (applicable_widgets.length == 0) {
                            // Create one and track it as the "latest"
                            const new_widget = this.viewer.add_new_widget_from_factory(BranchProfileTableWidget, {data_id:this.data_id})
                            new_widget.geometry_filters.apply_config_data({ bank: bank, table: table, row: row })

                            // TODO: refreshing the widget here does not work!
                            this.viewer.refresh_widget(new_widget, {force_sync: true}) // Refresh with current state (i.e. do not change time range)
                            this._cell_filter_dlg._last_applied = [new_widget]
                        } else {
                            this._cell_filter_dlg.show(applicable_widgets, bank, table, row)

                            // Show a modal dialog for selecting the profile/trace view that you want to apply the selection to.
                        }
                    }, timeout)
                }
            })

        }


        if (this.table_means == null) {
            Plotly.purge(this.heatmap_table_chart_name)
        } else {
            // Table profile (left side)
            const mean_data = {
                x: this.table_means,
                y: this.shape.yValues,
                type: 'line',
                colorscale: ColorScaleGradientBlueGreenRed
            }

            const dummy_data = {
                x: [0],
                y: [0],
                type: 'markers'
            }

            // Chart of table means next to the heatmap
            this.table_chart_layout['datarevision'] = this.datarevision
            Plotly.react(this.heatmap_table_chart_name, [mean_data, dummy_data], this.table_chart_layout, {displaylogo: false, modeBarButtonsToRemove: HeatMapWidget.plotly_buttons_to_remove});
        }

        if (this.row_values == null) {
            Plotly.purge(this.heatmap_row_chart_name)
        } else {
            // Row profile (top)
            var per_row_table_data = {
                x: this.shape.xValues,
                y: this.row_values,
                type: 'scattergl', //type: 'bar' is intense to draw
                mode: 'lines',
                //marker: {
                //    size:4,
                //    colorscale: colorScaleGradient,
                //}
                line: {
                    width: 0.5,
                }
            }

            // TODO: these should be subplots
            this.row_chart_layout['datarevision'] = this.datarevision;
            Plotly.react(this.heatmap_row_chart_name, [per_row_table_data], this.row_chart_layout, {displaylogo: false, modeBarButtonsToRemove: HeatMapWidget.plotly_buttons_to_remove});
        }
    }

    get_request_kwargs() {
        return { stat_col: this._stat_full_name,
                 allow_bins: true, // Setting this to false can be used as a work-around having the processor loaded for the wrong stats
                 allow_threads: false,
                 mode: this._heatmap_mode,}
    }

    // New data-source assigned
    on_assign_data() {
        const shape = this.data_source.type_specific.shape
        this.shape = new HeatMapShape(shape.table.max+1, shape.row.max+1, shape.bank.max+1)

        // Reset data arrays to use new shape
        this.zData = this.shape.make_zero_array()
        this.zValues = null

        this._make_layouts() // Regenerate layout because of heatmap shape-change
    }

    // Assign a new stat to this heatmap and perform the appropriate actions to deal with it
    _assign_stat(stat_name, stat_full_name) {
        if (this.data_id == null)
            return // No data associated yet. Adding this stat is premature

        this._stat = stat_name
        this._stat_full_name = stat_full_name

        this._update_modes_allowed()
        this._fix_current_mode()

        // Generate the layouts after change in stat
        this._make_layouts()

        // Also need to regenerate the processor because of how heatmap generators work - they require some
        // precalculation of bins to optimize the heatmap queries and (for memory & load-time reasons) only precalculate
        // these bins for the stats we know we want to use.
        // Calling this now will cause the widget/viewer to fetch our stat info through get_processor_kwargs, which
        // will be updated to include the above stat name change.
        this._reassign_data(this.data_id)

        // Ask for a refresh to redraw with the new stat (this is probably redundant with the _reassign_data)
        this.viewer.refresh_widget(this)
    }

    // Enable/disable mode options as appropriate based on current stat type
    _update_modes_allowed() {
        if (this.data_source == null)
            return // too early

        const stat_type = this.data_source.get_stat_info(this._stat_full_name).type || StatTypes.STAT_TYPE_ATTRIBUTE
        const is_stat_delta = stat_type == StatTypes.STAT_TYPE_SUMMABLE

        const select = $('#' + this.heatmap_info_name).find('#bp-heatmap-select-mode')
        const options = select.find('option')
        for (const _opt of options) {
            const opt = $(_opt)
            if (is_stat_delta) {
                opt.attr('disabled', opt.val() != HEATMAP_MODE.SUM ? 'disabled' : null)
            } else {
                opt.attr('disabled', opt.val() == HEATMAP_MODE.SUM ? 'disabled' : null)
            }

            //opt.prop('selected', this._heatmap_mode == opt.val())
        }
    }

    // Update selected mode if it is disabled. _update_modes_allowed() should be called prior to this
    _fix_current_mode() {
        const select = $('#' + this.heatmap_info_name).find('#bp-heatmap-select-mode')
        const options = select.find('option')
        for (const _opt of options) {
            const opt = $(_opt)
            if (opt.prop('selected')) {
                if (opt.attr('disabled') == 'disabled') {
                    // This selected item is disabled and must be replaced with another option that is still enabled
                    opt.prop('selected', false)
                    for (const _opt2 of options) {
                        const opt2 = $(_opt2)
                        if (opt2.attr('disabled') != 'disabled') {
                            this._heatmap_mode = opt2.val()
                            opt2.prop('selected', true) // select this option as a replacement
                            break
                        }
                    }
                } else {
                    // Ensure right mode is selected now!
                    this._heatmap_mode = opt.val()
                }
                break
            }
        }
    }

    // Rebuild some html stuff related to the layout of the widget, current stat, heatmap mode, etc.
    // This is called when the data-source, stat, or the widget layout changes, so it doesn't have to be that efficient.
    _make_layouts() {
        // Update some display fields
        const info_div = $('#' + this.heatmap_info_name)
        info_div.html('')

        // Generate the display options
        const select = $(`<select id="bp-heatmap-select-mode" style="font-size: 12px; border-color: #bbb; height:16px;" title="Some options may be disabled based on the type of stat selected. For instance, summable (delta) stats can be summed while counters should be viewed as a first/last value or a diff (last-first)"></select>`)
        select.append(`<option value="${HEATMAP_MODE.SUM}" ${this._heatmap_mode==HEATMAP_MODE.SUM ? 'selected' : ''}>sum over range</option>`)
        select.append(`<option value="${HEATMAP_MODE.DIFF}" ${this._heatmap_mode==HEATMAP_MODE.DIFF ? 'selected' : ''}>last - first</option>`)
        select.append(`<option value="${HEATMAP_MODE.LAST}" ${this._heatmap_mode==HEATMAP_MODE.LAST ? 'selected' : ''}>last value</option>`)
        select.append(`<option value="${HEATMAP_MODE.FIRST}" ${this._heatmap_mode==HEATMAP_MODE.FIRST ? 'selected' : ''}>first value</option>`)

        info_div.append($(`<span class="smalltext">Heatmap Type:</span>`))
        info_div.append($(`<br/>`))
        info_div.append(select)
        info_div.append($(`<br/>`))

        // Ensure the modes are disabled where appropriate
        this._update_modes_allowed()

        // Register for a mode-change
        select.on('change', (e) => {
            this._heatmap_mode = e.target.value // assign new mode
            viewer.refresh_widget(this, {force_no_sync: true})
        })

        // Stat info
        const stat_type = this.data_source != null ? (this.data_source.get_stat_info(this._stat_full_name).type || StatTypes.STAT_TYPE_ATTRIBUTE) : '?'
        info_div.append($(`<span class="smalltext">Displaying Stat:</span>`))
        info_div.append($(`<br/>`))
        info_div.append($(`<span>"${this._stat}"</span>`))
        info_div.append($(`<span class="tinytext"> (${stat_type} stat)</span>`))


        // Generate some layouts for the plots
        this.layout = this._make_layout()
        this.table_chart_layout = this._make_table_chart_layout()
        this.row_chart_layout = this._make_row_chart_layout()
    }

    _make_layout() {
        const ticktext = []
        const tickvals = []
        for (let table = 0; table < this.shape.ntables; table++) {
            for (let bank = 0; bank < this.shape.nbanks; bank++) {
                ticktext.push(`t${table}b${bank}`)
                tickvals.push(table * this.shape.nbanks + bank)
            }
        }

        return {
          title: '',
          //annotations: [],
          //width: 500,
          autosize: true, // allows scaling to fit space. This does not seem to make things smaller
          //height: tableAxisHeight,
          margin: { t:0, l:HeatMapWidget.heatmapLeftMargin, r:HeatMapWidget.heatmapRightMargin, b:HeatMapWidget.bottomMargin},
          xaxis: {
            autorange: true, // gets near-pixel precision and less blurring compared to manual range
            //autorange: false, // performance
            //range: [0, this.shape.xdim], // performance
            //title: 'row',
            ticks: '',
            side: 'bottom'
          },
          yaxis: {
            autorange: true, // gets near-pixel precision and less blurring compared to manual range
            //autorange: false, // performance
            //range: [-0.5, this.shape.ydim+0.5], // performance
            //title: 'table index',
            ticks: '',
            ticksuffix: ' ',
            ticktext: ticktext,
            tickvals: tickvals,
          },
          font: {
            size: 10,
          },
        }
    }

    // Side table-chart layout
    _make_table_chart_layout() {
        return {
          autosize: true,
          showlegend: false,
          title: 'Table Means',
          //annotations: [],
          //width: 500,
          //height: HeatMapWidget.tableAxisHeight,
          margin: { t:0, l:20, r:0, b:HeatMapWidget.bottomMargin},
          xaxis: {
            tickangle: 0,
            autorange: 'reversed',
            title: '',
            ticks: '',
            side: 'bottom'
          },
          yaxis: {
            autorange: false, // performance
            range: [-0.5, this.shape.ydim-0.25], // performance (and fixes alignment problems)
            title: 'table index (table# x nbanks + bank#)',
            showticklabels: false,
            //width: 700,
            //height: 700,
          }
        }
    }

    // Top row-chart layout
    _make_row_chart_layout() {
        return {
          autosize: true,
          showlegend: false,
          title: '',
          //annotations: [],
          //width: 500,
          //height: 100,
          margin: { t:0, l:HeatMapWidget.heatmapLeftMargin, r:HeatMapWidget.heatmapRightMargin, b:0},
          xaxis: {
            visible: false,
            autorange: false,
            range: [-0.5, this.shape.xdim+0.5],
            title: '',
            autotick: true,
            ticks: '',
            side: 'bottom'
          },
          yaxis: {
            autorange: true,
            title: '',
            ticks: '',
            ticksuffix: ' ',
            //width: 700,
            //height: 700,
            autosize: true
          }
      }
    }
}

// Constant layout parameters
HeatMapWidget.tableAxisHeight = 350
HeatMapWidget.heatmapRightMargin = 120
HeatMapWidget.heatmapLeftMargin = 40
HeatMapWidget.bottomMargin = 15

HeatMapWidget.typename = 'bp-heatmap'
HeatMapWidget.description = 'A branch predictor heatmap'
HeatMapWidget.processor_type = Processors.SHP_HEATMAP
HeatMapWidget.data_type = DataTypes.SHP_TRAINING_TRACE
HeatMapWidget.plotly_buttons_to_remove = ['editInChartStudio','sendDataToCloud','zoom2d','pan2d','lasso2d','zoomIn2d','zoomOut2d','autoScale2d']
