var updateAllSocket = null;

var start = Date.now();

var whichChanged = null;
var source = null;
var w1Value = null;
var w2Value = null;
var xAxis = null;
var yAxis = null;
var vertPanelSource = null;
var vertHistSource1 = null;
var horzPanelSource = null;
var horzHistSource1 = null;
var regLineSource = null;
var xTsLabel = null;
var yTsLabel = null;
var scatterView = null;

function startup() {
    updateAllSocket =
        new WebSocket('ws://' + window.location.host + '/ws/getData');

    updateAllSocket.onclose = function(e) {
        console.error('data socket closed unexpectedly, reconnecting');
        startup();
    };

    updateAllSocket.onmessage = dataReturned;
}

function check() {
    if (!updateAllSocket || updateAllSocket.readyState == 3) {
        startup();
    }
}

startup();
setInterval(check, 5000);

/*
 * demo code for reading and getting the 2d histogram from the server
 */
document.onkeydown = function(evt) {
    evt = evt || window.event;
    const iterations = 4;

    const shpDirectories = ['/path-to-installation/prototyping/python-processing/data-sources/data/'];
    const simDbDirectories = ['/path-to-installation/plato/demo/data'];
    if (evt.keyCode == 84) {
    	shpDirectories.forEach(directory => runTestHeatmap(iterations, directory));
    	shpDirectories.forEach(directory => runTestLinePlot(iterations, directory));
        runTestLayouts(iterations, "testUser");
    } else if (evt.keyCode == 83) {
    	simDbDirectories.forEach(directory => runTestLinePlot(iterations, directory));
    }

};

function runTestLayouts(iterations, user) {
    var layoutSocket0 = new WebSocket('ws://' + window.location.host + '/ws/utility');
    const userName = "user" + Math.round(Math.random() * 1024);
    var seqNums = [];

    layoutSocket0.onopen = function(e) {
    	for (var i = 0; i < iterations; i++) {
    		// send several layouts
    		do {
    			seqNum = Math.round(Math.random() * 16384);
    		} while (seqNum in seqNums);

    		content = JSON.stringify({"a": 1,
    								  "b": i*i,
    								  "c": i*i*i,
    								  "d": i*i*i*i});
    		seqNums[seqNum] = [i,content];

    		const saveLayoutJson = JSON.stringify({'reqSeqNum': seqNum,
	                                               'command': 'saveLayout',
	                                               'user': userName,
	                                               'name': "layout" + i,
	                                               'overwrite': false,
	                                               'version': i,
	                                               'content': content
	                                              });
	        this.send(saveLayoutJson);
    	}
    };

    layoutSocket0.onmessage = function(e) {
    	var message = JSON.parse(e.data);

    	if (message["result"] != "success") {
    		console.error("Error: " + message["error"]);
    	}
    	if (message["command"] == "saveLayout") {
    		iteration = seqNums[message['reqSeqNum']][0];

    		const loadLayoutJson = JSON.stringify({"reqSeqNum": message["reqSeqNum"],
    											   "command": 'loadLayout',
    											   "user": userName,
    											   "name": "layout" + iteration});
    		this.send(loadLayoutJson);
    	} else if (message["command"] == "loadLayout") {
    		content = seqNums[message['reqSeqNum']][1];

    		if (!_.isEqual(JSON.parse(content), JSON.parse(message["content"]))) {
    			console.error("mismatch: " + content + " " + message["content"]);
    		} else {
    			console.log("loadLayout() compare correct!");
    		}
    	}
    };
}

function runTestHeatmap(iterations, directory) {
    var s0 = new WebSocket('ws://' + window.location.host + '/ws/sources')

    s0.onopen = function() {
        const jsonValue = JSON.stringify({
            'reqSeqNum': Math.floor(Math.random() * 8192),
            'command': 'loadDirectory',
            'directory': directory
        });
        this.send(jsonValue);
    };

    s0.onmessage = function(e) {
        var message = JSON.parse(e.data);
        console.log(message);
        if (message['command'] == 'loadDirectory' &&
            message['result'] == 'complete') {
            sources = message['sources'];
            console.log(sources);
            for (var i = 0; i < sources.length; i++) {
                var dataId = sources[i]['dataId'];
                var newMessage = {
                    'reqSeqNum': Math.floor(Math.random() * 8192),
                    'command': 'getProcessor',
                    'kwargs': {
                        'stat_columns': ['thrash_1', 'd_weight'],
                        'bin_size': 300000
                    },
                    'processor': 'shp-heatmap-generator',
                    'dataId': dataId
                };
                this.send(JSON.stringify(newMessage));
            }
        } else if (
            message['command'] == 'getProcessor' &&
            message['result'] == 'complete') {
            processorId = message['processorId'];
            var jsonValue = {
                'processorId': processorId,
                'kwargs': {'units': 'training-index', 'stat_col': 'd_weight'},
                'first': -1,
                'last': -1
            };

            var s1 =
                new WebSocket('ws://' + window.location.host + '/ws/getData');

            s1.onopen = function(e) {
                for (var i = 0; i < iterations; i++) {
                    const one = Math.floor(Math.random() * 8192);
                    const two = Math.floor(Math.random() * 8192);
                    jsonValue['reqSeqNum'] = Math.floor(Math.random() * 8192);
                    jsonValue['first'] = Math.min(one, two);
                    jsonValue['last'] = Math.max(one, two);
                    this.send(JSON.stringify(jsonValue));
                }
            };

            s1.rows = -1;
            s1.columns = -1;

            s1.onmessage = function(e) {
                if (typeof e.data == 'string') {
                    jsonData = JSON.parse(e.data);
                    this.rows = jsonData['processorSpecific']['numRows'];
                    this.columns = jsonData['processorSpecific']['numCols'];
                    console.log("zMin=" + jsonData["processorSpecific"]["zMin"]);
                    console.log("zMax=" + jsonData["processorSpecific"]["zMax"]);
                    this.tableMeansBlobOffset = jsonData["processorSpecific"]["tableMeansBlobOffset"];
                    this.rowMeansBlobOffset = jsonData["processorSpecific"]["rowMeansBlobOffset"];
                } else {
                    const reader = new FileReader();
                    reader.rows = this.rows;
                    reader.columns = this.columns;
                    reader.onload = function(e) {
                        const msg = reader.result;
                        const intArray = new Int32Array(msg.slice(0, 8));
                        var count = 8;
                        var outputArray = new Array(this.rows);
                        for (var i = 0; i < this.rows; i++) {
                            outputArray[i] = new Float32Array(
                                msg.slice(count, count + 4 * this.columns))
                            count += 4 * this.columns;
                        }

                        console.log(outputArray);
                    };
                    reader.readAsArrayBuffer(e.data);
                }
            };
        }
    };
}

function runTestLinePlot(iterations, directory) {
    var s0 = new WebSocket('ws://' + window.location.host + '/ws/sources')

    s0.onopen = function() {
        const jsonValue = JSON.stringify({
            'reqSeqNum': 155,
            'command': 'loadDirectory',
            'directory': directory
        });
        this.send(jsonValue);
    };

    s0.requests = [];
    s0.count = 0;

    s0.onmessage = function(e) {
        var message = JSON.parse(e.data);
        console.log(message);
        if (message['command'] == 'loadDirectory' &&
            message['result'] == 'complete') {
            sources = message['sources'];
            console.log(sources);
            for (const source of sources) {
                const dataId = source['dataId'];
                const name = source['name'];
                if (name.endsWith("hdf5")) {
                	processor = 'shp-line-plot-generator';
                	s0.requests[s0.count] = true;
                } else {
                	processor = 'simdb-line-plot-generator';
                	s0.requests[s0.count] = false;
                }
                var newMessage = {
                    'reqSeqNum': this.count,
                    'command': 'getProcessor',
                    'processor': processor,
                    'kwargs': {},
                    'dataId': dataId
                };
                this.send(JSON.stringify(newMessage));
                this.count++;
            }
        } else if (
            message['command'] == 'getProcessor' &&
            message['result'] == 'complete') {
            const processorId = message['processorId'];
            const reqSeqNum = message['reqSeqNum'];
            const isShp = this.requests[reqSeqNum];

            var s1 =
                new WebSocket('ws://' + window.location.host + '/ws/getData');

            if (isShp) {
	            var jsonValue = {
	                'processorId': processorId,
	                'kwargs': {
	                    'units': 'training-index',
	                    'stat_cols': ['bias_at_training', 'correct', 'yout'],
	                    'max_points_per_series': 0
	                }
	            };
            } else {
            	var jsonValue = {
	                'processorId': processorId,
	                'kwargs': {
	                    'units': 'cycles',
	                    'stat_cols': ['top.core0.rob.ipc', 'top.core0.rob.ReorderBuffer_utilization_weighted_nonzero_avg'],
	                    'max_points_per_series': 0
	                }
	            };
            }

            s1.onopen = function(e) {

                for (var i = 0; i < iterations; i++) {
                    console.debug(this);
                    const one = Math.floor(Math.random() * 8192);
                    const two = Math.floor(Math.random() * 8192);
                    jsonValue['reqSeqNum'] = Math.floor(Math.random() * 8192);
                    jsonValue['first'] = Math.min(one, two);
                    jsonValue['last'] = Math.max(one, two);
                    this.send(JSON.stringify(jsonValue));
                }
            };

            s1.rows = -1;
            s1.columns = -1;

            s1.onmessage = function(e) {
                if (typeof e.data == 'string') {
                    jsonData = JSON.parse(e.data);
                    this.rows = jsonData['processorSpecific']['numSeries'];
                    this.columns = jsonData['processorSpecific']['pointsPerSeries'];
                } else {
                    const reader = new FileReader();
                    reader.rows = this.rows;
                    reader.columns = this.columns;
                    reader.onload = function(e) {
                        const msg = reader.result;
                        const intArray = new Int32Array(msg.slice(0, 8));
                        var count = 8;
                        var outputArray = new Array(this.rows);
                        for (var i = 0; i < this.rows; i++) {
                            outputArray[i] = new Float32Array(
                                msg.slice(count, count + 4 * this.columns))
                            count += 4 * this.columns;
                        }

                        console.log(outputArray);
                    };
                    reader.readAsArrayBuffer(e.data);
                }
            };
        }
    };
}

function dataReturned(e) {
    const reader = new FileReader();
    reader.onload = function(event) {
        const floatArray = new Float32Array(reader.result);

        var count = 0;
        jsonData = e.target.jsonData["processorSpecific"];
        // unpack x values
        var length = parseInt(jsonData["pointsPerSeries"]);
        const pointsPerSeries = length;
        if (length > 0) {
        	var xVals = floatArray.slice(count, count + length);
        	count += length;
        	// unpack y values
        	var yVals = floatArray.slice(count, count + length);
        	count += length;
        }
        // unpack hedges
        length = jsonData["hEdgesLength"];
        var hedges = floatArray.slice(count, count + length);
        count += length;
        // unpack hhist
        length = jsonData["hHistLength"];
        var hhist = floatArray.slice(count, count + length);
        count += length;
        // unpack vedges
        length = jsonData["vEdgesLength"];
        var vedges = floatArray.slice(count, count + length);
        count += length;
        // unpack vhist
        length = jsonData["vHistLength"];
        var vhist = floatArray.slice(count, count + length);
        count += length;
        // unpack regX
        length = jsonData["regLength"];
        var regX = floatArray.slice(count, count + length);
        count += length;
        // unpack regY
        var regY = floatArray.slice(count, count + length);
        count += length;

        firstHedges = hedges.slice(1);
        secondHedges = hedges.slice(0, -1);
        horzPanelSource.data['right'] = firstHedges;
        horzPanelSource.data['left'] = secondHedges;
        horzPanelSource.data['top'] = hhist;

        horzHistSource1.data['right'] = firstHedges;
        horzHistSource1.data['left'] = secondHedges;

        firstVedges = vedges.slice(1);
        secondVedges = vedges.slice(0, -1);
        vertPanelSource.data['top'] = firstVedges;
        vertPanelSource.data['bottom'] = secondVedges;
        vertPanelSource.data['right'] = vhist;

        vertHistSource1.data['top'] = firstVedges;
        vertHistSource1.data['bottom'] = secondVedges;

    	regLineSource.data['x'] = regX;
    	regLineSource.data['y'] = regY;
    	console.assert(regX.length == regY.length, "must be equal length: %d vs %d",regX.length, regY.length);

    	firstRow = jsonData["firstRow"]
        lastRow = jsonData["lastRow"]

        if (whichChanged == 'x') {
        	console.assert(pointsPerSeries > 0, "should have some points when changing the value");
            source.data[whichChanged] = xVals;
            xAxis.attributes.axis_label = w1Value;
            xTsLabel.text = w1Value;
            scatterView.attributes.indices = Array.range(firstRow, lastRow);
        } else if (whichChanged == 'y') {
        	console.assert(pointsPerSeries > 0, "should have some points when changing the value");
            source.data[whichChanged] = yVals;
            yAxis.axis_label = w2Value;
            yTsLabel.attributes.text = w2Value;
            scatterView.attributes.indices = Array.range(firstRow, lastRow);
        } else {
        	// range change, no need to update axes or scatter data
        	const left = Math.round(rs.value[0]);
            const right = Math.round(rs.value[1]);
            box.left = left;
            box.right = right;
            box.change.emit();
            // console.log("left=" + left + " right=" + right);
            scatterView.attributes.indices = Array.range(firstRow, lastRow);
        }
    	horzHistSource1.change.emit();
    	vertPanelSource.change.emit();
    	vertHistSource1.change.emit();
    	regLineSource.change.emit();
    	horzPanelSource.change.emit();
    	scatterView.change.emit();
    	xTsLabel.change.emit();
    	yTsLabel.change.emit();
    	xAxis.change.emit();
    	yAxis.change.emit();

        console.log('all: elapsed time: ' + (Date.now() - start) + 'ms');
    };
    if (e.data[0] == "{") {
    	jsonData = JSON.parse(e.data);
    	e.target.jsonData = jsonData;
    	// console.log(jsonData);
    } else {
    	reader.readAsArrayBuffer(e.data);
    }
}

function updateAll(
    cb_obj, x_source, x_w1Value, x_w2Value, x_xAxis, x_yAxis,
    x_vertPanelSource, x_vertHistSource1, x_horzPanelSource, x_horzHistSource1,
    x_regLineSource, x_xTsLabel, x_yTsLabel, x_rs, x_scatterView, x_box) {
    start = Date.now();

    const processorId = document.getElementById("processorId").value;
    const firstCycle = parseInt(document.getElementById("firstCycle").value);
    const lastCycle = parseInt(document.getElementById("lastCycle").value);

    if (cb_obj.value == x_w1Value) {
    	whichChanged = 'x';
    } else if (cb_obj.value == x_w2Value) {
    	whichChanged = 'y';
    }

    source = x_source;
    w1Value = x_w1Value;
    w2Value = x_w2Value;
    xAxis = x_xAxis;
    yAxis = x_yAxis;
    xTsLabel = x_xTsLabel;
    yTsLabel = x_yTsLabel;
    vertPanelSource = x_vertPanelSource;
    vertHistSource1 = x_vertHistSource1;
    horzPanelSource = x_horzPanelSource;
    horzHistSource1 = x_horzHistSource1;
    regLineSource = x_regLineSource;
    scatterView = x_scatterView;
    box = x_box;

    const jsonValue = JSON.stringify({
    	'first': Math.round(x_rs.value[0]),
    	'last': Math.round(x_rs.value[1]),
    	'firstCycle': firstCycle,
    	'lastCycle': lastCycle,
    	'reqSeqNum': Math.floor(Math.random() * 8192),
    	'processorId': processorId,
    	'kwargs': {
    		'whichChanged': whichChanged,
    		'units': 'cycles',
    		'stat_cols': [w1Value, w2Value],
    		'max_points_per_series': -1
    }});

    updateAllSocket.send(jsonValue);
}

Array.range = (start, end) =>
    Array.from({length: (end - start)}, (v, k) => k + start);

function rangeUpdateCB(x_rs, box, scatter, scatterView, ts1, ts2, source, p,
        w1, w2, xAxis,yAxis, horzPanelSource, horzHistSource1, vertPanelSource, vertHistSource1,
        regLineSource, xName, yName) {
	rs = x_rs;
	whichChanged = "";

    updateAll('', source, w1.value, w2.value, xAxis, yAxis, vertPanelSource,
              vertHistSource1, horzPanelSource, horzHistSource1,
              regLineSource, xName, yName, rs, scatterView, box);
}

function debounced(delay, func) {
    let timerId;
    return function(...args) {
        if (timerId) {
            clearTimeout(timerId);
        }
        timerId = setTimeout(() => {
            func(...args);
            timerId = null;
        }, delay);
    }
}

var rangeUpdateDebounceCB = debounced(105, rangeUpdateCB);
var timeSeriesPanDebounceCB = debounced(105, timeSeriesPanCB);

function timeSeriesPanCB(rs, box, scatter, scatterView, ts1, ts2, source, p,
        w1, w2, xAxis,yAxis, horzPanelSource, horzHistSource1, vertPanelSource, vertHistSource1,
        regLineSource, xName, yName) {
    rs.start = Math.round(ts1.x_range.start);
    rs.end = Math.round(ts1.x_range.end);
    var change = false;
    if (rs.value[0] < rs.start) {
        rs.value[0] = rs.start;
        change = true;
    }
    if (rs.value[1] > rs.end) {
        rs.value[1] = rs.end;
        change = true;
    }
    if (change) {
        rangeUpdateDebounceCB(rs, box, scatter, scatterView, ts1, ts2, source, p,
                w1, w2, xAxis,yAxis, horzPanelSource, horzHistSource1, vertPanelSource, vertHistSource1,
                regLineSource, xName, yName);
        rs.change.emit();
    }
}
