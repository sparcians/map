// Describes a SimDB correlation widget, a collection of Bokeh widgets
function range(start, end) {
    return Array(end - start + 1).fill().map((_, idx) => start + idx)
}

Array.range = (start, end) =>
    Array.from({length: (end - start)}, (v, k) => k + start)

class SimDbCorrelationWidget extends BaseWidget {
    constructor(el, id) {
        super(el, id, SimDbCorrelationWidget.defaultHeight)
        this.plot_div_name = this.name + '-correlation-div'

        // Set up the widget
        this.plot_div = $(`<div id="${this.plot_div_name}" class="" style="width:100%; height:100%;"></div>`)
        this.jbody.html('')
        this.jbody.append(this.plot_div)

        this.datarevision = 0
        this.layout = this._make_layout()
        this._stat_cols = []

        // Initial data
        this.source = new Bokeh.ColumnDataSource({
            data: {
                x: [0, 1],
                y: [0, 1],
                indices: [0, 1]
                }
        })
        this.horzPanelSource = new Bokeh.ColumnDataSource({
            data: {
                bottom: [0, 0],
                left: [0, 1],
                right: [1, 2],
                top: [0, 1]
            }
        })
        this.horzHistSource = new Bokeh.ColumnDataSource({
            data: {
                bottom: [0, 0],
                left: [0, 1],
                right: [1, 2],
                top: [0, 1]
            }
        })
        this.vertPanelSource = new Bokeh.ColumnDataSource({
            data: {
                top: [0, 1],
                bottom: [1, 2],
                right: [0, 1],
                left: [0, 0]
            }
        })
        this.vertHistSource = new Bokeh.ColumnDataSource({
            data: {
                top: [0, 1],
                bottom: [1, 2],
                right: [0, 1],
                left: [0, 0]
            }
        })
        this.regLineSource = new Bokeh.ColumnDataSource({
            data: {
                x: [0, 1],
                y: [0, 1],
                }
        })

        this.wholeSource = new Bokeh.ColumnDataSource({
            data: {
                x: [0, 1],
                y: [0, 1],
                indices: [0, 1],
                }
        })

        this.scatterView = new Bokeh.CDSView({
            source: this.wholeSource,
            filters: [new Bokeh.IndexFilter({
                indices: [...Array(this.source.data.indices.length).keys()]})]
        })
        this.whichChanged = "x"
        this.first = null
        this.last = null
        this.selectWidgetX = null
        this.selectWidgetY = null
        this.box = null
        this.hasAllData = false

        this.p = null
        this.scatter = null
    }

    _make_layout() {
        return {
            autosize: true, // allows scaling to fit space
            // height: 150,

            showlegend: true,
            title: '',
            // TODO: grab these from the UI layout (or have the ui pass them in)
            margin: { t:10,
                      l:50, // roughly match td to the left of time slider
                      r:160, // roughly match window_size textbox
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
        }
    }

    // Someone asked us to clear stats
    on_clear_stats() {
        this._stat_cols = []
    }

    // Add stats, skipping duplicates, and refresh
    _add_new_stats(new_stats) {
        // Prevent duplicate stats
        for (const stat_item of new_stats) {
            if (!list_contains(this._stat_cols, stat_item)) {
                this._stat_cols.push(stat_item)
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
        this.render()
    }

    // (Save) Get state to be stored in the layout file. This will be given back to apply_config_data later.
    get_config_data() {
        return { stat_cols: this._stat_cols }
    }

    // (Load) Apply a dictionary of configuration data specific to this type of widget
    apply_config_data(d) {
        this._stat_cols = d.stat_cols
        while (this._stat_cols.length < 2) {
            this._stat_cols.push(this.data_source.stats[Math.round(Math.random() * this.data_source.stats.length)])
        }
    }

    update_scatterview() {
        var first = this.viewer.time_slider.selection()[0]
        var last = this.viewer.time_slider.selection()[1]
        let firstIndex = this.wholeSource.data.indices.findIndex(function(element) {return element >= first} )
        firstIndex = (firstIndex == -1) ? 0 : firstIndex
        let lastIndex = this.wholeSource.data.indices.findIndex(function(element) {return element >= last} )
        lastIndex = (lastIndex == -1) ? this.wholeSource.data.indices.length - 1 : lastIndex - 1
        this.scatterView.attributes.indices = Array.range(firstIndex, lastIndex)
        this.scatterView.change.emit()
        this.scatter.change.emit()
    }

    // New data was received from the server
    on_update_data(json) {
        if (json == null) {
            this.xVals = null
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            return
        }

        if (!this.hasAllData) {
            // need the whole source? go get it first
            const callback = (obj) => {
                // this returns all the points for this pair of stats, update
                // the array that has all the data and the filter
                const psData = obj.processorSpecific
                let count = 0
                let floatArray = new Float32Array(obj.raw_binary_data)
                let length = parseInt(psData.pointsPerSeries)
                // unpack indices
                let indices = floatArray.slice(count, count + length)
                count += length
                let xVals = floatArray.slice(count, count + length)
                count += length
                // unpack y values
                let yVals = floatArray.slice(count, count + length)

                this.wholeSource.data.x = xVals
                this.wholeSource.data.y = yVals
                this.wholeSource.data.indices = indices
                this.wholeSource.change.emit()
                this.update_scatterview()
            }

            this._request_oob_data(
                callback,
                {
                    all_data: true,
                    constrain: false,
                    kwargs: this.get_request_kwargs()
               })
            this.hasAllData = true
        }

        // just a normal update
        const psData = json.processorSpecific

        this.selectWidgetX.value = this._stat_cols[0] ? this._stat_cols[0] : psData.stat_cols[Math.round(Math.random() * psData.stat_cols.length)]
        this.selectWidgetY.value = this._stat_cols[1] ? this._stat_cols[1] : psData.stat_cols[Math.round(Math.random() * psData.stat_cols.length)]

        this.first = psData.first
        this.last = psData.last
        this.box.left = this.first
        this.box.right = this.last
        this.box.change.emit()

        // unpack the data
        var count = 0

        const floatArray = new Float32Array(json.raw_binary_data)
        var length = parseInt(psData.pointsPerSeries)

        if (length > 0) {
            // mark that new points are recovered
            this.whichChanged = ""
            // unpack indices
            var indices = floatArray.slice(count, count + length)
            count += length
            var xVals = floatArray.slice(count, count + length)
            count += length
            // unpack y values
            var yVals = floatArray.slice(count, count + length)
            count += length

            this.source.data.x = xVals
            this.p.x_range.start = this.p.x_range.reset_start = this.p.x_range.bounds[0] = psData["xMin"] - 1E-6
            this.p.x_range.end = this.p.x_range.reset_end = this.p.x_range.bounds[1] = psData["xMax"] + 1E-6

            this.source.data.y = yVals
            this.p.y_range.start = this.p.y_range.reset_start = this.p.y_range.bounds[0] = psData["yMin"] - 1E-6
            this.p.y_range.end = this.p.y_range.reset_end = this.p.y_range.bounds[1] = psData["yMax"] + 1E-6

            this.source.data.indices = indices
            this.source.change.emit()

        }

        // range update only
        this.update_scatterview()

        // unpack hedges
        length = psData.hEdgesLength
        var hedges = floatArray.slice(count, count + length)
        count += length
        // unpack hhist
        length = psData.hHistLength
        var hhist = floatArray.slice(count, count + length)
        count += length

        this.horzHistSource.data.left = this.horzPanelSource.data.left = hedges.slice(0, -1)
        this.horzHistSource.data.right = this.horzPanelSource.data.right = hedges.slice(1)
        this.horzHistSource.data.top = [...Array(hedges.length - 1).fill(0)]
        this.horzPanelSource.data.top = hhist
        this.horzPanelSource.data.bottom = this.horzHistSource.data.bottom = [...Array(hedges.length - 1).fill(0)]
        this.horzPanelSource.change.emit()

        // unpack vedges
        length = psData.vEdgesLength
        var vedges = floatArray.slice(count, count + length)
        count += length
        // unpack vhist
        length = psData.vHistLength
        this.vertPanelSource.data.right = floatArray.slice(count, count + length)
        count += length

        this.vertPanelSource.data.top = this.vertHistSource.data.top = vedges.slice(1)
        this.vertPanelSource.data.bottom = this.vertHistSource.data.bottom = vedges.slice(0, -1)

        this.vertHistSource.data.right = [...Array(vedges.length - 1).fill(0)]
        this.vertPanelSource.data.left = this.vertHistSource.data.left = [...Array(vedges.length - 1).fill(0)]
        this.vertPanelSource.change.emit()

        // unpack regX
        length = psData.regLength
        this.regLineSource.data.x = floatArray.slice(count, count + length)
        count += length
        // unpack regY
        this.regLineSource.data.y = floatArray.slice(count, count + length)

        this.regLineSource.change.emit()
    }

    // Called when needs to re-render
    on_render() {
        // Detect no-data condition
        if (this._stat_cols.length == 0) {
            Plotly.purge(this.plot_div_name)
            return
        }

        this.datarevision++

        // Now that there is data, clear status warnings
        this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)

        // See if plot existed already and if not
        const plot_div = $(this.element).find('#' + this.plot_div_name)
        const plot_existed = plot_div.html() != ''

        if (!plot_existed && this._stat_cols.length > 0) {
            // TODO need to get actual stats from the first query
            var plt = Bokeh.Plotting
            this.selectWidgetX = new Bokeh.Widgets.Select({
                title: "X-axis Value",
                value: this._stat_cols[0],
                options: this.data_source.stats,
                callback: {
                    execute(obj) {
                        obj.ownerWidget.whichChanged = "x"
                        obj.ownerWidget.hasAllData = false
                        obj.ownerWidget.p.xaxis[0].axis_label = obj.ownerWidget._stat_cols[0] = obj.ownerWidget.selectWidgetX.value
                        obj.ownerWidget._request_custom_update(obj.ownerWidget.first, obj.ownerWidget.last)
                    }
                }
            })
            this.selectWidgetX.ownerWidget = this

            this.selectWidgetY = new Bokeh.Widgets.Select({
                title: "Y-axis Value",
                value: this._stat_cols[1],
                options: this.data_source.stats,
                callback: {
                    execute(obj) {
                        obj.ownerWidget.whichChanged = "y"
                        obj.ownerWidget.hasAllData = false
                        obj.ownerWidget.p.yaxis[0].axis_label = obj.ownerWidget._stat_cols[1] = obj.ownerWidget.selectWidgetY.value
                        obj.ownerWidget._request_custom_update(obj.ownerWidget.first, obj.ownerWidget.last)
                    }
                }
            })
            this.selectWidgetY.ownerWidget = this

            var TOOLS = "reset,save,pan,wheel_zoom,box_zoom,box_select,lasso_select,hover"
            var TS_TOOLS = "reset,save,hover"

            this.p = new plt.figure({
                tools: TOOLS,
                plot_width: 700,
                plot_height: 700,
                min_border: 10,
                min_border_left: 30,
                toolbar_location: "above",
                title: "Comparison",
                output_backend: "webgl",
                x_axis_label: this.selectWidgetX.value,
                y_axis_label: this.selectWidgetY.value})

            this.p.x_range = new Bokeh.Range1d({
                start: Math.min(...this.source.data.x) + 1E-6,
                end: Math.max(...this.source.data.x) - 1E-6,
                bounds: "auto"
                })

            this.p.y_range = new Bokeh.Range1d({
                start: Math.min(...this.source.data.y) + 1E-6,
                end: Math.max(...this.source.data.y) - 1E-6,
                bounds: "auto"
                })

            this.p.background_fill_color = "#fafafa"
            this.p.select(Bokeh.BoxSelectTool)[0].select_every_mousemove = false
            this.p.select(Bokeh.LassoSelectTool)[0].select_every_mousemove = false
            this.p.toolbar.active_scroll = "wheel_zoom"
            // TODO not working at the moment
            this.p.toolbar.active_scroll = this.p.select(Bokeh.WheelZoomTool)

            this.scatter = this.p.scatter(
                    {field: "x"},
                    {field: "y"},
                    {
                        source: this.wholeSource,
                        size: 3,
                        marker: "diamond",
                        color: "navy",
                        alpha: 0.85,
                    })

            this.scatter.view = this.scatterView

            var hh = new plt.figure({toolbar_location: null,
                                 plot_width: this.p.plot_width,
                                 plot_height: 250,
                                 x_range: this.p.x_range,
                                 min_border: 10,
                                 min_border_left: 50,
                                 y_axis_location: "right",
                                 y_axis_label: "count",
                                 output_backend: "webgl",})
            // hh.y_range = "auto"
            hh.xgrid.grid_line_color = null
            hh.yaxis.major_label_orientation = Math.pi / 4
            hh.background_fill_color = "#fafafa"

            hh.quad({left: {field: "left"},
                    bottom: {field: "bottom"},
                    top: {field: "top"},
                    right: {field: "right"},
                    source: this.horzPanelSource,
                    color: "rgb(170,186,215)",
                    line_color: "rgb(144,159,186)"})

            hh.quad({left: {field: "left"},
                    bottom: {field: "bottom"},
                    top: {field: "top"},
                    right: {field: "right"},
                    source: this.horzHistSource,
                    alpha: 0.5,
                    color: "#3A8785",
                    line_color: null})

            var pv = new plt.figure({toolbar_location: null,
                                plot_width: 250,
                                plot_height: this.p.plot_height,
                                y_range: this.p.y_range,
                                min_border: 10,
                                y_axis_location: "right",
                                x_axis_label: "count",
                                output_backend: "webgl"})
            // pv.x_range = "auto"
            pv.ygrid.grid_line_color = null
            pv.xaxis.major_label_orientation = Math.PI / 4
            pv.background_fill_color = "#fafafa"

            pv.quad({left: {field: "left"},
                    right: {field: "right"},
                    top: {field: "top"},
                    bottom: {field: "bottom"},
                    source: this.vertPanelSource,
                    color: "rgb(170,186,215)",
                    line_color: "rgb(144,159,186)"})

            pv.quad({left: {field: "left"},
                    right: {field: "right"},
                    top: {field: "top"},
                    bottom: {field: "bottom"},
                    source: this.vertHistSource,
                    alpha: 0.5,
                    color: "#3A8785",
                    line_color: null})


            this.p.line(
                    {field: "x"},
                    {field: "y"},
                    {
                        source: this.regLineSource,
                        color: "#f44242",
                        line_width:2.5})

            this.box = new Bokeh.BoxAnnotation({fill_alpha: 0.5,
                                               line_alpha: 0.5,
                                               level: "underlay",
                                               // TODO get selected extents from main app
                                               left: this.source.data.indices[0],
                                               right: this.source.data.indices.slice(-1)[0]})

            var ts1 = new plt.figure({plot_width: 500,
                                      plot_height: 225,
                                      title: null,
                                      tools: TS_TOOLS,
                                      // TODO need to figure out which units we're actively using
                                      x_range: new Bokeh.Range1d({start: this.data_source.time_range_by_units.cycles.first,
                                                                  end: this.data_source.time_range_by_units.cycles.last,
                                                                  bounds: "auto"})})
            ts1.active_scroll = "xwheel_zoom"
            ts1.bounds = "auto"

            ts1.line({field: "indices"}, {field: "x"}, {source: this.wholeSource})
            ts1.add_layout(this.box)

            var ts2 = new plt.figure({plot_width: 500,
                                      plot_height: 225,
                                      title: null,
                                      tools: TS_TOOLS,
                                      x_range: ts1.x_range})
            ts2.active_scroll = "xwheel_zoom"
            ts2.bounds = "auto"

            ts2.line({field: "indices"}, {field: "y"}, {source: this.wholeSource})
            ts2.add_layout(this.box)

            // make the layout
            var doc = new Bokeh.Document()
            var newLayout = plt.gridplot([
                [this.p, pv, new Bokeh.Column({children: [this.selectWidgetX, ts1, this.selectWidgetY, ts2]})],
                [hh, new Bokeh.Spacer(), new Bokeh.Spacer()],
                ],
                {
                sizing_mode: "scale_both",
                merge_tools: false})

            doc.add_root(newLayout)

            Bokeh.embed.add_document_standalone(doc, plot_div[0])
        }
    }

    // kwargs to pass to processor function when we make our requests
    get_request_kwargs() {
        return { stat_cols: this._stat_cols,
                 whichChanged: this.whichChanged,
                 max_points_per_series : SimDbCorrelationWidget.max_points_per_series,
                }
    }

}
SimDbCorrelationWidget.defaultHeight = 1000
SimDbCorrelationWidget.max_points_per_series = 4500
SimDbCorrelationWidget.typename = 'sim-correlation-widget'
SimDbCorrelationWidget.description = 'A few plots to help do correlation'
SimDbCorrelationWidget.processor_type = Processors.SIMDB_CORRELATION_DATA
SimDbCorrelationWidget.data_type = DataTypes.SIMDB
SimDbCorrelationWidget.plotly_buttons_to_remove = ['editInChartStudio','sendDataToCloud','zoom2d','pan2d','lasso2d','zoomIn2d','zoomOut2d','autoScale2d','resetScale2d']
