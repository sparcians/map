
// Represents a timeline drag-slider with labels and debounce.
// Currently, labels are implemented elsewhere
class TimeSliderComplex {

    // Construct with the jquery wrapper of the div
    // Debounce prevents successive on_slide events from firing more frequently than the given number of milliseconds
    constructor(jdiv, on_slide=function(first,last){}, debounce_ms=100) {
        const self = this

        this.jdiv = jdiv
        this.on_slide = on_slide
        this.debounce_ms = debounce_ms

        // Debounce state
        this.last_event = performance.now()
        this.deferred = null
        this.last_range = null

        const tmp_min = 0
        const tmp_max = 100000

        // Set up time-slider object
        this.llabel= this.jdiv.find('#time-slider-label-left')
        this.rlabel= this.jdiv.find('#time-slider-label-right')

        this.lhandle = this.jdiv.find('.ui-slider-handle')[0]
        this.rhandle = this.jdiv.find('.ui-slider-handle')[1]
        this.jdiv.find('#time-slider').dragslider({
            //animate: true,   // Works with animation - DISABLED for performance
            range: true,     // Must have a range to drag.
            rangeDrag: true, // Enable range dragging.
            step: 1,
            values: [tmp_min, tmp_max],

            // Upon construction of the time-slider
            create: function() {
                $(self.lhandle).text(numberWithCommas($( this ).dragslider( "values" )[0]));
                $(self.rhandle).text(numberWithCommas($( this ).dragslider( "values" )[1]));
            },

            start: function ( event, ui ) {

            },

            stop: function ( event, ui ) {
                // Replace the deferred event and force it to execute immediately
                self.deferred = function() { self._fire_event(ui.values[0], ui.values[1], performance.now()) }
                self._deferred_event(self.last_event)
            },

            // Continuous updates while slider the time-slider
            slide: function( event, ui ) {
                self._internal_on_slide(ui.values[0], ui.values[1])
            }
        })

        this.reset_extents(tmp_min, tmp_max) // Update labels and stuff
    }

    min() {
        return this.jdiv.find('#time-slider').dragslider('option', 'min')
    }

    max() {
        return this.jdiv.find('#time-slider').dragslider('option', 'max')
    }

    first() {
        return this.jdiv.find('#time-slider').dragslider('values')[0]
    }

    last() {
        return this.jdiv.find('#time-slider').dragslider('values')[1]
    }

    // Returns [first,last] range
    selection() {
        return this.jdiv.find('#time-slider').dragslider('values')
    }

    // Assign the selection
    set_selection(first, last) {
        first = Math.max(first, this.min())
        last = Math.min(last, this.max())
        first = Math.min(first, last) // keep first <= last

        this.jdiv.find('#time-slider').dragslider('values', [first, last])

        this._internal_on_slide(first, last) // Above does not trigger event
    }

    zoom_out_all() {
        this.set_selection(this.min(), this.max())
    }

    // Update the total range
    reset_extents(min, max) {
        const ds = this.jdiv.find('#time-slider')
        const vals = ds.dragslider('values')
        ds.dragslider('option', 'min', min)
        ds.dragslider('option', 'max', max)
        ds.dragslider('values', vals) // Restore values after setting min/max to fix slider location bug

        this.llabel.html(numberWithCommas(min))
        this.rlabel.html(numberWithCommas(max))
    }

    // Handle sliding
    _internal_on_slide(first, last) {
        const self = this

        $(this.lhandle).text(numberWithCommas(first))
        $(this.rhandle).text(numberWithCommas(last))

        const t = performance.now()
        if (t - this.last_event < this.debounce_ms) {
            // Schedule a deferred event if not already scheduled
            if (this.deferred == null) {
                const cur_last_event = this.last_event
                setTimeout(function() { self._deferred_event(cur_last_event) }, this.debounce_ms - (t - this.last_event))
            }

            // Last event was too recently. Defer this & schedule it to be called
            this.deferred = function() { self._fire_event(first, last, performance.now()) }
        } else {
            // Fire now and clear anything staged
            this._fire_event(first, last, t)
        }
    }

    _deferred_event(expected_last_event) {
        if (this.last_event != expected_last_event) {
            return; // We've moved on from the time associated with this event
        }

        if (this.deferred == null) {
            return; // Deferred event was already handled
        }

        this.deferred() // Fire

    }

    _fire_event(first, last, t) {
        this.last_event = t // reset last-event time
        this.deferred = null // Nothing deferred since event is firing now with latest data

        // Only fire if not a redundant event
        if (this.last_range == null || this.last_range[0] != first || this.last_range[1] != last) {
            this.last_range = [first, last]
            this.on_slide(first, last)
        }
    }
}