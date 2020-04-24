// Schedules draws with a delay between draws in order to allow more recent data to arrive and possibly
// replace stale data.
class DrawScheduler {

    // delay_ms indicates how longer after data we should wait to begin drawing things.
    constructor(delay_ms) {
        this.delay_ms = delay_ms

        this.scheduled = false
        this.to_draw = {}
        this.ordering = []

        this.num_draws = 0
    }

    // Add a new widget to draw list if not already added.
    // Schedule a new draw if not already scheduled.
    schedule(pwidget) {
        if (Options.TRACE_LATENCY)
            time_log('scheduled draw for widget id', pwidget.widget_id)
        if (!this.to_draw.hasOwnProperty(pwidget.widget_id)) {
            this.to_draw[pwidget.widget_id] = 1
            this.ordering.push(pwidget)
        }

        // Schedule if not scheduled yet
        if (!this.scheduled) {
            if (Options.TRACE_LATENCY)
                time_log('new draw had to be scheduled')
            setTimeout(() => { this._draw() }, this.delay_ms)
            this.scheduled = true
        }
    }

    // Handle a draw event
    _draw() {
        if (Options.TRACE_LATENCY)
            time_log(`draw #${this.num_draws} items=${this.ordering.length}`)
        this._draw_recursive()
    }

    // Recursively draw, pausing briefly between each item to allow new data to arrive so that we don't draw with old
    // data
    _draw_recursive() {
        // Allow new widgets to be added while looping, though any widget can be added at most once
        if (this.ordering.length > 0) {

            // Take first widget from list
            const pwidget = this.ordering[0]
            this.ordering.splice(0,1) // Drop [0]

            try {
                if (!pwidget.response_buffer.has_data())
                    time_warn(`widget ${pwidget.name} has no data buffered but it was scheduled by draw`)

                if (Options.TRACE_LATENCY)
                    time_log('finally updating data and drawing widget id', pwidget.widget_id)

                try {
                    // Apply response(s) data to the widget (indirectly calls update_data)
                    pwidget.update_data(pwidget.response_buffer.latest)

                    // Render the widget (passing the same data for reference)
                    pwidget.render(pwidget.response_buffer.latest)
                } finally {
                    pwidget.response_buffer.clear()
                }

            } catch (error) {
                time_error(error.message, error.stack)
            }

            // Pause to allow some more data responses to be handled since they may overwrite data.
            // Note that this is still 'scheduled=true' and to_draw is not cleared so there is no repeat renders
            // during this _draw event
            setTimeout(() => { this._draw_recursive() }, 1)
            return
        }

        this.num_draws++
        this.scheduled = false
        this.to_draw = {}
        this.ordering = []
    }
}
