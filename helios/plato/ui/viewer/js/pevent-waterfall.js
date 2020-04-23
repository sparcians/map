// Describes a SimDB correlation widget, a collection of Bokeh widgets


Array.randomRange = (length, min, max) =>
    Array.from({length: length}, () => {return min + Math.floor(Math.random() * (max - min))});

Array.range = (start, end) =>
    Array.from({length: (end - start)}, (v, k) => k + start);

class PeventWaterfallWidget extends BaseWidget {

    constructor(el, id) {
        super(el, id, PeventWaterfallWidget.defaultHeight)
        this.plot_div_name = this.name + '-pevent-heatmap-div'

        this.palette = PeventWaterfallWidget.colors[PeventWaterfallWidget.colorPalette];
        PeventWaterfallWidget.colorPalette = (PeventWaterfallWidget.colorPalette + 1) % PeventWaterfallWidget.colors.length;

        // Set up the widget
        this.plot_div = $(`<div id="${this.plot_div_name}" class="" style="width:100%; height:100%;"></div>`)
        this.jbody.html('')
        this.jbody.append(this.plot_div)

        this.datarevision = 0
        this.layout = this._make_layout()

        /*
         * this.source = new Bokeh.ColumnDataSource({ data: { min: [0, 1, 2, 3], max: [4, 5, 7, 8], stages: [ "DECODE", //"RENAME",
         * "DISPATCH", //"EXECUTE", "COMPLETE", "RETIRE"], DECODE: Array.randomRange(8, 1, 4), //RENAME: Array.randomRange(8, 1, 4),
         * DISPATCH:Array.randomRange(8, 1, 4), //EXECUTE: Array.randomRange(8, 1, 4), COMPLETE:Array.randomRange(8, 1, 4), RETIRE:
         * Array.randomRange(8, 1, 4), } });
         */
        this.source = [
            ["C1", "C2", "C3", "C4", "C5"],
            ["DECODE", 1,2,3,4],
            ["DISPATCH", 2,3,4,5],
            ["COMPLETE", 6,7,8,9],
            ["RETIRE", 12,13,14,15],
        ]
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
        return { stat_cols: this._stat_cols }
    }

    // (Load) Apply a dictionary of configuration data specific to this type of
    // widget
    apply_config_data(d) {
        this._stat_cols = d.stat_cols
        // while (this._stat_cols.length < 2) {
        // this._stat_cols.push(this.data_source.stats[Math.round(Math.random() * this.data_source.stats.length)]);
       // }
    }

    removeIndex(index) {
        if (this._stat_cols.length > 1) {
            // rebuild the array without this index
            this.on_update_data(this.json, index);
            var obj = this.get_last_update();
            this._stat_cols.splice(index, 1);
            this._request_custom_update(obj.first, obj.last);
        }
    }

    // New data was received from the server
    on_update_data(json) {
        if (json == null) {
            this.xVals = null;
            this.add_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE);
            return;
        }

        return;

        const psData = json.processorSpecific;

        // unpack the data
        const histLengths = psData.histLengths[0];
        const edgesLengths = psData.edgesLengths[0];
        this.numberLanes = psData.series.length;
        this.laneLabels = psData.series;

        const floatArray = new Float32Array(json.raw_binary_data);

        const edges = floatArray.slice(0, edgesLengths);
        this.x2range.start = edges[0] - 0.5,
        this.x2range.end = edges[edges.length - 1] + 0.5;

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

        // Now that there is data, clear status warnings
        this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)

        // See if plot existed already and if not
        const plotDiv = $(this.element).find('#' + this.plot_div_name)
        const plotExists = plotDiv.html() != '';
        var b = null;

        if (!plotExists) {
            var plt = Bokeh.Plotting;
            const LINE_ARGS = {color: "#3A8785", line_color: null};
            const TOOLS = "reset,save,xpan,wheel_zoom,box_zoom,box_select,lasso_select,hover";

            // TODO need to make this dynamic depending on the data
            // this.mapper = new Bokeh.LinearColorMapper({palette:this.palette, low: Math.min(...this.source.data.value), high:
            // Math.max(...this.source.data.value)});
            /*
             * this.x2range = new Bokeh.Range1d({ start: -0.5, end: 500 * this.histogramWidth - 0.5, bounds: "auto", });
             *
             * this.yrange = new Bokeh.Range1d({ start: -0.5, end: this.numberLanes - 0.5, bounds: "auto"})
             */
            this.p = new plt.figure({
                tools: TOOLS,
                plot_width: 1400,
                plot_height: PeventWaterfallWidget.defaultHeight,
                min_border: 10,
                min_border_left: 30,
                toolbar_location: "right",
                title: null,
                /*
                 * js_event_callbacks: { tap: [{execute(_obj) { if (_obj.y > -0.5 && _obj.sx > 23 && _obj.x < 0) { const yIndex =
                 * Math.floor(_obj.y + 0.5); _obj.origin.origin.removeIndex(yIndex); } }}] },
                 */
                /*
                 * x_range: new Bokeh.Range1d({ start: -0.5, end: this.histogramWidth - 0.5, }),
                 */
                y_range: this.yrange,
                /*
                 * extra_x_ranges: { foo: this.x2range, },
                 */
                output_backend: "webgl",
                x_axis_label: "Event Sample",
                y_axis_label: "Event Type"});

            // this.p.toolbar.inspectors[0].tooltips = [["time", "@time"], ["events at this time", "@value"], ];

            b = Bokeh.Charts.bar(this.source,
                    {
                        // y: ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'],
                        orientation: "horizontal",
                        stacked: true,
                        color: this.palette,
                        source: this.source,
                        // stacked: true,
                    });

            if (false) {
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

            // make the layout
            var doc = new Bokeh.Document();

            var newLayout = plt.gridplot([[b]], {
                toolbar_location: "right",
                plot_width: PeventWaterfallWidget.defaultWidth,
                plot_height: PeventWaterfallWidget.defaultHeight
                });

            doc.add_root(newLayout);

            Bokeh.embed.add_document_standalone(doc, plotDiv[0]);
        }

    }

    // kwargs to pass to processor function when we make our requests
    get_request_kwargs() {

        // reduce points
        // const max_points_per_series = Math.max(absolute_min_points_per_series, absolute_max_points_per_series - (1000 *
        // excess_stat_cols))

        return {
            stat_cols: [],
            max_points_per_series : 200,
        };
    }

}
PeventWaterfallWidget.colorPalette = 0
PeventWaterfallWidget.defaultHeight = 500
PeventWaterfallWidget.defaultWidth = 1400
PeventWaterfallWidget.typename = 'pevent-waterfall-widget'
PeventWaterfallWidget.description = 'A waterfall diagram to show pevent density'
PeventWaterfallWidget.processor_type = Processors.PEVENT_TRACE
PeventWaterfallWidget.data_type = DataTypes.PEVENT_TRACE
PeventWaterfallWidget.plotly_buttons_to_remove = ['editInChartStudio','sendDataToCloud','zoom2d','pan2d','lasso2d','zoomIn2d','zoomOut2d','autoScale2d','resetScale2d']
PeventWaterfallWidget.colors = [[
    '#000003',
    '#01010B',
    '#040415',
    '#0A0722',
    '#0F0B2C',
    '#160E3A',
    '#1C1046',
    '#231152',
    '#2D1060',
    '#350F6A',
    '#400F73',
    '#481078',
    '#52127C',
    '#5A157E',
    '#61187F',
    '#6B1C80',
    '#731F81',
    '#7C2381',
    '#842681',
    '#8D2980',
    '#952C80',
    '#9E2E7E',
    '#A7317D',
    '#B0347B',
    '#B93778',
    '#C23A75',
    '#CB3E71',
    '#D3426D',
    '#DA4769',
    '#E24D65',
    '#E85461',
    '#EE5D5D',
    '#F3655C',
    '#F6705B',
    '#F9795C',
    '#FA825F',
    '#FC8E63',
    '#FD9768',
    '#FDA26F',
    '#FEAC75',
    '#FEB77D',
    '#FEC085',
    '#FEC98D',
    '#FDD497',
    '#FDDD9F',
    '#FCE8AA',
    '#FCF1B3',
    '#FBFCBF'],
    ['#440154',
    '#45085B',
    '#470F62',
    '#47186A',
    '#481E70',
    '#472676',
    '#472C7B',
    '#45327F',
    '#433A83',
    '#424085',
    '#3F4788',
    '#3D4C89',
    '#3A538B',
    '#37588C',
    '#355D8C',
    '#32638D',
    '#30688D',
    '#2D6E8E',
    '#2B738E',
    '#29798E',
    '#277D8E',
    '#25828E',
    '#23888D',
    '#218C8D',
    '#1F928C',
    '#1E978A',
    '#1E9C89',
    '#1FA187',
    '#21A685',
    '#25AB81',
    '#2AB07E',
    '#32B57A',
    '#39B976',
    '#44BE70',
    '#4DC26B',
    '#57C665',
    '#64CB5D',
    '#70CE56',
    '#7ED24E',
    '#8BD546',
    '#9AD83C',
    '#A7DB33',
    '#B5DD2B',
    '#C5DF21',
    '#D2E11B',
    '#E1E318',
    '#EEE51B',
    '#FDE724'],
    ['#00204C',
    '#002355',
    '#00275D',
    '#002B68',
    '#002E6F',
    '#00326E',
    '#06366E',
    '#17396D',
    '#243E6C',
    '#2C416B',
    '#35466B',
    '#3B496B',
    '#424E6B',
    '#47516B',
    '#4C556B',
    '#52596C',
    '#575D6D',
    '#5D616E',
    '#61656F',
    '#676970',
    '#6B6D71',
    '#6F7073',
    '#757575',
    '#797877',
    '#7E7D78',
    '#838178',
    '#898578',
    '#8E8978',
    '#938D78',
    '#999277',
    '#9E9676',
    '#A49B75',
    '#A99F74',
    '#AFA473',
    '#B5A871',
    '#BAAC6F',
    '#C0B16D',
    '#C6B66B',
    '#CCBB68',
    '#D2C065',
    '#D8C561',
    '#DECA5E',
    '#E4CE5B',
    '#EBD456',
    '#F0D951',
    '#F7DF4B',
    '#FDE345',
    '#FFE945'],]
