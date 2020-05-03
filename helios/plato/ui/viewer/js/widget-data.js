

// Tracks outstanding requests for a widget
class OutstandingRequestTracker {

    constructor(pwidget) {
        this.pwidget = pwidget

        // Current outstanding count
        this.num_outstanding = 0

        // Current deferred request
        this.deferred = null

        // Cumulative stats
        this.num_deferred = 0
        this.num_sent = 0
    }

    // Clear the outstanding requests
    clear() {
        this.num_outstanding = 0
        this.deferred = null
    }
}
OutstandingRequestTracker.Limit = 1 // Number of outstanding requests allowed. Must be 1 or more.

// Calculates average latency based on a moving window
class LatencyCalculator {
    constructor(window_size=10) {
        this.window_size = window_size

        this.values = []
        this.sum = 0
    }

    // Calculate new latency
    update(new_val) {
        this.values.push(new_val)
        this.sum += new_val
        if (this.values.length > this.window_size) {
            this.sum -= this.values[0]
            this.values.splice(0,1)
        }
        return this.sum / this.values.length
    }
}

// ResponseHandler for a given outstanding request
class ResponseHandler {
    constructor(request, callback) {
        this.callback = callback
        this.response_json = null
        this.request = request // Save original request for reference
    }

    set_json(json) {
        this.response_json = json
    }

    invoke(binary_blob) {
        this.response_json.raw_binary_data = binary_blob
        this.callback(this.response_json)
    }

    error() {
        this.callback(null)
    }
}

class WidgetChannel {
    constructor(prefix, pwidget) {
        this.endpoint = '/ws/getData'
        this.prefix = prefix
        this.pwidget = pwidget
        this.ws = new WebSocketWrapper((e) => { this._onmessage(e) }, prefix, this.endpoint)
        this.ws.onreconnected = () => { this._onreconnected() }

        this.outstanding = new OutstandingRequestTracker(pwidget)
        this.last_sent = null

        // Json/binary protocol state
        this.binary_response_target = null

        // Response handlers by sequence number (to map responses onto clients)
        this.handlers = {}

        this.num_requests = 0
    }

    // Handle reconnections. Do some logic to restart the request stream by abandoning deferred requests and
    // expecting a json response next instead of binary. Then request a widget update to resubmit the last request.
    _onreconnected() {
        // Discard the deferred stuff and force the widget to refresh
        this.outstanding.clear()

        // Clear out the latest response target since we'll not expect json before any binary
        this.binary_response_target = null

        // Refresh the viewer
        this.pwidget.viewer.refresh_widget(this.pwidget, {force_no_sync:true}) // Request immediate refresh now that widget is following global time slider
    }

    // Send a request immediately, ignoring any deferring and order
    send_now(pwidget, request, callback) {
        this.ws.await_connection(() => {
            const jsonString = JSON.stringify(request)
            this.ws.send(jsonString)

            // Construct a callback to handle the response
            const req_time = performance.now()
            this.handlers[request.reqSeqNum] = new ResponseHandler(jsonString, (response_json) => {
                const latency_ms = performance.now() - req_time
                if (Options.TRACE_LATENCY)
                    time_log(`DataSource relaying oob response for widget id ${pwidget.widget_id} in ${latency_ms} ms`)

                // Provide data back to viewer
                callback(response_json, latency_ms)
            })
        })
    }

    request(pwidget, request, callback) {
        // Let the widget know it is waiting on a request
        pwidget.on_request()

        this.ws.await_connection(() => {
            // Count outstanding requests and defer this latest request of too many
            const data = {request: request, callback: callback}

            if (this.outstanding.num_outstanding >= OutstandingRequestTracker.Limit) { // Threshold
                // Must wait to submit this request because there are some outstanding
                // Replace any other deferred request
                //if (this.outstanding.deferred != null)
                //    time_log('DataSource dropping previous deferred request', this.outstanding.deferred, pwidget.widget_id, tracker.num_outstanding)

                //time_log('DataSource deferring request', json, pwidget.widget_id)
                this.outstanding.deferred = data
                this.outstanding.num_deferred++
                return true // connection OK
            }

            if (Options.TRACE_LATENCY)
                time_log('DataSource sending along request now for widget id', pwidget.widget_id)

            this._send(data)

        })
    }

    // Send the request and associate a response handler to it
    _send({request, callback}={}) {

        // Clear any deferred request because this current request will supersede it
        this.outstanding.deferred = null

        // Making a new request - count it as a new outstanding request
        this.outstanding.num_outstanding++
        this.outstanding.num_sent++
        this.num_requests++

        // Send the request bytes over the socket
        const jsonString = JSON.stringify(request)
        this.ws.send(jsonString)

        // Construct a callback to handle the response
        const req_time = performance.now()
        this.handlers[request.reqSeqNum] = new ResponseHandler(jsonString, (response_json) => {
            //time_log('DataSource receiving response for', this.txseqn, json, this.pwidget.widget_id, this.outstanding.num_sent, this.outstanding.num_deferred, this.outstanding.num_outstanding)
            this.outstanding.num_outstanding--

            // If something was deferred upon this response, send it out now that we're under the limit for outstanding requests
            if (this.outstanding.num_outstanding < OutstandingRequestTracker.Limit && this.outstanding.deferred != null) {
                //time_log('DataSource sending out deferred request', json, this.pwidget.widget_id, this.outstanding.num_sent)
                this._send(this.outstanding.deferred)
            }

            // Log performance
            const latency_ms = performance.now() - req_time
            if (Options.TRACE_LATENCY)
                time_log(`DataSource relaying response for widget id ${this.pwidget.widget_id} in ${latency_ms} ms`)

            // Provide data back to viewer
            callback(response_json, latency_ms)
        })
    }

    // Handle socket-message event
    _onmessage(msg) {
        // State machine to read some json first and if state says we need to read binary then read that
        if (this.binary_response_target == null) {
            // Read json packet
            //time_log('reading json packet', msg)
            let json
            try {
                json = JSON.parse(msg)
            } catch(error) {
                time_error('Failed to parse json from server. Got: ', msg, error.message, error.stack)
            }

            const handler = this.handlers[json.reqSeqNum]

            if (json.result == 'complete') {
                handler.set_json(json) // Save until binary gets here
                this.binary_response_target = json.reqSeqNum // Next packet will be binary
            } else if(json.result == 'error') {
                time_warn('Failed to get data: ', json.stackTrace, ' original request:', handler.request)
                handler.error()
                delete this.handlers[json.reqSeqNum]
            } else {
                time_error('Malformed response from server for getting data. Unknown result: ', msg)
                handler.error()
            }

        } else {
            // Read binary packet
            //time_log('reading binary packet for ', this.binary_response_target, msg)
            const handler = this.handlers[this.binary_response_target]
            delete this.handlers[this.binary_response_target]

            this.binary_response_target = null

            handler.invoke(msg) // Call back ResponseHandler with final json
        }
    }
}


// Communicates with server to make requests and route responses to callbacks asynchronously.
// Provides reconnect ability and automatically defers requests if there are too many outstanding requests
class WidgetDataSource {
    constructor(prefix) {
        this.prefix = prefix

        this.txseqn = 0

        this._latency_calc = new LatencyCalculator()
        this.latency = 0 // Latency in ms

        this._old_channel_requests = 0
        this.channels = {} // Connections by websocket
    }

    get_num_requests() {
        let num = this._old_channel_requests
        for (const pwidget in this.channels) {
            const channel = this.channels[pwidget]
            num += channel.num_requests
        }

        return num
    }

    _get_channel(pwidget) {
        let channel = this.channels[pwidget.widget_id]
        if (typeof channel == 'undefined') {
            // Need to drop old dead connections. Here is an ok place to do it
            for (const k in this.channels) {
                const channel = this.channels[k]
                if (channel.ws.is_over()) {
                    this._old_channel_requests += channel.num_requests
                    delete this.channels[k]
                }
            }

            time_log('Opening new websocket for widget ', pwidget.widget_id)
            channel = new WidgetChannel(this.prefix, pwidget)
            channel.onconnected = () => {}
            this.channels[pwidget.widget_id] = channel
        }
        return channel
    }

    // Make an oob request - usually from a widget.
    // This does not perform any deferrals and is effectively a different logical stream than the other "normal"
    // request()s.
    request_oob(pwidget, first, last, units, kwargs, callback) {
        if (Options.TRACE_LATENCY)
            time_log('Datasource received new OOB request for ', pwidget.widget_id, first, last, units)

        kwargs.units = units

        // Build request
        const request = {
            processorId : pwidget.processor_id,
            first       : first,
            last        : last,
            kwargs      : kwargs,
            reqSeqNum   : this.txseqn,
        }

        const channel = this._get_channel(pwidget)

        channel.send_now(pwidget, request, (response_json, latency_ms) => {
            callback(response_json, latency_ms)
        })

        return true // sent
    }

    // Request some data for a resource and supply data back to a callback
    // Time range is inclusive.
    // Returns true if connection is OK and request is scheduled/sent. Return false if need to reconnect
    // to be relatively infrequent.
    // Callback form is (data, latency_ms)
    request(pwidget, start_t, stop_t, units, branch_predictor_filtering, uid, callback) {
        //time_log('Datasource received new request for widget id', pwidget.widget_id, start_t, stop_t, units)

        // Global state available to the widget for its request
        const globals = {
            filters: {
                bp: branch_predictor_filtering,
            }
        }

        // Defer to widget to prepare its request
        const req = pwidget.prepare_request(units, start_t, stop_t, uid, globals)

        // SKip this request/update at the request of the widget
        if (req == null)
            return false

        // Validate
        if (typeof req.first == 'undefined')
            throw new Error('widget prepare_request must set "first" field')

        if (typeof req.last == 'undefined')
            throw new Error('widget prepare_request must set "last" field')

        if (typeof req.kwargs == 'undefined')
            throw new Error('widget prepare_request must set "kwargs" field')

        if (typeof req.kwargs.units == 'undefined')
            throw new Error('widget prepare_request must set "units" field within "kwargs" object')

        if (typeof req.reqSeqNum != 'undefined')
            throw new Error('widget prepare_request must not set reqSeqNum')

        if (typeof req.processorId != 'undefined')
            throw new Error('widget prepare_request must not set processorId')

        if (typeof req.reqSeqNum != 'undefined')
            throw new Error('widget prepare_request must not set reqSeqNum')

        // Append protocol fields
        req.processorId = pwidget.processor_id
        req.reqSeqNum   = this.txseqn++

        const channel = this._get_channel(pwidget)

        const actual_range = {
            first: req.first,
            last: req.last,
            units: req.kwargs.units,
        }

        channel.request(pwidget, req, (response_json, latency_ms) => {
            this.latency = this._latency_calc.update(latency_ms)
            callback(response_json, actual_range, latency_ms)
        })
    }
}
