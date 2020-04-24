// Test widget
class CustomRequestWidgetTest extends BaseWidget {
    constructor(el, id) {
        super(el, id, 250)

        // Set up the widget
        this.jdiv = $(`<div style="overflow-y:scroll; height:100%"></div>`)
        this.jbody.html('<b>Event log </b>(latest event at top)<hr/>')
        this.jbody.append(this.jdiv)
    }

    _log(msg) {
        this.jdiv.html((msg + '<br/>' + this.jdiv.html()).slice(0, 10000))
    }

    on_clear_stats() {
        this._log(`[${performance.now().toFixed(1)}] ` + 'on_clear_stats()')

    }

    // got a new stat from the ui.
    on_add_stat(new_stat_name) {
        this._log(`[${performance.now().toFixed(1)}] ` + 'on_add_stat()')
    }

    // New data was received from the server
    on_update_data(json, meta) {
        // Note that the meta info is the original unmodified request
        this._log(`[${performance.now().toFixed(1)}] ` + `on_update_data(units: ${meta.units}, first: ${meta.first}, last: ${meta.last}) uid: ${meta.uid}, actual_range: ${meta.actual_range.units}, ${meta.actual_range.first}, ${meta.actual_range.last})`)
    }

    // Called when needs to re-render
    on_render(meta) {
        this._log(`[${performance.now().toFixed(1)}] ` + `render(units: ${meta.units}, first: ${meta.first}, last: ${meta.last}) uid: ${meta.uid}, actual_range: ${meta.actual_range.units}, ${meta.actual_range.first}, ${meta.actual_range.last})`)
    }

    // kwargs to pass to processor function when we make our requests
    get_request_kwargs() {
        this._log(`[${performance.now().toFixed(1)}] ` + 'get_request_kwargs()')
        return {stat_cols: ['correct'],
                max_points_per_series:5000}
    }

    // See BaseWidget for explanation
    prepare_request(units, first, last, uid, globals) {

        // Sometimes do not request
        if (first % 2 == 0) {
            this._log(`[${performance.now().toFixed(1)}] ` + `prepare_request(${units}, ${first}, ${last}, ${uid}) => null`)

            return null // No request
        }

        this._log(`[${performance.now().toFixed(1)}] ` + `prepare_request(${units}, ${first}, ${last}, ${uid}) => request`)

        const kwargs = this.get_request_kwargs()

        kwargs.units = units

        // Opt not to constrain
        //const [first, last] = this.data_source.constrain(units, first, last)

        // Just modify instead for the purpose of this test
        first = Math.floor(first / 1000) * 1000
        last = Math.floor(last / 1000) * 1000

        return {
            first: first,
            last: last,
            kwargs: kwargs,
        }

        // NOTE: A widget that makes only oob requests may return null here instead of an object
    }

}

CustomRequestWidgetTest.typename = 'TEST-bp-request-path'
CustomRequestWidgetTest.description = 'Test widget for custom requests'
CustomRequestWidgetTest.processor_type = Processors.SHP_LINE_PLOT
CustomRequestWidgetTest.data_type = DataTypes.SHP_TRAINING_TRACE
