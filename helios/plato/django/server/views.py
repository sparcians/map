from os.path import dirname, basename
import os.path

from asgiref.sync import async_to_sync
from bokeh.embed import components
from bokeh.events import Event, PanEnd, Reset, PinchEnd, Pinch, MouseWheel
from bokeh.layouts import gridplot, layout, column, row
from bokeh.models import BoxSelectTool, LassoSelectTool, ColumnDataSource, BoxAnnotation, CDSView
from bokeh.models.arrow_heads import ArrowHead
from bokeh.models.callbacks import CustomJS
from bokeh.models.filters import IndexFilter
from bokeh.models.ranges import Range1d
from bokeh.models.widgets import Select, Button, RangeSlider
from bokeh.plotting import figure, curdoc
from django.http import HttpResponse
from django.shortcuts import render, render_to_response
from django.template import loader, RequestContext
from numpy.linalg.linalg import LinAlgError

from server.beakercache import cache
import numpy as np
from plato.backend.datasources.sparta_statistics.datasource import SpartaDataSource

from .bpEndpoint import bpEndpoint

defaultDb = os.path.join(dirname(dirname(__file__)), "ea243e37-3e1c-4aaf-9b77-b2a4ec8b0e31.db")


def data(request):
    '''
    handle a data request
    '''
    print(request)


def index(request):
    '''
    generate the main layout page for the demo, this has been deprecated but
    serves as a reference for using the Bokeh python library
    '''

    jsonData = {"file": defaultDb}
    returnValue = {}
    async_to_sync(bpEndpoint.loadFile)(jsonData, returnValue)

    jsonData = {"dataId": returnValue["sources"][0]["dataId"],
                "processor": "simdb-get-all-data-generator",
                "kwargs": {}}
    returnValue = {}
    async_to_sync(bpEndpoint.getProcessor)(jsonData, returnValue)

    processorId = returnValue["processorId"]
    generator = bpEndpoint.getSimDbStatsGenerator(processorId)
    sourceMeta = SpartaDataSource.get_source_information(defaultDb)
    '''
    correlation = currentDF.corr()
    source = ColumnDataSource(currentDF.corr())
    colors = ["#75968f", "#a5bab7", "#c9d9d3", "#e2e2e2", "#dfccce", "#ddb7b1", "#cc7878", "#933b41", "#550b1d"]
    mapper = LinearColorMapper(palette=Viridis3, low=-1, high=1)

    heatmapFigure = figure(plot_width = 1400, plot_height = 1400, title = "Correlation Coefficients",
                           x_range = nodeList, y_range = nodeList,
                           toolbar_location = None, tools = "hover", x_axis_location = "above")
    heatmapFigure.xaxis.major_label_orientation = math.pi/2
    heatmap = heatmapFigure.rect(correlation, y = nodeList, width = heatmapFigure.width, height = heatmapFigure.height,
                                 fill_color = {'field' : 'z', 'transform' : mapper})

    #print(r.data_source)
    #curdoc().add_root(heatmapFigure)
    show(heatmapFigure)
    '''
    # dropdown widgets
    menu = [(a, a) for a in sourceMeta["stats"]]

    selectWidgetX = Select(title = "X Value", value = 'top.core0.rob.ipc', options = menu)

    selectWidgetY = Select(title = "Y Value", value = 'top.core0.rob.ReorderBuffer_utilization_weighted_nonzero_avg', options = menu)

    newLayout = plotIt('top.core0.rob.ipc',
                       'top.core0.rob.ReorderBuffer_utilization_weighted_nonzero_avg',
                       [selectWidgetX, selectWidgetY],
                       generator,
                       sourceMeta)

    script, div = components(newLayout)

    return render(request, 'server/index.html', {'script': script,
                                                 'div': div,
                                                 'title': f"{basename(defaultDb)} ({dirname(defaultDb)}) Correlation Page",
                                                 'processorId': processorId,
                                                 'firstCycle': sourceMeta["timeRange"][0]["first"],
                                                 'lastCycle': sourceMeta["timeRange"][0]["last"]})


def plotIt(xName, yName, widgetList, generator, sourceMeta):
    '''
    generate the initial plots and populate them with data
    '''
    LINE_ARGS = dict(color = "#3A8785", line_color = None)
    TOOLS = "reset,save,pan,wheel_zoom,box_zoom,box_select,lasso_select,hover"
    TS_TOOLS = "reset,save,pan,xwheel_zoom,hover"

    firstCycle = sourceMeta["timeRange"][0]["first"]
    lastCycle = sourceMeta["timeRange"][0]["last"]
    startCycle, x, y = generator.adapter.get_points(firstCycle, lastCycle,
                                                    sourceMeta["timeRange"][0]["units"],
                                                    [xName, yName], -1)

    source = ColumnDataSource(data = dict(x = x, y = y, index = startCycle))

    scatterView = CDSView(source = source, filters = [IndexFilter(np.arange(len(x)))])

    # create the scatter plot
    p = figure(tools = TOOLS,
               plot_width = 700,
               plot_height = 700,
               min_border = 10,
               min_border_left = 30,
               toolbar_location = "above",
               title = "Comparison",
               output_backend = "webgl",
               x_axis_label = xName,
               y_axis_label = yName)

    xAxis = p.xaxis[0]
    yAxis = p.yaxis[0]
    p.background_fill_color = "#fafafa"
    p.select(BoxSelectTool).select_every_mousemove = False
    p.select(LassoSelectTool).select_every_mousemove = False

    scatter = p.scatter('x', 'y', source = source,
                        size = 3,
                        marker = 'diamond',
                        color = "navy",
                        alpha = 0.85,
                        view = scatterView)

    # create the horizontal histogram
    hedges, hhist = generator.generate_histogram(firstCycle, lastCycle,
                                                 sourceMeta["timeRange"][0]["units"],
                                                 xName, -1)
    hh = figure(toolbar_location = None,
                plot_width = p.plot_width,
                plot_height = 200,
                x_range = p.x_range,
                min_border = 10,
                min_border_left = 50,
                y_axis_location = "right",
                y_axis_label = "count",
                output_backend = "webgl")
    hh.xgrid.grid_line_color = None
    hh.yaxis.major_label_orientation = np.pi / 4
    hh.background_fill_color = "#fafafa"

    horzPanelSource = ColumnDataSource(data = dict(bottom = np.zeros(len(hedges) - 1),
                                                   left = hedges[:-1],
                                                   right = hedges[1:],
                                                   top = hhist))
    hh.quad(left = 'left', bottom = 'bottom', top = 'top', right = 'right',
            source = horzPanelSource, color = "rgb(170,186,215)",
            line_color = "rgb(144,159,186)")
    horzHistSource = ColumnDataSource(data = dict(bottom = np.zeros(len(hedges) - 1),
                                                   left = hedges[:-1],
                                                   right = hedges[1:],
                                                   top = np.zeros(len(hedges) - 1)))

    hh.quad(left = 'left', bottom = 'bottom', top = 'top', right = 'right',
            source = horzHistSource, alpha = 0.5, **LINE_ARGS)

    # create the vertical histogram
    vedges, vhist = generator.generate_histogram(firstCycle, lastCycle,
                                                 sourceMeta["timeRange"][0]["units"],
                                                 yName, -1)

    pv = figure(toolbar_location = None,
                plot_width = 200,
                plot_height = p.plot_height,
                y_range = p.y_range,
                min_border = 10,
                y_axis_location = "right",
                x_axis_label = "count",
                output_backend = "webgl")
    pv.ygrid.grid_line_color = None
    pv.xaxis.major_label_orientation = np.pi / 4
    pv.background_fill_color = "#fafafa"

    vertPanelSource = ColumnDataSource(data = dict(top = vedges[1:],
                                                   bottom = vedges[:-1],
                                                   right = vhist,
                                                   left = np.zeros(len(vedges))))
    pv.quad(left = 'left', bottom = 'bottom', top = 'top', right = 'right', source = vertPanelSource, color = "rgb(170,186,215)", line_color = "rgb(144,159,186)")

    vertHistSource1 = ColumnDataSource(data = dict(top = vedges[1:],
                                                   bottom = vedges[:-1],
                                                   right = np.zeros(len(vedges) - 1),
                                                   left = np.zeros(len(vedges) - 1)))
    # vh1 =
    pv.quad(left = 'left', bottom = 'bottom', top = 'top', right = 'right', source = vertHistSource1, alpha = 0.5, **LINE_ARGS)

    rs = RangeSlider(start = firstCycle,
                     end = lastCycle,
                     value = (firstCycle, lastCycle),
                     step = 10,
                     title = "Visible Range",
                     width = 535,
                     align = "center",
                     show_value = True)

    # compute the trend line

    try:
        xValues, yValues = generator.generate_regression_line(firstCycle, lastCycle,
                                                              sourceMeta["timeRange"][0]["units"],
                                                              [xName, yName], -1)
        regLineSource = ColumnDataSource("x", "y", data = dict(x = xValues, y = yValues))
        regLine = p.line('x', 'y', source = regLineSource, color = "#f44242", line_width = 2.5)
    except LinAlgError:
        regLine.visible = False

    box = BoxAnnotation(fill_alpha = 0.5, line_alpha = 0.5, level = 'underlay',
                        left = rs.start, right = rs.end)

    # time series 1
    ts1 = figure(plot_width = 600, plot_height = 225, title = xName,
                 tools = TS_TOOLS, active_scroll = "xwheel_zoom",
                 x_range = Range1d(firstCycle, lastCycle, bounds = "auto"))
    ts1.line(x = 'index', y = 'x', source = source,)
    ts1.add_layout(box)

    xTsLabel = ts1.title

    ts2 = figure(plot_width = 600, plot_height = 225, title = yName,
                 tools = TS_TOOLS, active_scroll = "xwheel_zoom",
                 # y_range = Range1d(-1.1 * max(y), 1.1 * max(y), bounds = (-1.1 * max(y), 1.1 * max(y))),
                 x_range = ts1.x_range)
    ts2.line(x = 'index', y = 'y', source = source,)
    ts2.add_layout(box)
    yTsLabel = ts2.title

    # setup JS callbacks for widgets
    w1, w2 = widgetList

    rs.js_on_change('value', CustomJS(args = dict(rs = rs,
                                                  box = box,
                                                  scatter = scatter,
                                                  scatterView = scatterView,
                                                  ts1 = ts1,
                                                  ts2 = ts2,
                                                  source = source,
                                                  p = p,
                                                  w1 = w1,
                                                  w2 = w2,
                                                  xAxis = xAxis,
                                                  yAxis = yAxis,
                                                  horzPanelSource = horzPanelSource,
                                                  horzHistSource = horzHistSource,
                                                  vertPanelSource = vertPanelSource,
                                                  vertHistSource1 = vertHistSource1,
                                                  regLineSource = regLineSource,
                                                  xName = xTsLabel,
                                                  yName = yTsLabel),
                                      code = '''
        rangeUpdateDebounceCB(rs, box, scatter, scatterView, ts1, ts2, source, p,
                  w1, w2, xAxis,yAxis, horzPanelSource, horzHistSource,
                  vertPanelSource, vertHistSource1, regLineSource, xName, yName);
                  '''))

    timeSeriesPanJS = CustomJS(args = dict(rs = rs,
                                           box = box,
                                           scatter = scatter,
                                           scatterView = scatterView,
                                           ts1 = ts1,
                                           ts2 = ts2,
                                           source = source,
                                           p = p,
                                           w1 = w1,
                                           w2 = w2,
                                           xAxis = xAxis,
                                           yAxis = yAxis,
                                           horzPanelSource = horzPanelSource,
                                           horzHistSource = horzHistSource,
                                           vertPanelSource = vertPanelSource,
                                           vertHistSource1 = vertHistSource1,
                                           regLineSource = regLineSource,
                                           xName = xTsLabel,
                                           yName = yTsLabel),
                               code = '''
        timeSeriesPanDebounceCB(rs, box, scatter, scatterView, ts1, ts2, source, p, w1, w2, xAxis,
                                yAxis, horzPanelSource, horzHistSource, vertPanelSource, vertHistSource1,
                                regLineSource, xName, yName);''')

    ts1.js_on_event(PanEnd, timeSeriesPanJS)
    ts1.js_on_event(Pinch, timeSeriesPanJS)
    ts1.js_on_event(Reset, timeSeriesPanJS)
    ts1.js_on_event(MouseWheel, timeSeriesPanJS)

    ts2.js_on_event(PanEnd, timeSeriesPanJS)
    ts2.js_on_event(Pinch, timeSeriesPanJS)
    ts2.js_on_event(Reset, timeSeriesPanJS)
    ts2.js_on_event(MouseWheel, timeSeriesPanJS)

    dropdownCallback = CustomJS(args = dict(source = source,
                                            p = p,
                                            w1 = w1,
                                            w2 = w2,
                                            xAxis = xAxis,
                                            yAxis = yAxis,
                                            horzPanelSource = horzPanelSource,
                                            horzHistSource = horzHistSource,
                                            vertPanelSource = vertPanelSource,
                                            vertHistSource1 = vertHistSource1,
                                            regLineSource = regLineSource,
                                            xName = xTsLabel,
                                            yName = yTsLabel,
                                            rs = rs,
                                            scatterView = scatterView,
                                            ),
                                code = '''
        updateAll(cb_obj, source, w1.value, w2.value, xAxis, yAxis,
                  vertPanelSource, vertHistSource1,
                  horzPanelSource, horzHistSource,
                  regLineSource, xName, yName, rs, scatterView);
                  ''')

    w1.js_on_change("value", dropdownCallback)

    w2.js_on_change("value", dropdownCallback)

    # w2.js_on_change("select", CustomJS(code='''console.log("asdf");'''))

    # global layout
    newLayout = layout([row([p, pv, column([widgetList[0], widgetList[1], rs, ts1, ts2])]),
                        [hh]])

    return newLayout
