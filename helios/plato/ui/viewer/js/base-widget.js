SyncMode = {
    FOLLOW: 'follow',
    MANUAL: 'manual',
}


class BaseWidget {
    constructor(element, id, default_height) {
        const self = this

        // Outermost DOM element for widget
        this.element = element

        this.widget_id = id

        // Name of widget
        this.name = element.getAttribute('plato-widget-name')

        // Type of this plato widget
        this.typename = element.getAttribute('plato-widget-type')

        this.default_height = default_height

        // Set element to default height
        $(this.element).height(default_height)

        // Build the basic widget structure
        // Include a .plato-widget-sort-handle  so the widget can be sorted
        $(element).html(`
            <div style="display:grid; width:100%; height:100%; grid-template-rows:20px 1fr; grid-template-areas: 'header' 'body';">
                <div class="plato-widget-header" style="grid-area: header;">
                    <div style="position:absolute; top: 0px; height: 20px; display: grid; width:100%; grid-template-columns:1fr auto auto auto auto auto auto auto 120px; grid-template-areas: '... status processor source latency filtering time-sync manual-time-sync buttons';">
                        <div id="manual-time-sync-div" class="widget-title-control-group" style="grid-area:manual-time-sync; margin-left: -9px; background-color: #cfdfff;">
                            <!-- the containing div here is moved to the left and in the DOM before time-sync-div so that it is underneath it and looks like an extension of it -->
                            <!--<div class="widget-title-control-group expansion"></div>-->
                            &nbsp;
                            <span class="widget-title-group-button" id="do-sync-resync" title="Causes the widget to refresh with the global time slider's current range">resync</span>
                            <span class="widget-title-group-button" id="do-sync-all" title="Causes the widget to refresh with the entire time-range available in open data-sources">all-data</span>
                        </div>
                        <div id="time-sync-div" class="widget-title-control-group" style="grid-area:time-sync; max-width:200px;">sync:
                            <span class="widget-title-group-option" id="sync-follow" title="Causes the widget will follow the global time-slider. This is sometimes disabled when widgets have a high calculation or redraw time">follow</span>
                            <span class="widget-title-group-option" id="sync-manual" title="Causes the widget to show the same data regardless of the global-time-slider.">manual</span>
                        </div>
                        <div class="widget-title-control-group" style="grid-area:filtering; max-width:800px;">filt:
                            <span class="widget-title-group-item" id="filter"></span>
                        </div>
                        <div class="widget-title-group" style="grid-area:status; max-width:800px;">status:
                            <span class="widget-title-group-item" id="status-msg"></span>
                        </div>
                        <div class="widget-title-group" style="grid-area:latency; max-width:200px;">
                            <span class="widget-title-group-item" id="latency" title="Latency of request to actual response-handling. This includes some client-side draw delays. Latency in parentheses is server internal latency "></span>
                        </div>
                        <div id="buttons" style="grid-area:buttons"></div>
                    </div>
                    <span class="ui-icon ui-icon-arrowthick-2-n-s plato-widget-sort-handle"></span>
                    <div class="plato-widget-datasource circle-icon" style="top:2px;"><div class="icon-label">.</div></div>
                    <span class="plato-widget-title">${this.name}</span>
                    <span class="plato-widget-typename">(${this.typename})</span>
                    <span class="plato-widget-id">#${this.widget_id}</span>
                </div>
                <div class="plato-widget-body" style="grid-area: body;"></div>
            </div>
            <div id="progress-bar" style="position:absolute; top: 18px; min-width:20px; height:2px; z-index: 10; width:100%; border: none;"></div>
        `)

        // Attach a hiding (loading processor) div
        this.hider = $(`<div style="position:absolute;top:var(--widget-header-height);left:0;width:100%;height:100%;background-color:#ffffd0c0;z-index:1000;display:none"></div>`)
        $(this.element).append(this.hider)

        // Attach a missing datasource msg div. Leave the corner resize handle of the widget visible at the bottom. z-index is 0 to still show data-sources dragged onto it.
        this.missing_data_source_div = $(`<div style="position:absolute; top:var(--widget-header-height); left:0; width:100%; height:calc(100% - 40px); background-color:#fff; z-index:0;">
                                            <div style="border: 2px dashed #ddd; margin: 10px; width:calc(100% - 20px); height:calc(100% - 20px);">
                                                <div style="position: absolute; left:50%; top:50%; transform: translate(-50%, -70%); color: var(--missing-stuff-text-color);">
                                                    <h2>No Data Source</h2>
                                                    <p>
                                                        Drag a data-source or specific statistic from the sources panel to this widget to begin using it,
                                                    </p>
                                                </div>
                                            </div>
                                          </div>`)
        $(this.element).append(this.missing_data_source_div)

        // Header jquery element
        this.jheader = $(element).find('.plato-widget-header')

        // Body jquery element
        this.jbody = $(element).find('.plato-widget-body')

        // Data-source icon
        this.jdatasource_icon = this.jheader.find('.plato-widget-datasource')

        // Progress bar indicating waiting on data
        this.wait_on_data_pbar = $(this.element).find("#progress-bar")
        this.wait_on_data_pbar.progressbar({
          value: false
        })
        //pbar.find('.ui-progressbar-value').css({'background-color': '#f88'}) // red
        this.wait_on_data_pbar.find('.ui-progressbar-value').css({'background-color': '#ff8'}) // yellow
        this.wait_on_data_pbar.css({display: 'none'})


        // Set up buttons
        const jbuttons = $(element).find('#buttons')
        const clone = $(`<div style="position:absolute; top:4px; right:100px" class="plato-collapse-button ui-icon ui-icon-newwin" title="Clone this widget"></div>`)
        clone.click(function() { self.clone() } )
        jbuttons.append(clone)
        const collapse = $(`<div style="position:absolute; top:4px; right:80px" class="plato-collapse-button ui-icon ui-icon-triangle-1-n" title="Collapse this widget to a single bar"></div>`)
        collapse.click(function() { self.collapse() } )
        jbuttons.append(collapse)
        const expand = $(`<div style="position:absolute; top:4px; right:60px" class="plato-expand-button ui-icon ui-icon-arrowthick-2-ne-sw" title="Expand this widget. If collapsed, expands back to previous size. If not collapsed, expands to default size."></div>`)
        expand.click(function() { self.expand() } )
        jbuttons.append(expand)
        const gear = $(`<div style="position:absolute; top:4px; right:40px" class="plato-settings-button ui-icon ui-icon-gear" title="open detailed widget settings"></div>`)
        gear.click(function() { self.settings() } )
        jbuttons.append(gear)
        const trash = $(`<div style="position:absolute; top:4px; right:20px" class="plato-close-button ui-icon ui-icon-trash" title="move widget to trash. It can be recovered from trash until this tab is closed or reloaded"></div>`)
        trash.click(function() { self.trash() } )
        jbuttons.append(trash)

        // Set up filtering control
        const filter_div = $(element).find('#filter')
        const item_none = $(`<span class="widget-title-group-option" id="filtering-none" title="Use branch predictor filtering">none</span>`)
        filter_div.html('')
        filter_div.append(item_none)
        item_none.on('click',()=>{
            item_none.addClass('selected')
            item_none.siblings().removeClass('selected')
            this._set_filtering(WidgetDataFiltering.NONE)
            this.viewer.refresh_widget(this, {force_no_sync: true}) // Request immediate refresh now that widget is following global time slider
        })
        if (this.supports_branch_trace_filtering()) {
            const item_bp = $(`<span class="widget-title-group-option" id="filtering-bp" title="Use branch predictor filtering">BP</span>`)
            filter_div.append(item_bp)
            item_bp.on('click', ()=>{
                this._set_filtering(WidgetDataFiltering.BP)
                this.viewer.refresh_widget(this, {force_no_sync: true}) // Request immediate refresh now that widget is following global time slider
            }).hover(
                  ()=>{ this.viewer.branch_filt_ctrl.start_pulsate() },
                  ()=>{ this.viewer.branch_filt_ctrl.stop_pulsate() })
        }
        this._set_filtering(WidgetDataFiltering.NONE) // Default

        // Populate body (placeholder)
        this.jbody.html(`uninitialized widget class: "${this.typename}"`)

        // Set up follow-mode control
        this._sync_mode = SyncMode.FOLLOW
        if (this.can_follow()) {
            this._set_sync_mode(SyncMode.FOLLOW)
            this.jheader.find('#sync-follow').on('click', ()=> {
                const prev_sync_mode = this._set_sync_mode(SyncMode.FOLLOW)
                if (prev_sync_mode != SyncMode.FOLLOW) {
                    this.viewer.refresh_widget(this) // Request immediate refresh now that widget is following global time slider
                }
            })
        } else {
            this._set_sync_mode(SyncMode.MANUAL)
        }
        this.jheader.find('#sync-manual').on('click', ()=> {
            const prev_sync_mode = this._set_sync_mode(SyncMode.MANUAL)
        })

        this.jheader.find('#do-sync-resync').on('click', ()=> {
            this.viewer.refresh_widget(this, {force_sync:true}) // Request immediate refresh to time-slider
        })
        this.jheader.find('#do-sync-all').on('click', ()=> {
            this.viewer.refresh_widget(this, {force_sync_all:true}) // Request immediate refresh to all-time
        })

        // Data-Response handling: delays data to coalesce it.
        // TODO: This should have adaptive latency. If drawing takes a long time, reconfigure the response buffer to wait longer
        this.response_buffer = new ResponseBuffer()

        // Statistics
        this.num_renders = 0
        this.num_data_updates = 0

        // Collapse/Expand
        this.last_height = null

        this.data_id = null // Assigned by viewer
        this.data_source = null
        this.processor_id = null // Assigned by viewer data-manager

        this._status_messages = new Set() // Status messages currently being displayed

        this._last_timings = null // Timings object from last data-update

        this._server_latency_ms = 0 // server internal latency reported by responses
        this._response_latency_ms = 0 // request->response latency
        this._buffered_latency_ms = 0 // response->on_data latency
        this._draw_start_latency_ms = 0 // on_data->draw
        this._draw_latency_ms = 0 // draw time

        this._last_request = null



        // State from data-updates
        this._needs_custom_update = true
        this._last_update = null

        // Show that the data-source is missing
        this._refresh_source_icon()
    }

    // Viewer command to make this widget droppable for stats/datasources
    make_droppable() {

        // Only allow drops of the appropriate data type
        const data_source_type_class = data_source_class(this.factory.data_type)
        const stat_class = stat_data_class(this.factory.data_type)

        $(this.element).droppable({
            accept: `.${stat_class}, .${data_source_type_class}`, // Accept data-sources & stats (which include data sources)
            classes: {
              "ui-droppable-active": "ui-state-active", // feedback when dragging
              "ui-droppable-hover": "plato-drop-hover" // feedback when hovering
            },
            drop: (event, ui) => {
                const new_data_id = ui.draggable[0].data_id

                if (ui.draggable.hasClass('stat-bin-item')) {
                    const new_stat_name = ui.draggable[0].stat_name
                    this.add_stat(new_data_id, new_stat_name)
                } else if(ui.draggable.hasClass('data-source-bin-item')) {
                    this._reassign_data(new_data_id)
                    $(this.element).effect('highlight') // positive feedback from the drag/drop
                }

                return true
            },
        })

    }

    _set_filtering(mode) {
        this._filtering = mode

        let item
        if (mode == WidgetDataFiltering.BP) {
            item = this.jheader.find('#filtering-bp')
        } else if (mode == WidgetDataFiltering.NONE) {
            item = this.jheader.find('#filtering-none')
        } else {
            throw `Filtering mode ${mode} has not ui support in base-widget`
        }

        item.addClass('selected')
        item.siblings().removeClass('selected')
    }

    _set_sync_mode(sync_mode) {
        const prev_sync_mode = this._sync_mode
        this._sync_mode = sync_mode

        if (!this.can_follow()) {
            this.jheader.find('#sync-follow').addClass('disabled')
        } else {
            this.jheader.find('#sync-follow').removeClass('disabled')
        }

        if (this._sync_mode == SyncMode.FOLLOW) {
            this.jheader.find('#sync-follow').addClass('selected')
            this.jheader.find('#sync-manual').removeClass('selected')

            this.jheader.find('#manual-time-sync-div').css('display', 'none')

        } else if (this._sync_mode == SyncMode.MANUAL) {
            this.jheader.find('#manual-time-sync-div').css('display', '')

            this.jheader.find('#sync-follow').removeClass('selected')
            this.jheader.find('#sync-manual').addClass('selected')
        }

        return prev_sync_mode
    }

    // Viewer interface

    // Is this widget currently following the global timer or is it manually-synced
    should_follow_global() {
        return this._sync_mode == SyncMode.FOLLOW
    }

    // Is this widget currently using a WidgetDataFiltering mode?
    should_use_filtering(mode) {
        return mode == this._filtering
    }

    // When viewer is updating widgets and a widget has should_follow_global() = false, this is called to see if a
    // custom update is needed.
    get_custom_update_range() {
        if (this._sync_mode != SyncMode.MANUAL)
            return null // If not in manual sync mode, widget will get updates normally

        if (!this._needs_custom_update)
            return null // After first update, no need

        return this._last_update // may be null if no data yet
    }

    // Get last update range for this widget. May be null before first update
    get_last_update() {
        return this._last_update
    }

    // Viewer marked us stale because we were not updated when global time updated
    mark_stale() {
        if (this._sync_mode == SyncMode.MANUAL) {
            this.jheader.addClass('stale')
            this.add_status_msg(BaseWidget.STALE)
        }
    }

    // Make this widget useless and stop it from handling more callbacks permanently
    expire() {
        this.viewer = null // Sever reference back to this viewer to make it clear that this is attached
        delete this.response_buffer
    }

    // Is the widget expired (unusable) or still alive? If not, try not to call functions on it.
    is_expired() {
        return this.viewer == null
    }

    gui_timer() {
        const el = $(this.element).find('#latency')

        // If there is a request that has not been satisfied recently, show it instead of the last request latency
        let latency
        if (this._last_request != null) {
            latency = (performance.now() - this._last_request)
            el.html(latency.toFixed(0) + 'ms')

            // If a small amount of time elapsed (longer than the typical server refresh time) then show the waiting
            // progress bar just to indicate to the user that "something" is going on. This will disappear when the
            // widget gets updated.
            if (performance.now() - this._last_request > 200) {
                this.wait_on_data_pbar.css({display: 'block'})

                // Above some larger time threshold, notify that we're still waiting on the server with more clear state
                if (performance.now() - this._last_request > 1000) {
                    this.add_status_msg(BaseWidget.WAITING_FOR_SERVER)

                    // Track that we've been waiting for a while so that we can flash the widget header to get the
                    // user's attention when its finally done.
                    this._waiting_for_a_while = true

                    // Show the yellow box covering the widget
                    //this.hider.css('display', 'block')

                    // May have been missing data but waiting for a response should be shown at this time.
                    // If response ends up empty, this missing-data status msg will be reapplied later.
                    this.remove_status_msg(BaseWidget.NO_DATA_FOR_TIME_RANGE)
                }
            }
        } else {
            latency = this._response_latency_ms + this._buffered_latency_ms + this._draw_start_latency_ms + this._draw_latency_ms
        }

        if (Options.DEBUG_LATENCY)
            el.html(`${latency.toFixed(0)}(${this._response_latency_ms.toFixed(0)}(${this._server_latency_ms.toFixed(0)})+${this._buffered_latency_ms.toFixed(0)}+${this._draw_start_latency_ms.toFixed(0)}+${this._draw_latency_ms.toFixed(0)}) ms`)
        else
            el.html(`${latency.toFixed(0)}(${this._server_latency_ms.toFixed(0)}) ms`)
        // hiding color because it is distracting // el.css('background-color', latency_to_color(latency))
    }

    on_request() {
        // Track this request in case server takes a long time to reply.
        this._last_request = performance.now()
    }

    // Assign the data set
    assign_data(data_id, data_source) {
        this.missing_data_source_div.css('display','none')

        this.data_id = data_id
        this.data_source = data_source

        //time_log(`assigned data ${data_id} ${data_source.name} ${data_source.stats} to widget ${this.name} ${this.typename}`)

        this._refresh_source_icon()

        // Do not notify the widget if assigned a null-datasource. Some widgets would have to be updated.
        if (this.data_source == null)
            return

        try {
            this.on_assign_data() // Notify widget
        } catch (error) {
            time_error(error.message, error.stack)
        }
    }

    // Add a stat to this widget (implementation supported)
    add_stat(data_id, stat_name) {
        if (!this._check_new_data_id(data_id)) {
            // User rejected
            return
        }

        this.on_add_stat(stat_name)
    }

    set_stats(data_id, stat_names) {
        if (!this._check_new_data_id(data_id)) {
            // User rejected
            return
        }

        this.on_clear_stats()
        for (const stat_name of stat_names) {
            this.on_add_stat(stat_name)
        }
    }

    // Assign a processor to this widget based on its processor type and data-source
    assign_processor(processor_id) {
        this.processor_id = processor_id

        this._refresh_source_icon()

        this.on_assign_processor()

        // Now that we have a processor/source, refresh
        this.viewer.refresh_widget(this)
    }

    has_processor() {
        return this.processor_id != null
    }

    can_request() {
        return this.processor_id != null && !this.is_collapsed()
    }

    clone() {
        this.viewer.clone_widget(this)
    }

    collapse() {
        const h = this.jheader.height()
        if ($(this.element).height() > this.jheader.height()) {
            this.last_height = $(this.element).height()
            $(this.element).height(h)
        }
    }

    expand() {
        if ($(this.element).height() <= this.jheader.height() && this.last_height != null) {
            $(this.element).height(this.last_height)
            this.last_height = null
            this.viewer.refresh_widget(this)
        } else if ($(this.element).height() < this.default_height) {
            $(this.element).height(this.default_height)
            this.viewer.refresh_widget(this)
        }
    }

    is_collapsed() {
        return $(this.element).height() <= this.jheader.height()
    }

    trash() {
        this.viewer.trash_widget(this)
    }

    // Show a modal settings dialog
    settings() {
        const bgdiv = $(`<div class="full-window-div" style="background-color:#aaaa;"></div>`)
        const dlg = $(`<div style="padding:8px; z-index: 2000; border-radius:8px; border: 1px solid black; background-color:white; min-width:200px; min-height:200px; position: fixed; left:50%; top:50%; transform: translate(-50%,-50%);
                        display:grid; grid-template-rows: auto 1fr auto; grid-template-areas: 'header' 'content' 'footer';">
                           <div style="grid-area: header;">
                                <span style="font-size:18px; font-weight:bold;">Widget Settings</span>
                                <hr/>
                                <!--<div style="position:absolute; top:10px; right:8px; cursor: pointer;" class="plato-close-button ui-icon ui-icon-closethick" title="close these settings"></div>-->
                           </div>
                           <div class="content" style="grid-area: content; height:100%;"></div>
                           <div style="grid-area: footer; text-align: center;">
                                <hr/>
                                <a href="javasript:void(0);" class="apply">ok</a>
                           </div>
                       </div>`)

        const container = dlg.find('.content')

        dlg.on('keyup', (e) => {
            if (e.key == "Enter") {
                try {
                    if (this.on_settings_applied(container)) {
                        cleanup()
                    }
                } catch (exception) {
                    cleanup()
                    throw exception
                }
            } else if (e.key == "Escape") {
                cleanup()
            }
        })

        const apply = dlg.find('.apply')
        apply.on('click', () => {
            try {
                if (this.on_settings_applied(container))
                    cleanup()
            } catch (exception) {
                cleanup()
                throw exception
            }
        })

        // Opting not to use close button for nows
        //const close = dlg.find('.plato-close-button')
        //close.on('click', () => { cleanup() })

        function cleanup() {
            bgdiv.remove()
            dlg.remove()
        }

        // Create the dialog
        try {
            $('body').append(bgdiv)
            $('body').append(dlg)

            // Allow the subclass to fill in the settings
            this.on_settings(container)
        } catch (exception) {
            cleanup()
            throw exception
        }
    }

    // Receive a custom data response from the server
    receive_oob_data(callback, obj) {
        // TODO: Track usage stats and warn about misuse
        callback(obj)
    }

    // One of the global filters changed
    global_filter_configuration_changed() {
        this.on_global_filter_configuration_changed()
    }

    // Update the draw data for the widget. Driven by the UI
    update_data(obj) {
        // Data result and query info
        const data = obj.json
        const first = obj.first
        const last = obj.last
        const units = obj.units
        const uid = obj.uid

        // Track timings
        const timings = obj.timings // from viewer
        timings.buffer_latency = performance.now() - timings.buffer_time
        timings.data_time = performance.now()
        this._last_timings = timings

        // State from data-updates. Stored for manual mode layout-saving
        this._last_update = {first: first, last: last, units: units}
        this._needs_custom_update = false

        if (this._sync_mode == SyncMode.MANUAL) {
            // Show manual button in a default color to indicate we are no longer stale
            // TODO: This just indicates a refresh. If the global slider/settings has moved SINCE then, we could
            //       actually be stale. This is a ui bug that needs to be fixed using a global settings version number
            //       or something like that.
            this.jheader.find('#sync-manual').addClass('selected')
        }
        this.remove_status_msg(BaseWidget.STALE)
        this.jheader.find('#sync-manual').removeClass('selected-warning')
        this.jheader.find('#sync-manual').html(BaseWidget.MANUAL_BUTTON_TEXT_DEFAULT)
        this.jheader.removeClass('stale')

        this.num_data_updates++
        this.on_update_data(data, obj)
    }

    render(obj) {
        if (this.is_collapsed())
            return // Skip draws when not visible (note that data is still updated)

        this.num_renders++

        if (this._last_timings != null) {
            this._last_timings.draw_start_latency = performance.now() - this._last_timings.data_time
        }
        const t = performance.now()
        try {
            this.on_render(obj)
        } finally {
            if (this._last_timings != null) {
                this._last_timings.draw_latency = performance.now() - t
                this._update_latency(this._last_timings)
            }
        }
    }

    notify_loading_processor() {
        this.hider.css('display', 'block')
        this.add_status_msg(BaseWidget.LOADING_PROCESSOR)
    }

    notify_loading_processor_done() {
        this.hider.css('display', 'none')
        this.remove_status_msg(BaseWidget.LOADING_PROCESSOR)
    }

    // Return a dictionary of data to configuring a generic widget (Widgets should NOT override this)
    get_internal_config_data() {
        const d = {
            height: $(this.element).height(),
            sync: {
                mode: this._sync_mode,
            },
            filtering: {
                mode: this._filtering,
            },
            data: {
                data_id: this.data_id,
                data_source_name_hint: (this.data_source != null ? this.data_source.name : ''), // Helper to restore old dataset in the case of redundancy
            }
        }
        // TODO: timeline association, etc.

        if (this._last_update != null) {
            d.sync.first = this._last_update.first
            d.sync.last = this._last_update.last
            d.sync.units = this._last_update.units
        }

        return d
    }

    // Apply data from get_internal_config_data that was saved
    apply_internal_config_data(d) {
        $(this.element).height(d.height) // Restore height

        // Restore slider/sync
        if (typeof d.sync != 'undefined') {
            this._set_sync_mode(d.sync.mode)

            if (typeof d.sync.first != 'undefined') {
                this._last_update = {
                    first: d.sync.first,
                    last: d.sync.last,
                    units: d.sync.units,
                }
            }
        }

        this._set_filtering((d.filtering && d.filtering.mode) || WidgetDataFiltering.NONE)

    }


    // Overridable Interface

    // Is it safe to make a thumbnail of this widget. Some widgets that contain very large DOM structures (e.g. giant
    // tables) are too slow to make a thumbnail and will fail while causing the browser to be unresponsive for a bit.
    // Those widgets should override this and return false
    is_thumbnail_safe() {
        return true
    }

    // Is the widget allowed to follow the global slider (this should be invariant).
    // Note that this is the capability of the widget - independent of whether it is currently in follow or manual sync
    // mode.
    can_follow() {
        return true
    }

    // Does this widget support branch trace filtering? This
    supports_branch_trace_filtering() {
        return false
    }

    // Get configuration information for the processor. Requested by the viewer to supply us a processor. This is called
    // any time a processor will be requested, usually only once per widget after construction.
    get_processor_kwargs() {
        return {}
    }

    // Return a dictionary of data specific to configuring this type of widget.
    // Subclasses should override this. This should generally only happen right after construction when loading the
    // widget for the first time
    get_config_data() {
        return {}
    }

    // Apply a dictionary of configuration data specific to this type of widget.
    // Subclasses should override this. This should generally only happen right after construction when loading the
    // widget for the first time.
    apply_config_data(d) {
    }

    // Prepare a request based on the input arguments and return it. The request must include first, last, and
    // kwargs, where kwargs has a units key. Generally, widgets receiving this will soon receive on_update_data
    // and then a render.
    //
    // Warning: overriding this method may cause your widget to lose functionality and break.
    //
    // For example, the following is a valid, minimal implementation:
    //     return { first: first, last: last, kwargs { units: units } }
    //
    // If the widget does not want to make a request at this time for this data, it may return null instead of a
    // request object. If returning null, this widget may not see on_update_data or on_render associated with this
    // request.
    //
    // Note that the metadata received later in on_update_data() and on_render() will be associated with the original
    // request range and units. If you want to view the modified range, use the metadata's actual_range field.
    //
    // The keys processorId and reqSeqNum must not exist in the returned object.
    prepare_request(units, first, last, uid, globals) {

        // Constrain the request to this widget's data-source's data-range
        const [constrained_first, constrained_last] = this.data_source.constrain(units, first, last)

        const kwargs = this.get_request_kwargs()

        kwargs.units = units

        // Add extra information to the query when supported by the widget and processor
        if (this.should_use_filtering(WidgetDataFiltering.BP)) {
            kwargs.branch_predictor_filtering = globals.filters.bp
        }

        return {
            first: constrained_first,
            last: constrained_last,
            kwargs: kwargs,
        }

        // NOTE: A widget that makes only oob requests may return null here instead of an object
    }


    // Subclass Interface. Do not call these directly

    // Called when any of the global filter configuration changed. Normally, widgets will automatically get new data
    // pushed when this happens but some widgets are always in manual mode and want to reflect the state of the filters
    // without requesting new data. Most widgets will never need to use this. This has nothing to do with this widget's
    // selected filter mode.
    on_global_filter_configuration_changed() {
    }

    // Modal settings dialog is being shown. Populate an empty container div with settings.
    // The container div should stretch to fit content. Don't forget newlines.
    // Changes made here should be reflected immediately (because it's usually simpler to implement).
    // TODO: We should have a collection of settings widgets stylized for plato.
    on_settings(container) {
        // It is recommended to use jquery append to this container so that one can just call up the inheritance tree
        // using super.on_settings(c) and get the parent options too.

        // Create a textbox for editing the widget name
        const name = $(`<input type="text" class="round-box" value=${this.name}>`)
        container.append('name: ')
        container.append(name)
        container.append('<br/>')
        name.on('keyup', () => {
            const new_name = name[0].value
            $(this.element).attr('plato-widget-name', new_name)
            this.name = new_name
            this.jheader.find('.plato-widget-title').html(new_name)
        })

        // Place initial focus here so that the user can just start typing and Enter/Esc dialog-box keybindings work.
        name.focus()

        //container.html('There are no settings for this widget')
    }

    // Modal settings dialog accepted (as opposed to cancelled)
    on_settings_applied(container) {
        // Settings were applied (if you weren't reflecting settings changes automatically)
        return true // Allow settings to close
    }

    on_clear_stats() {
        this.viewer.show_error_popup('Error',`on_clear_stat not implemented for ${this.name}`)
        $(this.element).effect('shake')
    }

    // Called when a new stat is added. data_id is guaranteed to match the stat if called through add_stat
    on_add_stat(stat_name) {
        this.viewer.show_error_popup('Error',`on_add_stat not implemented for ${this.name}`)
        $(this.element).effect('shake')
    }

    // Called at the start of widget resize
    on_resize_started() {
        // mostly harmless // time_error(`Widget ${this.name} (${this.typename}) does not implement on_resize_started()`)
    }

    // Called upon end of widget resize
    on_resize_complete() {
        // mostly harmless // time_error(`Widget ${this.name} (${this.typename}) does not implement on_resize_complete()`)
    }

    // Called when new data is received. This is usually followed by an on_render() event.
    on_update_data(data, meta) {
        time_error(`Widget ${this.name} (${this.typename}) does not implement on_update_data()`)
    }

    // Called when this widget should render itself
    on_render() {
        time_error(`Widget ${this.name} (${this.typename}) does not implement on_render()`)
    }

    // supply kwargs to pass to processor function
    get_request_kwargs() {
        return {}
    }

    // Notification that this.data_source has been assigned
    on_assign_data() {
    }

    // Notification that a processor was assigned to this widget
    on_assign_processor() {
    }


    // Protected

     _reassign_data(new_data_id) {
        // Clear data-source
        this.processor_id = null
        this.data_id = null
        this.data_source = null

        // Update this widgets' icon indicating source
        this._refresh_source_icon()

        // Request update for this widget's data
        this.viewer.reassign_data(this, new_data_id)

        // Get us a new processor too whenever that is done
        if (new_data_id != null)
            this.viewer.reload_widget_processor(this)
    }

    // Request data for specific range without moving the global slider.
    // This response arrives through the main on_update_data event
    // TODO: Widget may not know units, so that could be a problem
    // TODO: Widgets need to know about unit changes too so they can respond with new requests like this.
    _request_custom_update(first, last) {
        if (this._last_update == null) {
            time_warn(`Widget ${this.name} requested custom update but no last update was available`)
            return
        }

        this._last_update.first = first
        this._last_update.last = last

        this._needs_custom_update = true
        this.viewer.refresh_widget(this) // Refresh using this new selected range to get new data
    }

    // Causes a one-time out-of-band request with all data for the widget. Still uses the widget's
    // get_processor_kwargs() for the query, so downsampling will work.
    //
    // This requires the widget to actually have a data source (on_assign_data). So this cannot be called at
    // construction.
    //
    // Result will come back through given callback. There is no guarantee of ordering with respect to normal
    // data updates
    _request_oob_data(callback, {all_data=false, constrain=true, kwargs={}}={}) {
        this.viewer.request_oob_data(this, callback, {all_data:all_data, constrain: constrain, kwargs:kwargs}) // Refresh using this new selected range to get new data
    }


    // Private

    // Store latency of recent request response.
    // request_ms: total request to response-handling time
    // server_ms: server internal latency
    _update_latency(timings) {
        // TODO: Waiting for server should be removed based on no requests in the pipeline.
        this.remove_status_msg(BaseWidget.WAITING_FOR_SERVER)

        // If we've been waiting for a while, show a flash effect to draw attention
        if (this._waiting_for_a_while == true) {
            this.jheader.effect('highlight')
            this._waiting_for_a_while = false
        }

        // Remove the animated indeterminate progress bar
        this.wait_on_data_pbar.css({display: 'none'})

        // Clear the hider box that translucently covers the widget
        this.hider.css({display: 'none'})

        this._response_latency_ms = timings.request_latency
        this._buffered_latency_ms = timings.buffer_latency
        this._draw_start_latency_ms = timings.draw_start_latency
        this._draw_latency_ms = timings.draw_latency
        this._last_request = null // We got a response - show that responses latency in the gui instead of this

        if (timings.server_latency != null)
            this._server_latency_ms = timings.server_latency
    }

    // Update the data-source icon title
    _refresh_source_icon() {
        const ds_name = this.data_source != null ? this.data_source.name : 'none'
        this.jdatasource_icon.prop('title', `data: ${ds_name}\nprocessor: ${this.processor_id}`)

        if (this.data_source == null) {
            this.jdatasource_icon.find('.icon-label').html('?')
            this.jdatasource_icon.css('background-color', 'white')
        } else {
            this.jdatasource_icon.find('.icon-label').html(this.data_source.ui_letter)
            this.jdatasource_icon.css('background-color', this.data_source.ui_color)
        }

    }

    _check_new_data_id(data_id) {
         if (data_id != this.data_id) {
            // Cannot drag a stat and change data id at the moment (it would be a surprise)
            // TODO: Show a custom styled dialog asking if the user wants to replace the data-id
            const msg = 'This stat is from a different data source than the existing stats in this widget.\n\nAccepting this change will replace all stats currently in this widget.\nSupport for multiple data sources per widget may come later.\n\nProceed?'
            if (!confirm(msg)) {
                return false
            }

            // TODO: Reassigning the data and processor here is redundant.
            // The on_add_stat might request a new processor anyway (e.g. heatmap) which will take extra time
            this._reassign_data(data_id)
            this._stat_cols = []
        }

        return true
    }

    add_status_msg(msg) {
        this._status_messages.add(msg)
        this._update_status()
    }

    remove_status_msg(msg) {
        this._status_messages.delete(msg)
        this._update_status()
    }

    _update_status() {
        let msg = ''
        for (const item of this._status_messages) {
            if (msg.length == 0)
                msg += item
            else
                msg += '; ' + item
        }
        const el = $(this.element).find('#status-msg')
        if (msg.length > 0)
            el.html(msg)
        else
            el.html('ok')

        if (msg.length > 0) {
            el.css('background-color', Colors.WARNING)
        } else {
            el.css('background-color', Colors.NEUTRAL)
        }
    }
}

// Status messages
BaseWidget.LOADING_PROCESSOR = 'loading data processor'
BaseWidget.WAITING_FOR_SERVER = 'waiting for server response'
BaseWidget.NO_DATA_FOR_TIME_RANGE = 'no data in this source for selected time range'
BaseWidget.STALE = '[NOT SYNCED]'

// Other
BaseWidget.MANUAL_BUTTON_TEXT_DEFAULT = 'manual'
BaseWidget.MANUAL_BUTTON_TEXT_RESYNC = 'resync'