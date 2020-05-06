// Describes a simple lineplot widget

// Manages active/free colors
class ColorManager {
    constructor() {
        const p = palette('mpn65',64)
        this.colors = p.map((x)=> [x,false]) // all unused

        this.used_colors = new Set()
    }

    // Take a color
    take() {
        const color = this.colors[0]
        for (let i = 0; i < this.colors.length; i++) {
            if (this.colors[i][1] == false) {
                this.colors[i][1] = true
                return this.colors[i][0]
            }

        }
        return '' // no colors left
    }

    // Free a color back to the manager
    free(color) {
        if (this.used_colors.has(color)) {
            this.used_colors.delete(color)
            for (let i = 0; i < this.colors.length; i++) {
                if (this.colors[i][0] == color) {
                    this.colors[i][1] = false // unused
                }
            }
        }
    }
}


// Simple (abstract) lineplot widget. Do not use this directly
class SimpleLinePlotWidget extends BaseWidget {
    constructor(el, id) {
        super(el, id, 250)
        this.plot_div_name = this.name + '-lineplot-div-' + id.toString()
        this.legend_div_name = this.name + '-lineplot-legend-div-' + id.toString()
        this.controls_div_name = this.name + '-lineplot-controls-div-' + id.toString()

        // Set up the widget
        // TODO use the class to imply the style
        //this.plot_div = $(`<div id="${this.plot_div_name}" class="" style="width:100%; height:100%;"></div>`)
        this.jbody.html('')
        this.jbody.append(`
            <div style="display: grid; grid-template-columns: 1fr 160px; grid-template-rows: 1fr auto 10px; grid-template-areas: 'plot legend' 'plot controls' 'plot spacer'; width:100%; height:100%;">
                <div id="${this.plot_div_name}" class="" style="grid-area:plot;"></div>
                <div id="${this.controls_div_name}" class="" style="grid-area:controls; border-top: 1px solid #ccc;">
                    <span class="smalltext">points/stat:</span> <span id="points">0</span><br/>
                    <span class="smalltext">downsampling:</span> <span id="downsampling">0</span><br/>
                    <span class="smalltext">annotations:</span> <span id="num-annotations">0</span> <a id="clear-annotations" href="javascript:void(0);" class="tinytext">(clear)</a>
                    <span class="tinytext" title="Annotations cannot currently be saved since they are specific to a single data-set">(annotations are not saved)</span>
                </div>
                <div id="${this.legend_div_name}" style="grid-area:legend; overflow-y: auto;"></div>
                <div style="grid-area:spacer;"><!-- this spacer is here to not block the resize handle for the widget with legend/controls content --></div>
            </div>
        `)
        this.plot_div = this.jbody.find('#' + this.plot_div_name)
        this.controls_div = this.jbody.find('#' + this.controls_div_name)
        this.legend_div = this.jbody.find('#' + this.legend_div_name)
        this.num_annotations = this.controls_div.find('#num-annotations')
        this.downsampling_span = this.controls_div.find('#downsampling')
        this.points_span = this.controls_div.find('#points')
        this.controls_div.find('#clear-annotations').on('click', () => {
            this._clear_annotations()
        })

        this.layout = this._make_layout()

        // Initial data
        this.series_time = null
        this.y_extents = [-1,1]

        this.cmgr = new ColorManager

        this._stat_cols = []
        this._stat_colors = [] // TODO: should save in layout. Would also need to update the color manager with already-used colors
        this._unplotted_annotations = [] // annotations that have not yet been applied to the plot
    }

    _make_layout() {
        return {
            autosize: true, // allows scaling to fit space
            //height: 150,

            showlegend: false,
            title: '',
            // TODO: grab these from the UI layout (or have the ui pass them in)
            margin: { t:10,
                      l:50, // roughly match td to the left of time slider
                      r:0, // legend will be manual to the right
                      b:20},
            xaxis: {
                autorange: false,
                title: '',
                ticks: '',
                side: 'bottom'
            },
            yaxis: {
                autorange: true,
                title: 'value',
                ticks: '',
                ticksuffix: ' ',
                autosize: true
            },
            font: {
                size: 10,
            },

            dragmode: 'pan', // default to pan as default control (panning is faster zooming for backend, so discourage zoom)
        }
    }

    // Someone asked us to clear stats
    on_clear_stats() {
        this._stat_cols = []
        this._stat_colors = {}
        this.cmgr = new ColorManager()
    }

    // Add stats, skipping duplicates, and refresh
    _add_new_stats(new_stats) {
        // Prevent duplicate stats
        for (const stat_item of new_stats) {
            if (!list_contains(this._stat_cols, stat_item)) {
                this._stat_cols.push(stat_item)
                this._stat_colors[stat_item] = this.cmgr.take()
            }
        }

        // Ask for a refresh to redraw with the new stat
        this.viewer.refresh_widget(this)
    }

    // Someone dropped a new stat on us
    on_add_stat(new_stat_name) {
        this._add_new_stats([new_stat_name])
    }

    // Called upon start of widget resize
    on_resize_started() {
    }

    // Called upon end of widget resize
    on_resize_complete() {
        // Hide the control/info panel below a certain widget height
        if ($(this.element).height() < 200) {
            this.controls_div.css({display: 'none', 'grid-area': ''})
        } else {
            this.controls_div.css({display: '', 'grid-area': 'controls'})
        }
        this.render()
    }

    // (Save) Get state to be stored in the layout file. This will be given back to apply_config_data later.
    get_config_data() {
        return { stat_cols: this._stat_cols,
               }
    }

    // (Load) Apply a dictionary of configuration data specific to this type of widget
    apply_config_data(d) {
        this._stat_cols = d.stat_cols
        this._stat_colors = {}
        for (const stat of this._stat_cols) {
            this._stat_colors[stat] = this.cmgr.take()
        }
    }

    // New data was received from the server
    on_update_data(json) {
        if (json == null) {
            this.series_time = null
            this.series = {}
            this.instruction_nums = []
            this.indices = []
            this.addresses = []
            this.targets = []
            this.modality = {}
            this.y_extents = [-1,1]
            this.downsampling = -1
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            this.downsampling_span.html('?')
            this.points_span.html('0')
            return
        }

        const msg = json.raw_binary_data

        const nseries = json.processorSpecific.numSeries
        const points_per_series = json.processorSpecific.pointsPerSeries
        const series_blob_offset = json.processorSpecific.seriesBlobOffset
        const modalities_offset = json.processorSpecific.modalitiesOffset
        this.downsampling = json.processorSpecific.downsampling
            const instruction_nums_offset = json.processorSpecific.instructionNumsOffset
        const indices_offset = json.processorSpecific.indicesOffset
        const addresses_offset = json.processorSpecific.addressesOffset
        const targets_offset = json.processorSpecific.targetsOffset

        this.downsampling_span.html(this.downsampling > 1 ? ('/' + this.downsampling.toString()) : 'none')
        this.points_span.html(points_per_series.toString())

        // Parse out heatmap binary data
        if (nseries == 0 || points_per_series == 0) {
            this.series_time = null
            this.series = {}
            this.instruction_nums = []
            this.indices = []
            this.addresses = []
            this.targets = []
            this.modality = {}
            this.downsampling = -1
            this.y_extents = [-1,1]
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            return
        }

        const data = new Float32Array(msg, series_blob_offset, nseries * points_per_series) // Skip dims
        const modalities = new Int16Array(msg, modalities_offset, nseries)
        this.series_time = data.subarray(0, points_per_series)
        this.series = {}
        this.modality = {}
        this.y_extents = json.processorSpecific.yExtents
        for (let i = 0; i < this._stat_cols.length; i++) {
            const begin = (i + 1) * points_per_series
            const end = begin + points_per_series
            this.series[this._stat_cols[i]] = data.subarray(begin, end)
            this.modality[this._stat_cols[i]] = modalities[i + 1]
        }


        // Capture some bp-specific stuff (this should be moved to the bp widget instead)
        if (typeof instruction_nums_offset != 'undefined')
            this.instruction_nums = new BigInt64Array(msg, instruction_nums_offset, points_per_series)
        else
            this.instruction_nums = []
        if (typeof indices_offset != 'undefined')
            this.indices = new BigInt64Array(msg, indices_offset, points_per_series)
        else
            this.indices = []
        if (typeof addresses_offset != 'undefined')
            this.addresses = new BigInt64Array(msg, addresses_offset, points_per_series)
        else
            this.addresses = []
        if (typeof targets_offset != 'undefined')
            this.targets = new BigInt64Array(msg, targets_offset, points_per_series)
        else
            this.targets = []
    }

    _update_plot(data) {
        // See if plot existed already and if not
        const plot_existed = this.plot_div.html() != ''

        Plotly.react(this.plot_div[0], data, this.layout, {displaylogo: false, modeBarButtonsToRemove: SimpleLinePlotWidget.plotly_buttons_to_remove, scrollZoom: true})

        if (!plot_existed) {
            // Capture layout events (e.g. box selection) to drive the global slider
            this.plot_div[0].on('plotly_relayout', (evt) => {
                if (typeof evt['xaxis.range[0]'] != 'undefined') {
                    const first = parseInt(evt['xaxis.range[0]'])
                    const last = parseInt(evt['xaxis.range[1]'])
                    if (this.should_follow_global()) {
                        this.viewer.widget_chose_time(this, first, last)
                    } else {
                        this._request_custom_update(first, last) // Flag as needing a custom update for last range
                    }
                    this.plot_div[0].layout.yaxis.autorange = true // Have to re-enable because zooming scales y data with an explicit range
                } else {
                    // Could be an annotation change
                }

                return false;

            })

            // Handle legend clicks (not perfect - cannot intercept them)
            this.plot_div[0].on('plotly_restyle', (evt) => {
                // On a legend click, remote the item
                if (evt[0].visible[0] == 'legendonly') { // Legend click/change?
                    const indices = evt[1]
                    let num_deleted = 0
                    for (const i of indices) { // note: assuming indices are sorted ascending
                        const stat = this._stat_cols[i - num_deleted]
                        this.cmgr.free(this._stat_colors[stat])
                        this._stat_cols.splice(i - num_deleted,1)
                        num_deleted++ // Compensate for removed items
                    }
                    if (num_deleted > 0) {
                        this.viewer.refresh_widget(this) // Request refresh with removed data
                    }
                }
            })

            // TODO: Figure out what the actually show when hovering
            this.plot_div[0].on('plotly_hover', (data) => {
                if (this.downsampling > 1) {
                    return // No hover until zoomed in close enough that there is no downsampling
                }

                if ((data.points || []).length > 0) {
                    const point = data.points[0]

                    // bp hovering. TODO: Move this to bp line plot subclasss
                    if (this.instruction_nums.length > 0) {
                        const inst_id = this.instruction_nums[point.pointIndex]
                        const branch_index = this.indices[point.pointIndex]
                        const addr = this.addresses[point.pointIndex]
                        const tgt = this.targets[point.pointIndex]

                        const grid = this.plot_div.find('g.gridlayer')[0]
                        const rect = grid.getBoundingClientRect()
                        const x_offset = -5
                        const y_offset = 5
                        const x = point.xaxis.d2p(point.x) + rect.left + x_offset
                        const y = point.yaxis.d2p(point.y) + rect.top + y_offset

                        const content = `inst#:${inst_id}<br>uBrn#:${branch_index}<br>ha:0x${addr.toString(16)}<br>tgt:0x${tgt.toString(16)}`
                        const hover = $(`<div id="bp-line-plot-hover-div" class="smalltext" style="left:0; top:${y};">${content}</div>`)
                        $('body').append(hover)
                        const border_width = parseInt(hover.css('border-width'))
                        hover.css({left: x - hover.width() - 2 * border_width}) // assign left after adding to DOM so we know width
                    }
                }
            })

            this.plot_div[0].on('plotly_unhover', (data) => {
                while ($('#bp-line-plot-hover-div').length > 0)
                    $('#bp-line-plot-hover-div').remove()
            })

            this.plot_div[0].on('plotly_click', (data) => {
                const point = data.points[0]

                // bp hovering. TODO: Move this to bp line plot subclasss
                if (this.instruction_nums.length > 0) {
                    const inst_id = this.instruction_nums[point.pointIndex]
                    const branch_index = this.indices[point.pointIndex]
                    const addr = this.addresses[point.pointIndex]
                    const tgt = this.targets[point.pointIndex]

                    const ay = point.y <= 0.5 ? 60 : -60 // pixels (must be larger than the height of the annotation text)
                    const new_index = (this.plot_div[0].layout.annotations || []).length
                    let removed = false
                    if (new_index > 0) {
                        // Remove if clicked on one that is already there
                        this.plot_div[0].layout.annotations.forEach((ann, idx) => {
                            if (ann.branch_index == branch_index) {
                                Plotly.relayout(this.plot_div[0], `annotations[${idx}]`, 'remove')
                                this.num_annotations.html((this.plot_div[0].layout.annotations || []).length.toString())
                                removed = true
                            }
                        })
                    }
                    if (!removed) {
                        if (this.downsampling > 1) {
                            this.viewer.show_error_popup('Cannot add annotation',
                                                         `Annotations can only be added to line plots when viewing data with no downsampling applied. Downsampling is currently ${this.downsampling}. Zoom in to reduce downsampling.`)
                            return
                        }

                        const new_anno = {
                            x: point.xaxis.d2l(point.x),
                            y: point.yaxis.d2l(point.y),
                            arrowhead: 5,
                            arrowsize: 2,
                            arrowwidth: 1,
                            ax: 0,
                            ay: ay,
                            bgcolor: 'rgba(255,255,255,0.8)',
                            font: {size:11, family:'Courier New, monospace', color:'#404040'},
                            text: `inst#:${inst_id}<br>uBrn#:${branch_index}<br>ha:0x${addr.toString(16)}<br>tgt:0x${tgt.toString(16)}`,
                            branch_index: branch_index,
                            index: new_index,
                            captureevents: true, // allow click events

                        }

                        Plotly.relayout(this.plot_div[0], `annotations[${new_index}]`, new_anno)
                        this.num_annotations.html(this.plot_div[0].layout.annotations.length.toString())
                    }
                }
            })

            this.plot_div[0].on('plotly_clickannotation', (event) => {
                Plotly.relayout(this.plot_div[0], `annotations[${event.index}]`, 'remove')
                this.num_annotations.html((this.plot_div[0].layout.annotations || []).length.toString())
            })

        }
    }

    // Called when needs to re-render
    on_render() {
        // Detect no-data condition
        if (this.series_time == null) {
            this._update_plot([]) // draw current range with no data
            return
        }

        // Set a new explicit range for the plot based on the latest update for this widget.
        // Do not use the data we are given because it could be a subset if some was not applicable.
        const last_range = this.get_last_update()
        this.layout.xaxis.range = [last_range.first, last_range.last]
        const range_length = last_range.last - last_range.first + 1

        // Draw some grey boxes beside the data so when panning it is clear that this area is "out of bounds"
        this.layout.shapes = []
        this.layout.shapes.push({
            type: 'rect',
            xref: 'x',
            yref: 'y',
            x0: last_range.first - 1 - range_length,
            x1: last_range.first - 1,
            y0: this.y_extents[0],
            y1: this.y_extents[1],
            line: {
                width: 0,
            },
            fillcolor: 'rgba(0,0,0,.1)',
        })
        this.layout.shapes.push({
            type: 'rect',
            xref: 'x',
            yref: 'y',
            x0: last_range.last + 1,
            x1: last_range.last + 1 + range_length,
            y0: this.y_extents[0],
            y1: this.y_extents[1],
            line: {
                width: 0,
            },
            fillcolor: 'rgba(0,0,0,.1)',
        })

        // Now that there is data, clear status warnings
        this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)

        // Grab the x axis
        const xs = this.series_time

        this.legend_div.html('')

        // Grab series value arrays and make them plotly plot items
        const data = []
        const lineWidth = 0.6 // thickness of lines (<1 implies transparency)
        for (const stat of this._stat_cols) {
            // Names that are too long shrink the plot area and cause misalignment between stacked line plots
            const display_name = make_string_length(stat, 24)
            const color = this._stat_colors[stat]

            // Choose a size for markers based on the number of points!
            // NOTE: This should be based on screen size, but it works ok without it
            const marker_size = xs.length < 50
                                 ? 6
                                 : xs.length < 200
                                    ? 4
                                    : xs.length < 500
                                       ? 3
                                       : 2

            // Special visualization for branch correctness only since it is a bit expensive and doesn't look good
            // if more than one view is like this.
            // NOTE: The length limit here is important, otherwise this takes too long to do.
            let legend_icon
            const fully_zoomed_and_spiky = this.downsampling <= 1 && this.modality[stat] <= 3 && this.modality[stat] != -1 && xs.length < 1000
            const ys = this.series[stat]
            if (this._stat_cols.length == 1 && stat == 'correct' && fully_zoomed_and_spiky) { // no downsampling (can be 0 or 1 depending on calculation)

                // Turn the points to vertical ticks to make correct/incorrect density more apparant as we zoom in
                const new_xs = []
                const new_ys = []
                const new_ys2 = []
                for (let i = 0; i < xs.length; i++) {
                    const x = xs[i]
                    const y = ys[i]
                    new_xs.push(x,  x,null)
                    new_ys.push(0.5,y,null)
                }

                // Add markers first since this is what we'll be adding annotations on (series [0])
                data.push({name: display_name, x: xs, y: ys, mode: 'markers', line: { width:lineWidth, color:color }, marker: { size: marker_size }})
                data.push({name: '', x: new_xs, y: new_ys, mode: 'lines', line: { width:lineWidth, color:color }, showlegend: false, opacity: 0.5})

                //data.push({name: display_name, x: xs, y: this.series[stat], mode: 'lines', line: { width:lineWidth, color:color }, showlegend: false})

                legend_icon = `<div style="background-color:${color}; width:3px; height: 3px; display:inline-block; margin-bottom:2px; margin-left: 5px; margin-right: 5px"></div>`
            } else if (fully_zoomed_and_spiky) {
                data.push({name: display_name, x: xs, y: ys, mode: 'lines+markers', line: { width:lineWidth, color:color }, marker: { size: marker_size }})

                legend_icon = `<div style="background-color:${color}; width:12px; height: 2px; display:inline-block; margin-bottom:3px;"></div>`
            } else {
                data.push({name: display_name, x: xs, y: ys, mode: 'lines', line: { width:lineWidth, color:color }})

                legend_icon = `<div style="background-color:${color}; width:12px; height: 2px; display:inline-block; margin-bottom:3px;"></div>`
            }

            // Build the legend entry
            const legend_item = $(`<div style="color: ${color}; border-bottom: 1px solid #eee; ">
                                        <div class="no-margin-no-padding" style="width: calc(100% - 24px); display: inline-block;"> ${legend_icon} ${display_name.replace(/\./g, '.<wbr/>')} </div>
                                        <div class="no-margin-no-padding" style="width: 20px; color: #aaa; display: inline-block; text-align: right;">
                                            <a href="javascript:void(0);" class="stealth-link">[x]</a>
                                        </div>
                                   </div>`)
            this.legend_div.append(legend_item)
            const anchor = legend_item.find('a')
            anchor.on('click', () => {
                // Remove this stat
                this.cmgr.free(this._stat_colors[stat])
                const idx = this._stat_cols.indexOf(stat)
                if (idx >= 0) {
                    this._stat_cols.splice(idx, 1)
                    this.viewer.refresh_widget(this) // Request refresh with removed data
                }
            })
        }


        // Update plot
        this._update_plot(data)
        //Plotly.react(this.plot_div[0], data, this.layout, {displaylogo: false, modeBarButtonsToRemove: SimpleLinePlotWidget.plotly_buttons_to_remove, scrollZoom: true})
    }

    // kwargs to pass to processor function when we make our requests
    get_request_kwargs() {
        const absolute_max_points_per_series = 5000
        const absolute_min_points_per_series = 400
        const excess_stat_cols = Math.max(0, this._stat_cols.length - 4)
        const max_points_per_series = Math.max(absolute_min_points_per_series, absolute_max_points_per_series - (1000 * excess_stat_cols)) // Reduce number of points for excessive columns
        return { stat_cols: this._stat_cols, // TODO: use real stats from this.data_source
                 max_points_per_series : max_points_per_series, // Limit points... Without this, number of points are impossibly large
                }
    }

    _clear_annotations() {
        const annotations = this.plot_div[0].layout.annotations || []
        while (annotations.length > 0) {
            Plotly.relayout(this.plot_div[0], `annotations[0]`, 'remove')
        }
        this.num_annotations.html((this.plot_div[0].layout.annotations || []).length.toString())
    }

}

// Omitting ['zoom2d','pan2d','resetScale2d','zoomOut2d']
SimpleLinePlotWidget.plotly_buttons_to_remove = ['editInChartStudio','sendDataToCloud','lasso2d','zoomIn2d','autoScale2d',]
