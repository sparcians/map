
// Wrapper around websocket with auto-reconnect and await-connection functionality
class WebSocketWrapper {

    constructor(onmessage, prefix, endpoint='/ws/branchPredictor/getData') {
        this.onmessage = onmessage
        this.onreconnected = () => {} // when a connection is completed

        this.prefix = prefix
        this.endpoint = endpoint
        this.uri = prefix + endpoint

        // Count consecutive failed connections to delay reconnect attempts
        this.latest_failed_connection_attempts = 0

        // Allows clients to wait for connections to be ready before making further requests
        this.connection_hooks = []

        // Identify the session (number of connections) to prevent stale data from coming back in async calls
        this.session_num = 0

        // Queue of messages to forward in order
        this.queue = []

        // receive ordering id for messages in `queue`
        this.rxtoken = 0

        // Debug id
        this.id = WebSocketWrapper.connection_num++

        this._connect()
    }


    // Return a status string for the current connection
    status() {
        if (typeof this.ws == 'undefined' || this.ws == null) {
            return 'unknown'
        }
        if (this.ws.readyState == this.ws.CONNECTING) {
            return 'connecting'
        }
        if (this.ws.readyState == this.ws.OPEN) {
            return 'connected'
        }
        if (this.ws.readyState == this.ws.CLOSING) {
            return 'closing'
        }
        if (this.ws.readyState == this.ws.CLOSED) {
            return 'closed'
        }
        return 'other'
    }

    // Registers a one-time-use callback to be called asynchronously when socket is finally connected.
    // If socket is connected now, callback is invoked async with minimal delay.
    // Attempts to connect if currently disconnected and not busy opening a connection
    await_connection(callback) {
        if (this.ws.readyState == this.ws.OPEN) {
            setTimeout(callback, 1) // Call back async asap
        } else {
            if (this.ws.readyState != this.ws.CONNECTING) {
                this._reconnect() // Begin reconnect
            } else {
                //time_log(`Caller awaiting connection ${this.uri} but already connected`) // debug
            }
            const start_t = performance.now()
            this.connection_hooks.push(callback)
        }
    }

    // See if a the socket is open or not and log why if not
    check_open() {
        if (this.ws.readyState != this.ws.OPEN) {
            if (this.ws.readyState == this.ws.CONNECTING) {
                time_warn(`websocket ${this.uri} ${this.id} not ready to send. must wait for connection`)
            } else {
                time_warn(`websocket ${this.uri} ${this.id} not ready to send. code=${this.ws.readyState}, Must connect first`)
            }
            return false
        }
        return true;
    }

    is_over() {
        return this.ws.readyState == this.ws.CLOSING || this.ws.readyState == this.ws.CLOSED
    }

    // Send the bytes. Caller should have checked with check_open first
    send(bytes) {
        this.ws.send(bytes)
    }

    // Connect and assign a new this.ws while incrementing the session number to prevent old responses from being handled.
    _connect() {

        time_log(`Attempting to connect to ${this.uri} (${this.id})`)
        this.session_num++ // Count new session, invalidating data coming back from previous
        this.ws = new WebSocket(this.uri);

        const cur_session = this.session_num
        this.ws.onmessage = (e) => {
            if (this.session_num != cur_session) {
                // Reject this response because it comes from an old session (i.e. we reconnected) and have a new ws.
                // This implies that this tracker and stuff is no longer used and can just be left for the gc to deal with
                time_warn(`DataSource rejecting stale socket data from session ${cur_session} due to new session ${this.session_num}`)
                return
            }

            // Callback to owner
            this._onmessage(e)
        }

        this.ws.onopen = () => {
            this.latest_failed_connection_attempts = 0 // clear failed connection attempts
            this._onopen()
        }

        this.ws.onclose = (e) => {
            time_error(`websocket ${this.uri} ${this.id} closed`, e.code)

            this._reconnect()
        }

        this.ws.onerror = (e) => {
            time_warn(`websocket ${this.uri} ${this.id} error: ${e}`)

            // Errors should be followed by closes, so no need to do anything here
        }
    }

    // Reconnect after a connection is lost. This is _connect but with a delay back-off based on the number of failed attempts
    _reconnect() {

        // Delay the reconnect based on number of attempts so far
        this.latest_failed_connection_attempts++
        setTimeout(() => { this._connect() }, this.num_connections * 100)
    }

    // Handle socket-open event
    _onopen() {
        time_log(`websocket ${this.uri} ${this.id} connected`)

        // Notify observers
        for (const callback of this.connection_hooks) {
            callback()
        }
        this.connection_hooks = []

        this.queue = [] // clear the message ordering queue state because we are starting fresh

        if (this.session_num > 1)
            this.onreconnected()
    }

    _onmessage(e) {

        if (e.type == 'load')
            return // don't care

        if (typeof e.data == 'undefined') {
            time_warn('websocket get message with undefined data. This should be handled explicitly')
            return
        }

        // Handle strings
        if (typeof e.data == 'string') {
            if (this.queue.length > 0) {
                // Store the message and a placeholder for the binary message
                this.queue.push([e.data,0]) // 0=ready to send
            } else {
                this.onmessage(e.data)
            }
            return
        }

        // Handle Blobs (binary)
        // When we get a blob, it has to be read which is a deferred action. Put a placeholder in a queue to keep order.
        // When it the data for this blob is finally read (order is not guaranteed), we need to update the correct item
        // in the queue with the new data. Then we see if we can drain things from the queue at that point.
        const rxtoken = this.rxtoken++
        this.queue.push([rxtoken,1]) // push a placeholder. 1 means not ready
        const reader = new FileReader();
        reader.onload = (e) => {
            const msg = reader.result

            // Because readAsArrayBuffer results can come back in ANY order, be sure we only send on the front of the queue
            if (rxtoken == this.queue[0][0]) {
                // This result (msg) corresponds to the placeholder at the front of the queue so it can be sent now
                this.onmessage(msg)
                this.queue.splice(0,1)

                // Drain queue until next deferred event
                while (this.queue.length > 0) {
                    if (this.queue[0][1] == 1)
                        break // First item is a deferred event so cannot send

                    this.onmessage(this.queue[0][0])
                    this.queue.splice(0,1)
                }
            } else {
                // Because we got data for something not at the front of the queue, replace the q item corresponding to
                // this binary data with the result so it can be sent along to the consumer when it reaches the front of
                // the queue.
                for (i = 0; i < this.queue.length; i++) {
                    if (this.queue[i][0] == rxtoken) {
                        this.queue[i][0] = msg // replace with final binary array buffer
                        this.queue[i][1] = 0 // not deferred
                        break
                    }
                }
                if (i == this.queue.length) {
                    // Note that if we didn't find a matching token its probably because the websocket was reconnected
                    // and the queue was cleared.
                    time_warn('Unpacked a blob from a websocket but found place in the queue to put it. If this did not happen after a reconnect it may be a problem')
                }
            }
        }
        reader.readAsArrayBuffer(e.data);

    }
}

WebSocketWrapper.connection_num = 0