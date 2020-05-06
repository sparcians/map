
function sleep(ms) {
    return new Promise(r => setTimeout(r, ms))
}

async function run_stress_test() {
    const output = $('#stress_output')
    output.html('')

    function log (msg) {
        const t = performance.now().toFixed(0)
        output.append(`<span class='msg'>[${t}ms] ${msg}</span><br/>`)
    }
    function err (msg) {
        const t = performance.now().toFixed(0)
        output.append(`<span class='error'>[${t}ms] ${msg}</span><br/>`)
    }
    function yay (msg) {
        const t = performance.now().toFixed(0)
        output.append(`<span class='happy'>[${t}ms] ${msg}</span><br/>`)
    }

    // Test config
    const uri = $('#uri')[0].value
    const processor_id =            $('#processor_id')[0].value
    const num_requests =              parseInt($('#num_reqs')[0].value)
    const request_report_frequency =  parseInt($('#req_report_freq')[0].value)
    const response_report_frequency = parseInt($('#resp_report_freq')[0].value)
    const test_timeout_ms =           parseInt($('#test_timeout_s')[0].value) * 1000

    // Initial request data
    let first = 0
    let last = 999

    // Number of responses collected during test
    let num_responses = 0

    // Timing vars
    let start_time
    let send_end_time
    let receive_end_time

    log('Attempting to connect to ' + uri)
    const ws = new WebSocket(uri);

    function abort() {
        ws.close()

        // server can process on 1st message so count ALL time for this calc.
        // This *might* be flawed though since server may be waiting for us to drain messages.
        const abort_time = performance.now()
        const duration = (abort_time - start_time)
        const ms_per_msg = (duration / num_responses).toFixed(5)

        ws.close()
        err(`Aborted due to timeout. ${num_responses} responses received at ${ms_per_msg} ms/msg (this is not latency)`)
    }

    ws.onmessage = async function() {
        num_responses++
        if (num_responses == 1) {
            yay(`Received first response`)
            await sleep(1)
        }
        if (num_responses % response_report_frequency == 0) {
            log(`Received ${num_responses} responses so far`)
            await sleep(1)
        }
        if (num_responses == num_requests) {
            receive_end_time = performance.now()
            const duration = (receive_end_time - send_end_time).toFixed(0)
            yay(`Received all responses in ${duration} ms`)
        }
    };

    ws.onopen = async function() {
        // Begin test by sending all requests. Note that without sleeping here, there is no chance to handle responses
        log(`Beginning test. Will send ${num_requests} requests for processorId '${processor_id}' and test will timeout after ${test_timeout_ms} ms`)
        start_time = performance.now()
        for (let i = 0; i < num_requests; i++) {
             // Build request
            const json = JSON.stringify({
                'processorId': processor_id,
                'first' : first,
                'last' : last
            })
            first++
            last++

            // Send
            ws.send(json)

            // Report and continue
            if (i > 0 && (i % request_report_frequency) == 0) {
                log(`Sent ${i} messages so far`)
                await sleep(1)
            }
        }

        // Test complete
        send_end_time = performance.now()
        const duration = (send_end_time - start_time).toFixed()
        log(`Took ${duration} ms to send all ${num_requests} requests`)

        // Schedule a timeout to abort test
        setTimeout(() => { abort() }, test_timeout_ms)
    };

    ws.ondisconnect = () => {
        err('websocket disconnected')
        abort()
    }

    ws.onerror = (e) => {
        err('websocket error: ' + e)
        abort()
    }
}


