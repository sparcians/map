// Describes a SimDB correlation widget, a collection of Bokeh widgets

function range(start, end) {
    return Array(end - start + 1).fill().map((_, idx) => start + idx);
}

Array.range = (start, end) =>
    Array.from({length: (end - start)}, (v, k) => k + start);

class PeventHeatmapWidget extends BaseWidget {

    constructor(el, id) {
        super(el, id, PeventHeatmapWidget.defaultHeight)
        this.plot_div_name = this.name + '-pevent-heatmap-div'

        this.palette = ColorPalettes[PeventHeatmapWidget.colorPalette];
        var plt = Bokeh.Plotting;
        const TOOLS = "reset,save,xpan,wheel_zoom,box_zoom,box_select,lasso_select,hover";

        PeventHeatmapWidget.colorPalette = (PeventHeatmapWidget.colorPalette + 1) % ColorPalettes.length;

        // Set up the widget
        this.plot_div = $(`<div id="${this.plot_div_name}" class="" style="width:100%; height:100%;"></div>`)
        this.jbody.html('')
        this.jbody.append(this.plot_div)
        // indicates whether the range is constrained to the available range or
        // whether it uses the same range as the other widges min/max (unconstrained)
        this.constrained = false;

        this.datarevision = 0
        this.layout = this._make_layout()

        this._stat_cols = []
        this.maxValue = 1;
        this.json = null;
        this.yrange = new Bokeh.Range1d({
            start: -0.5,
            end: this.numberLanes - 0.5,
            bounds: "auto"})

        this.histogramWidth = 256;

        this.x2range = new Bokeh.Range1d({
            start: -0.5,
            end: 500 * this.histogramWidth - 0.5,
            bounds: "auto",
        });

        this.x2axis = new Bokeh.LinearAxis({x_range_name: "foo", axis_label: "Event Time"});

        this.maxLanes = 32;
        this.laneLabels = Array.from({length: this.maxLanes}, (_, i) => String.fromCharCode('A'.charCodeAt(0) + i));
        this.numberLanes = this.maxLanes;
        var xx = new Array();
        var yy = new Array();
        var value = new Array();
        var time = new Array();

        // Initial data
        // default to 10 rows, can use a filter to reduce when fewer are selected
        for (var y = 0; y < this.numberLanes; y++) {
            for (var x = 0; x < this.histogramWidth; x++) {
                xx.push(x);
                yy.push(y);
                value.push(Math.random() * this.maxValue);
                time.push(y * this.numberLanes + x);
            }
        }

        this.source = new Bokeh.ColumnDataSource({
            data: {
                x: new Float32Array(xx),
                y: new Float32Array(yy),
                value: new Float32Array(value),
                time: new Int32Array(time)}
        });

        this.mapper = new Bokeh.LinearColorMapper({
            palette:this.palette,
            low: Math.min(...this.source.data.value),
            high: Math.max(...this.source.data.value)});

        this.p = new plt.figure({
            tools: TOOLS,
            //plot_width: 1400,
            //plot_height: PeventHeatmapWidget.defaultHeight,
            sizing_mode: "stretch_both",
            min_border: 10,
            min_border_left: 30,
            toolbar_location: "right",
            title: null,
            js_event_callbacks: {
                tap: [{execute(_obj) {
                    if (_obj.y > -0.5 && _obj.sx > 23 && _obj.x < 0) {
                        const yIndex = Math.floor(_obj.y + 0.5);
                        _obj.origin.origin.removeIndex(yIndex);
                    }
                }}]
            },
            x_range: new Bokeh.Range1d({
                start: -0.5,
                end: this.histogramWidth - 0.5,
                /* bounds: "auto" */}),
            y_range: this.yrange,
            extra_x_ranges: {
                foo: this.x2range,
            },
            output_backend: "webgl",
            x_axis_label: "Event Sample",
            y_axis_label: "Event Type"});

        this.p.origin = this;
        this.p.toolbar.inspectors[0].tooltips = [["time", "@time"], ["events at this time", "@value"], ];
        this.p.yaxis[0].ticker = new Bokeh.FixedTicker({
            ticks: range(0, this.maxLanes)
        });
        this.p.yaxis[0].js_event_callbacks["tap"] = [{execute(_obj) { console.log(_obj)}}];
        this.p.yaxis[0].formatter = new Bokeh.FuncTickFormatter({
            args: {obj: this},
            code: "return obj.laneLabels[tick]"
        })

        this.p.rect({
            x: {field: "x"},
            y: {field: "y"},
            source: this.source,
             x_range_name: "foo",
             width: 0.94,
             height: 0.94,
             fill_color: {"field": "value",
                          "transform": this.mapper},
             line_color: null
            });

        this.p.xaxis[0].visible = false;
        this.p.add_layout(this.x2axis, "below");

        this.colorbar = new Bokeh.ColorBar({
            color_mapper: this.mapper,
            major_label_text_font_size: "5pt",
            ticker: new Bokeh.BasicTicker({desired_num_ticks: Math.min(this.palette.length, 20)}),
            label_standoff: 6,
            border_line_color: null,
            location: [0,0],
        });

        this.p.add_layout(this.colorbar, 'right');
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
        if (this._stat_cols.length < this.maxLanes) {
            for (const stat_item of new_stats) {
                if (!list_contains(this._stat_cols, stat_item)) {
                    this._stat_cols.push(stat_item)
                }
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

    // (Save) Get state to be stored in the layout file. This will be given back
    // to apply_config_data later.
    get_config_data() {
        return {
            palette: this.palette,
            constrained: this.constrained,
            stat_cols: this._stat_cols,
            }
    }

    // (Load) Apply a dictionary of configuration data specific to this type of
    // widget
    apply_config_data(d) {
        this.palette = d.palette;
        this.constrained = d.constrained;
        this._stat_cols = d.stat_cols
        while (this._stat_cols.length < 2) {
            this._stat_cols.push(this.data_source.stats[Math.round(Math.random() * this.data_source.stats.length)]);
        }
    }

    removeIndex(index) {
        if (this._stat_cols.length > 1) {
            // rebuild the array without this index
            this.on_update_data(this.json);
            var obj = this.get_last_update();
            this._stat_cols.splice(index, 1);
            this._request_custom_update(obj.first, obj.last);
        }
    }

    // New data was received from the server
    on_update_data(json) {
        if (json == null) {
            this.xVals = null
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
            return
        }

        const psData = json.processorSpecific

        // unpack the data
        const histLengths = psData.histLengths[0]
        const edgesLengths = psData.edgesLengths[0]
        this.numberLanes = psData.series.length
        this.laneLabels = psData.series

        const floatArray = new Float32Array(json.raw_binary_data)

        const edges = floatArray.slice(0, edgesLengths)

        this.x2range.start = edges[0] - 0.5
        this.x2range.end = edges[edges.length - 1] + 0.5

        for (var i = 0; i < Math.min(this.maxLanes, psData.series.length); i++) {
            const sliceStart = i * histLengths + edgesLengths;
            this.source.data.value.set(floatArray.slice(sliceStart, sliceStart + histLengths), i * histLengths);
            this.source.data.time.set(edges.slice(0, edges.length - 1), i * histLengths);
        }

        this.mapper.low = Math.min(...this.source.data.value.slice(0, psData.series.length * histLengths - 1));
        this.mapper.high = Math.max(...this.source.data.value.slice(0, psData.series.length * histLengths - 1));

        this.yrange.bounds[1] = this.yrange.reset_end = this.yrange.end = Math.min(this.numberLanes, this.maxLanes) - 0.5;

        this.p.change.emit();
        this.source.change.emit();
    }

    // Called when needs to re-render
    on_render() {
        this.datarevision++

        // Detect no-data condition
        if (this.source == null) {
            Plotly.purge(this.plot_div_name)
            return
        }

        // Now that there is data, clear status warnings
        this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)

        // See if plot existed already and if not
        const plot_div = $(this.element).find('#' + this.plot_div_name)
        const plot_existed = plot_div.html() != '';

        if (!plot_existed) {
            // TODO need to get actual stats from the first query

            // make the layout
            var doc = new Bokeh.Document();

            var newLayout = Bokeh.Plotting.gridplot([[this.p]], {
                toolbar_location: "right",
                plot_width: PeventHeatmapWidget.defaultWidth,
                plot_height: PeventHeatmapWidget.defaultHeight,
                sizing_mode: "stretch_both",
                });

            doc.add_root(newLayout);

            Bokeh.embed.add_document_standalone(doc, plot_div[0]);
        }
    }

    // from BaseWidget, allows us to tweak what we ask for
    prepare_request(units, first, last, uid, globals) {

        console.log(`pevent-heatmap::prepare_request(${first}, ${last})`);

        const kwargs = this.get_request_kwargs();

        var constrainedFirst, constrainedLast;
        if (this.constrained) {
            [constrainedFirst, constrainedLast] = this.data_source.constrain(units, first, last)
        } else {
            constrainedFirst = first;
            constrainedLast = last;
        }

        kwargs.units = units;

        return {
            first: constrainedFirst,
            last: constrainedLast,
            kwargs: kwargs
        };
    }

    // kwargs to pass to processor function when we make our requests
    get_request_kwargs() {

        // reduce points
        // const max_points_per_series = Math.max(absolute_min_points_per_series, absolute_max_points_per_series - (1000 *
        // excess_stat_cols))

        return {
            stat_cols: this._stat_cols,
            max_points_per_series : this.histogramWidth,
        };
    }

    // override for creating the settings dialog for this widget
    on_settings(container) {
        super.on_settings(container);

        const fieldset = $(`
        <div name = "${this.plot_div_name}-settings">
          <fieldset>
            <legend>Range:</legend>
            <input type="radio" value="unconstrained" name="range-choice" id="unconstrained-choice" ${!this.constrained ? "checked" : ""}/>
            <label for="unconstrained-choice">unconstrained</label>
            <input type="radio" value="constrained" name="range-choice" id="constrained-choice" ${this.constrained ? "checked" : ""}/>
            <label for="constrained-choice">constrained</label>
          </fieldset>
        </div>`);

        container.append(fieldset)

        container.find("input[name=range-choice]").each(function() { $(this).checkboxradio() });

        /*
         * if you want to read this in real-time, good demo $('input:radio[name="range-choice"]').change( function() { if
         * ($(this).is(':checked')) { console.log($(this).val()); } });
         */
        container.append('<br />');
    }

    // when the settings are confirmed
    on_settings_applied(container) {
        var isChanged;
        if (container.find("input[name=range-choice]:checked")[0].id == "unconstrained-choice") {
            isChanged = this.constrained;
            this.constrained = false;
        } else {
            isChanged = !this.constrained;
            this.constrained = true;
        }

        if (isChanged) {
            console.log("changed from constrained to unconstrained (or vice versa), need to refresh data");
            var obj = this.get_last_update();
            this._request_custom_update(obj.first, obj.last);
        }

        return true;
    }
}
PeventHeatmapWidget.colorPalette = 0
PeventHeatmapWidget.defaultHeight = 500
PeventHeatmapWidget.defaultWidth = 1400
PeventHeatmapWidget.typename = 'pevent-heatmap-widget'
PeventHeatmapWidget.description = 'A heatmap to show pevent density'
PeventHeatmapWidget.processor_type = Processors.PEVENT_TRACE
PeventHeatmapWidget.data_type = DataTypes.PEVENT_TRACE
PeventHeatmapWidget.plotly_buttons_to_remove = ['editInChartStudio','sendDataToCloud','zoom2d','pan2d','lasso2d','zoomIn2d','zoomOut2d','autoScale2d','resetScale2d']
