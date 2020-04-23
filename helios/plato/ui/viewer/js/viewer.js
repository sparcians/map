// Represents a range of time
class TimeRangeRepr {
    constructor(unit_id) {
        this.unit_id = unit_id
        this.first = 0
        this.last = 54321
    }
}


// Note: construct during or after document onload
// Note: we are now using Django for logins, so much of this is dead code
class LoginDialog {
    constructor(error_cmd) {
        this._error_cmd = error_cmd
        this._login_promise_resolve = null

        this._login_dialog = $('#login-dialog').dialog({
            autoOpen: false,
            width: 700,
            modal: true,
            buttons: {
                "Login": () => {
                    this._attempt()
                },
                //Cancel: () => {
                //    this._save_layout_dialog.dialog('close')
                //    this._login_promise_resolve(null)
                //}
            },
        })

        $('#login-dialog').keypress((e) => {
            if (e.keyCode == $.ui.keyCode.ENTER) {
                this._attempt()
            }
        })

        // Hack to disable 'x' close button on the dialog
        $('#login-dialog').parent().find('.ui-dialog-titlebar-close').css('visibility', 'hidden')
    }

    // Show login box and get a response
    // Returns a promise so that this can be done modally async
    // Note: we are using django for login now, not this Javascript
    login(pre_fill_username='') {
        $('#login-username')[0].value = pre_fill_username
        const promise = new Promise((res,rej) => {
            this._login_promise_resolve = res;
        })
        this._login_dialog.dialog('open')
        return promise
    }

    _attempt() {
        const username = $('#login-username')[0].value
        if (this._validate(username)) {
            this._login_dialog.dialog('close');
            this._login_promise_resolve(username)
        }
    }

    _validate(username) {
        if (username == '') {
            this._error_cmd('Invalid username', 'Username must not be an empty string')
            return false
        }

        return true
    }

}


// Basic Widget viewer. `viewer` should be global accessible
const viewer = new function () {

    // List of active/renderable widgets
    this.pwidgets = []

    // Unique id to use for next new widget
    this.next_widget_id = 1

    // Scheduler for coalescing data arriving at different times into a single draw loop to maximize discard of stale
    // data in favor of new data and prevent different widgets from redrawing at different rates.
    this.draw_scheduler = new DrawScheduler(50)

    // Id for each widget request (not oob)
    this._request_uid = 0

    // How to create classes backing various widget. Lookup by plato-widget-type
    this.widget_factory = {
        [HeatMapWidget.typename]:            HeatMapWidget,
        //[MinimalHeatMapWidget.typename]:     MinimalHeatMapWidget, // Incomplete
        [BranchHistoryTableWidget.typename]: BranchHistoryTableWidget,
        [BranchProfileTableWidget.typename]: BranchProfileTableWidget,
        [BpLinePlotWidget.typename]:         BpLinePlotWidget,
        [SimDbLinePlotWidget.typename]:      SimDbLinePlotWidget,
        [SimDbCorrelationWidget.typename]:   SimDbCorrelationWidget,
        [PeventHeatmapWidget.typename]:      PeventHeatmapWidget,
        [PeventWaterfallWidget.typename]:    PeventWaterfallWidget,
    }

    if (Options.SHOW_TEST_WIDGETS) {
        this.widget_factory[CustomRequestWidgetTest.typename] = CustomRequestWidgetTest
    }

    // Validate widget factories
    for (const name in this.widget_factory) {
        const factory = this.widget_factory[name]

        console.assert('typename'       in factory, `Widget class ${name} is missing static attribute "typename". Widget will not function correctly without this`)
        console.assert('description'    in factory, `Widget class ${name} is missing static attribute "description". Widget will not function correctly without this`)
        console.assert('processor_type' in factory, `Widget class ${name} is missing static attribute "processor_type". Widget will not function correctly without this`)
        console.assert('data_type'      in factory, `Widget class ${name} is missing static attribute "data_type". Widget will not function correctly without this`)
    }

    this.last_win_dims = null

    this.min_widget_height = 20

    // Handle internal periodic timer
    this._internal_gui_timer = function () {
        // This is a periodic internal timer for stat updates and heartbeats.
        // It should deal with passive GUI updates that do not affect data or state.
        // It should not deal with any data-handling, processing, or widget management because asynchronous callbacks
        // are better for that in most cases

        function status_color(status) {
            if (status == 'connecting')
                return Colors.WARNING
            else if (status == 'connected')
                return Colors.GOOD
            else
                return Colors.ERROR
        }

        $('#server-num-requests').html(this.data_provider.get_num_requests() + ' reqs')
        $('#server-latency').html(this.data_provider.latency.toFixed(0) + 'ms')

        // Datasource info (if created)
        if (typeof this.data_manager != 'undefined') {
            $('#data-source-path').html(squish_string(this.data_manager.data_dir, 60))
            $('#data-source-path').attr('title', this.data_manager.data_dir)
            $('#data-source-count').html(this.data_manager.get_num_sources())
            const source_status = this.data_manager.status()
            const jsource_status = $('#data-server-status')
            jsource_status.html(source_status)
            jsource_status.css('background-color', status_color(source_status))
        }

        // Show notice of no widgets if appropriate
        if (this.pwidgets.length == 0) {
            $('#no-widgets-explanation').css('display', 'block')
        } else {
            $('#no-widgets-explanation').css('display', 'none')
        }

        for (const pwidget of this.pwidgets) {
            pwidget.gui_timer()
        }

        // Reschedule
        setTimeout(() => { this._internal_gui_timer() }, 400) // should be low enough to make things look responsive but not so low that the updates look animated or take up too much of time
    }


    // EVENTS

    // DOM is fully loaded. Set things up
    this.on_load = async function() {
        try {
            this.prefix = $("#asgiServer")[0].value

            // Data source to which requests for data are made
            this.data_provider = new WidgetDataSource(this.prefix)

            $('#server-uri').html(this.data_provider.prefix)

            const self = this
            this.time_slider = new TimeSliderComplex($('#time-slider-complex-1'), function(f,l) { self.on_time_change(f,l) })

            // Hook up animation controls
            const anim_btn = $('#animation-run-stop')
            this.animation = {playing: false, original_button_text: anim_btn[0].value}
            const stop_animation = () => {
                clearInterval(this.animation.interval)
                this.animation.interval = null
                this.animation.playing = false

                anim_btn[0].value = this.animation.original_button_text
                anim_btn.css('background-color', '')
                $('#animation-steps-per-second').prop('disabled', false)
                $('#animation-step-size').prop('disabled', false)
            }
            anim_btn.on('click', () => {
                if (this.animation.playing) {
                    stop_animation()
                } else {
                    try {
                        const ups_str = $('#animation-steps-per-second')[0].value
                        if (/^[0-9]+$/.test(ups_str) == false) {
                            throw 'Updates per second must be an integer'
                        }
                        const updates_per_second = parseInt(ups_str)
                        if (updates_per_second > 99 || updates_per_second < 1)  {
                            throw  'Updates per second must be an integer in range [1,99]'
                        }
                        const ss_str = $('#animation-step-size')[0].value
                        if (/^[0-9]+$/.test(ss_str) == false) {
                            throw 'Update step size must be an integer'
                        }
                        const step_size = parseInt(ss_str)
                        if (step_size < 1)  {
                            throw  'Updates per second must be an integer > 0]'
                        }

                        // Note that there is no need to disable the slider, panels, etc.
                        var first_step_size = step_size;
                        var last_step_size = step_size;
                        if ($(".rs-select-button:checked")[0].id == "rs-play-left") {
                            last_step_size = 0;
                        } else if ($(".rs-select-button:checked")[0].id == "rs-play-right") {
                            first_step_size = 0;
                        }

                        // Kick off the animation timer
                        this.animation.interval = setInterval( () => {
                            const orig_first = this.time_slider.first()
                            const orig_last = this.time_slider.last()
                            const delta = orig_last - orig_first - first_step_size + last_step_size

                            let last = orig_last + last_step_size
                            last = Math.min(last, this.time_slider.max())
                            let first = Math.min(orig_first + first_step_size, last - delta)

                            this.time_slider.set_selection(first, last)

                            if ((last_step_size > 0 && last >= this.time_slider.max()) || first >= last) {
                                stop_animation()
                            }
                        }, 1000 / updates_per_second )
                        this.animation.playing = true

                        // Effects
                        anim_btn[0].value = 'stop'
                        anim_btn.css('background-color', '#f88') // make the button red for now
                        $('#animation-steps-per-second').prop('disabled', true)
                        $('#animation-step-size').prop('disabled', true)
                    } catch (error) {
                        this.show_error_popup(error.message, error.stack)
                    }
                }
            })

            // decorate the play options boxes
            $(".rs-select-button").each(function() { $(this).checkboxradio({
                icon: false
            }) });

            // Create and hide the load-layout dialog
            this.load_layout_dialog = $('#load-layout-dialog').dialog({
                autoOpen: false,
                height: 280,
                width: 600,
                modal: true,
                buttons: {
                    "Load Layout": function() {
                        self._load_layout_dialog_submit()
                    },
                    Cancel: function() {
                        self.load_layout_dialog.dialog( "close" )
                    }
                },
                close: function() {
                }
            })

            // Create and hide the save dialog
            this.save_layout_dialog = $('#server-save-layout-dialog').dialog({
                autoOpen: false,
                height: 300,
                width: 600,
                modal: true,
                buttons: {
                    "Save Layout": function() {
                        self._save_layout_dialog_submit()
                    },
                    Cancel: function() {
                        self.save_layout_dialog.dialog( "close" )
                    }
                },
                close: function() {
                    $('#server-save-filename')[0].value = ''
                    $('#server-save-overwrite').attr('checked', false)
                }
            })

            // Create and hide the save dialog
            this.login_dialog = new LoginDialog((msg, detail) => { this.show_error_popup(msg,detail) })

            this.share_layout_dialog = $('#share-layout-dialog').dialog({
                autoOpen: false,
                height: 310,
                width: 700,
                modal: true,
            })

            // Set up the data-panel items to be collapsible
            const sections = $('.data-panel-section-header').click(function() {
                $(this).next().toggle('fast')
                const a = $(this).find('.ui-icon-triangle-1-n')
                const b = $(this).find('.ui-icon-triangle-1-s')
                a.removeClass('ui-icon-triangle-1-n').addClass('ui-icon-triangle-1-s')
                b.removeClass('ui-icon-triangle-1-s').addClass('ui-icon-triangle-1-n')
                return false;
            })
            for (const el of sections) {
                const section = $(el)
                section.next().addClass('data-panel-section-content')
                if (el.hasAttribute('start-collapsed')) {
                    section.prepend($('<span class="ui-icon ui-icon-triangle-1-s"></span>'))
                    section.next().height(200) // Give  these an initial height - otherwise they take up more than 100%
                    section.next().toggle('fast') // collapse
                } else {
                    section.prepend($('<span class="ui-icon ui-icon-triangle-1-n"></span>'))
                }

            }

            // Initialize height first because widget bins need to take up 100% to work but when the page is initialized it
            // takes up too much space.
            const min_panel_width = $('#data-panel-sections').width() - 2 // For some reason this extra 2 is required to prevent oversizing. Not sure why
            $('#data-panel-sections').find('.data-panel-section-content').height(200).resizable({
                minWidth: min_panel_width,
                maxWidth: min_panel_width,
                minHeight: 150,
                start: function (evt, ui) {

                },
                stop: function (evt, ui) {

                },
            })

            // Set up the new-widget bin
            const nwbin = $('#new-widget-bin')
            for (const k in this.widget_factory) {
                const factory = this.widget_factory[k]
                const typename = factory.typename
                const desc = factory.description
                const data_class = widget_data_class(factory.data_type) // Get a classname that can be used by droppables to filter on datasource type
                const new_widget_item = $(`
                    <div class="new-widget-bin-item ${data_class}" title="${desc}">
                        <span class="ui-icon ui-icon-plusthick"></span>
                        <b>${typename}</b><br/><span class="smalltext">${desc}</span>
                    </div>`)
                new_widget_item.click((event) => {
                    this.add_new_widget_from_factory(factory)
                    this._post_add_widgets()
                })
                new_widget_item[0].factory = factory
                nwbin.append(new_widget_item)
            }

            // Make the new-widget bin items droppable, but do this on a per-data-type so make sure we can only drop onto
            // widgets supporting this data type
            for (const dtype_id in DataTypes) {
                const data_type = DataTypes[dtype_id]

                $('.' + widget_data_class(data_type)).droppable({
                    accept: '.' + stat_data_class(data_type), // Accept stats only (for now). Could easily add data sources when widgets require it
                    classes: {
                      "ui-droppable-active": "ui-state-active", // feedback when dragging
                      "ui-droppable-hover": "ui-state-hover" // feedback when hovering
                    },
                    drop: function(event, ui) {
                        const factory = $(this)[0].factory
                        const data_id = ui.draggable[0].data_id
                        const pwidget = self.add_new_widget_from_factory(factory, {data_id:data_id})

                        if (ui.draggable.hasClass('data-source-bin-item')) {
                            // Nothing to do here
                        } else { // stat-bin-item
                            pwidget.set_stats(data_id, [ui.draggable[0].stat_name]) // Add this initial stat
                        }
                        self._post_add_widgets()
                    },
                })
            }

            // Set up the branch filtering
            this.branch_filt_ctrl = new BranchFilterControlPanel($('#branch-class-filtering'),
                                                                (filter)=>{
                                                                    this._refresh_all_widgets({bpfilter_changed: true})
                                                                },
                                                                (msg, detail)=>{ this.show_error_popup(msg,detail) })


            // Initial widgets
            // ==========================================
            //this.add_new_widget_from_factory(HeatMapWidget)
            ////this.add_new_widget_from_factory(MinimalHeatMapWidget)
            //this.add_new_widget_from_factory(SimpleLinePlotWidget)
            //this.add_new_widget_from_factory(BranchHistoryTableWidget)

            // Make widget container sortable (draggable widgets on the y axis)
            $('#widget-area').sortable({
                axis: 'y', // vertical only
                items: 'div.plato-widget:not(.ui-state-disabled)',
                handle: '.plato-widget-sort-handle',
            })

            // Show notice of no widgets if appropriate.
            // This is usually not needed but just in case the above is changed to not use any widgets
            if (this.pwidgets.length == 0) {
                $('#no-widgets-explanation').css('display', 'block')
            }

            this.popup_div = $('#message-popup')
            this.popup_div.hide()

            this.enable_widget_resize() // Do this last since it inserts some stuff into widget divs

            // Log in (actually "get username" since we've already authenticated through django before we get here
            this.username = await this.do_login(true)
            $('#user-username').html(this.username)

            // print the SHA1
            console.log("This page was generated by SHA1=" + $('#sha1')[0].value)

            // Open up layout for user
            this.layout_io = new LayoutIO(this.prefix, this.username, (msg, detail)=>{ this.show_error_popup(msg,detail) })

            // Start internal timer
            this._internal_gui_timer()

            const qparams = get_url_vars()

            // Get the data ready asynchronously
            const wait_title = 'Getting Data-Sources'
            this.deactivate_widgets(wait_title, 'Please wait')

            // TODO: Load data sources based on a menu
            const data_source_dirs = (typeof qparams.dataSourceDir == 'undefined')
                                ?
                                ['/please/use/dataSourceDir/to/set/search/path']
                                : qparams.dataSourceDir.split(',')
            this.data_manager = new DataManager(this.prefix,
                                                data_source_dirs[0],
                                                (msg, detail)=>{ this.show_error_popup(msg,detail) },
                                                (status)=>{ this.deactivate_widgets(wait_title, status) })

            const browser = new RemoteDirBrowser(this.prefix, (msg, detail)=>{ this.show_error_popup(msg,detail) })
            for (const data_dir of data_source_dirs) {
                let obj = await browser.get(data_dir)

                //obj.subdirectories

                // Load all sources into plato
                await this.data_manager.append_sources(obj.source_file_names)
                time_log('done with appending sources?')
            }

            // TODO: This should require a hook on reconnect too.. so that data-sources & processors can be re-requested immediately
            //await this.data_manager.refresh((ms)=>{ this.deactivate_widgets('Getting data-sources', 'Receiving Data') },
            //                                {data_dirs: data_source_dirs}) // TODO: Display Wait Time using 2nd callback

            // TODO: Load sources here by selection

            this.on_data_loaded()

            // Load an initial layout
            try {
                if (typeof qparams.loadLayoutUser != 'undefined' && typeof qparams.loadLayoutName != 'undefined') {
                    this.deactivate_widgets('Loading initial layout', 'Please wait')
                    const layout = await this.layout_io.load(qparams.loadLayoutName, qparams.loadLayoutUser)
                    if (layout != null) {
                        this._load_json_layout(layout)
                    }
                } else {
                    // Load the user's last autosave layout
                    this.deactivate_widgets('Getting last layout', 'Please wait')
                    const layout = await this.layout_io.load_autosave()
                    if (layout != null) {
                        this._load_json_layout(layout)
                    }
                }
            } catch (error) {
                time_warn(error, error.stack)
            } finally {
                this.reactivate_widgets() // Unhide things
            }

            // Get a list of layouts asynchronously
            this._refresh_layouts()

            // Start autosaving periodically
            // TODO: autosave upon any change (e.g. slider, etc). At least only save if there is a mouse movement since the
            //       last save.
            setInterval(() => { this.autosave() }, 30000)

        } catch (error) {
            time_error(error.message, error.stack, error)
            alert('Unexpected error during loading. Please report issue along with the developer console log output')
        }
    }

    // Async function that returns a new login name
    //  TODO: This function appears to be commented out.  Why is it here? I think originally
    //        that plato was pure javascript and then it was integrated with Django.  Perhaps
    //        the javscript login dialog and code are just cruft.  The only thing it now needs
    //        is to get the username.  Not yet sure where that came from.

    this.do_login = async function(skip_if_cookie_valid) {
        if (false) {
            const cookie_username = Cookies.get(CookieNames.USERNAME)
            let assumption
            if (typeof cookie_username == 'undefined') {
                assumption = ''
            } else if(skip_if_cookie_valid) {
                return cookie_username
            } else {
                assumption = cookie_username
            }
            const username = await this.login_dialog.login(assumption)
            Cookies.set(CookieNames.USERNAME, username)
            return username
        } else {
            return $("#username")[0].value;
        }
    }


    this.on_resize = function() {
        const win = $(window)
        const w = win.width()
        const h = win.height()

        // Update widget area only if width changes (since window height doesn't effect widget height)
        if (this.last_win_dims == null || this.last_win_dims[0] != w) {
            this.enable_widget_resize()
        }

        this.last_win_dims = [w,h]
    }

    // TEMP: take first currently-known source and assume its the one we want until told otherwise.
    // TODO: This needs to be smarter and associate only appropriate sources based on type (this is not known atm)
    this._assign_existing_data_id = function (pwidget) {
        const data_id = this.data_manager.find_data_of_type(pwidget.factory.data_type)

        if (data_id == null)
            return

        pwidget.assign_data(data_id, this.data_manager.get_data(data_id))
    }

    // Called when data & time ranges are ready
    this.on_data_loaded = function() {
        this.reactivate_widgets() // Unhide things

        this._on_partial_data_loaded()

        // Populate the units combobox
        const unitbox = $('#select-current-units')
        unitbox.html('')
        for (const unit of this.data_manager.all_units) {
            const is_common = this.data_manager.common_units.includes(unit)
            const common_info = is_common ? '' : '(excl)' // Keep this short so the dropdown doesn't overflow the space it has
            unitbox.append($(`<option value=${unit}>${unit}${common_info}</option>`))
        }
            // Representation of the current selected "time"
        this.time = new TimeRangeRepr(this.data_manager.default_unit)
        this.select_units(this.data_manager.default_unit) // This will also update the time slider

        // Respond to changes
        unitbox.on('change', () => {
            this.select_units(unitbox[0].value)

        })

        // Associate any sources with widgets that have none.
        // Note that widgets should have a data-id when constructed or loaded from disk.
        for (let pwidget of this.pwidgets) {
            if (!pwidget.data_id) {
                this._assign_existing_data_id(pwidget) // TODO: This association needs to be improved
            }

            if (pwidget.data_id != null && !pwidget.has_processor()) {
                this.data_manager.load_processor(pwidget)
            }
        }

        // Wait to get connected to read data
        //this.data_provider.await_connection(() => { this.on_data_ready() })
        this.on_data_ready()
    }

    this._on_partial_data_loaded = function() {
        const self = this

        // Set up the list of data-sources
        const bin = $('#data-source-bin')
        // TODO: Use a forEach type iteration to keep this.datas private to data_manager
        // TODO: Some of this is branch predictor specific
        for (const data_id in this.data_manager.datas) {
            const src = this.data_manager.datas[data_id]

            let units_rows = ''
            for (const unit of src.all_units()) {
                const range = src.time_range_for(unit)
                units_rows += `<span style="padding-left:22px;"><span class="smalltext">${range.units}: ${range.first} - ${range.last}</span></span><br/>`
            }
            const data_source_type_class = data_source_class(src.type_id) // class specific to the data type for precise drag/drop control
            const short_type_id = get_data_type_shorthand(src.type_id)
            const item = $(`<div class="data-source-bin-item ${data_source_type_class}" title="${src.name}\n${src.directory}\n(${src.data_id})">
                              <div class="circle-icon" style="background-color:${src.ui_color}; margin:1px; z-index:10"> <!-- z-index to show above beginning of name string -->
                                <div class="icon-label">${src.ui_letter}</div>
                              </div>
                              <b id="datasource-name" style="position:absolute; top:1px; white-space:nowrap; right:2px; display:inline-block;"></b><br/>
                              ${units_rows}
                              <div id="stat-list-toggle" style="background-color:#c0c0c0; cursor:hand">
                                <span class="ui-icon ui-icon-triangle-1-s"></span> Stats (${src.all_stats().length})
                                <i style="float: right; font-size:12px">${short_type_id}&nbsp;</i>
                              </div>
                            </div>`)
            bin.append(item)

            // Remove chars from the datasource name until it fits in the box, prefixing with a '...' char if it does not immediately fit
            const ds_name = item.find('#datasource-name')
            ds_name.html(src.name.slice(Math.max(0, src.name.length - 32))) // cut name down to get close but greater than the right size and then shrink further until it fits based on html width
            const ellipses = String.fromCharCode(0x2026)
            let i = 0
            while (ds_name.width() > 150 && i < 1000) {
                const s = ds_name.html()
                n = s.search(new RegExp(ellipses, 'g'))
                if (n < 0)
                    ds_name.html(ellipses + s.slice(1)) // no ellipses yet
                else
                    ds_name.html(ellipses + s.slice(n+ellipses.length+1))
                i++
            }

            // List all stats
            const stat_list = $(`<div id="stat-list" class="data-source-bin-stat-list" style="display:none;">
                                 <span class="smalltext" style="margin-bottom:2px;">Drag a stat to <u>a widget</u> to view</span>
                                 </div>`)
            bin.append(stat_list)
            for (const stat of src.all_stats()) {
                let stat_name_raw
                if (typeof stat == 'string') {
                    stat_name_raw = stat
                } else {
                    stat_name_raw = stat.name
                }

                const is_compound = stat.compound != undefined
                let stat_desc = is_compound ? 'This stat is a combination of other stats\n' : ''
                stat_desc += (stat.explanation || '')

                // Examine variables and draw them specially
                // TODO: generalize this to other variables
                const stat_name = stat_name_raw
                                    .replace('{table}', `<span style="color:orange; font-style:italic;">t</span>`)
                                    .replace(/\./g, '.<wbr/>') // Word-break opportunity (to avoid breaking mid-word)
                const data_class = stat_data_class(src.type_id)
                const stat_item = $(`<div class="stat-bin-item ${data_class}" title="${stat_name_raw}\n${stat_desc}">${stat_name}</div>`)
                stat_item[0].data_id = data_id
                stat_item[0].stat_name = stat_name_raw

                // TODO: hovertext
                if (stat.type == StatTypes.STAT_TYPE_ATTRIBUTE) {
                    //stat_item.append($(`<span class="ui-icon ui-icon-flag" style="float: right;" title="Attribute value. This value can have any value for any event and does not necessarily increase or decrease. Summing this value over a period of time is expected to have no real meaning"></span>`))
                    //stat_item.append($(`<span class="ui-icon ui-icon-image" style="float: right;" title="Attribute value. This value can have any value for any event and does not necessarily increase or decrease. Summing this value over a period of time is expected to have no real meaning"></span>`))
                } else if (stat.type == StatTypes.STAT_TYPE_SUMMABLE) {
                    stat_item.append($(`<span style="float: right; padding-right:5px;" title="Summable statistic. This stat contains a flag or a 'delta' value. It is summable when viewed in a heatmap to view aggregate behavior over a period of time.">&Sigma;</span>`))
                } else if (stat.type == StatTypes.STAT_TYPE_COUNTER) {
                    stat_item.append($(`<span class="ui-icon ui-icon-arrowthick-1-ne" style="float: right;" title="Counter statistic. This value tends to increase over time and holds the 'latest' value for the statistic"></span>`))
                } else {
                    // Unknown type
                }

                // place an icon in the corner indicating special types of stats
                // TODO: Clicking on this should show the formula for the stat
                if (is_compound) {
                    stat_item.append($(`<span class="ui-icon ui-icon-script" style="float: right;" title="This is scripted statistic combining 1 or more other statistics and some modifiers"></span>`))
                }

                stat_list.append(stat_item)
            }
            stat_list.append($('<div style="height:30px;"></div>')) // Tail so that container doesn't clip the final item in the stat list

            item.find('#stat-list-toggle').on('click', () => {
                const icon = item.find('#stat-list-toggle').find('.ui-icon')
                if (stat_list.css('display') == 'block') {
                    stat_list.css('display', 'none')
                    icon.addClass('ui-icon-triangle-1-s').removeClass('ui-icon-triangle-1-n')
                } else {
                    stat_list.css('display', 'block')
                    icon.removeClass('ui-icon-triangle-1-s').addClass('ui-icon-triangle-1-n')
                }
            } )

            item[0].data_id = data_id
        }

        // Configure drag and drop for the data-sources and stats
        $('.stat-bin-item').draggable({
            appendTo: 'body',
            scroll: false,
            //stack: 'div', // move to top when dragging
            //revert: true, // always go back when dragged
            helper: 'clone', // leave copy behind
            //containment: "#main",
            //containment: '#data-panel',
            })
        $('.data-source-bin-item').draggable({
            appendTo: 'body',
            scroll: false,
            //stack: 'div', // move to top when dragging
            //revert: true, // always go back when dragged
            helper: 'clone', // leave copy behind
            //containment: "#main",
            //containment: '#data-panel',
            })
    }

    // Called when data processors are all ready
    this.on_data_ready = function() {

        // Set initial time & trigger rendering
        this.on_time_change(this.time_slider.first(), this.time_slider.last())
    }

    // Called when the time slider is changed or when initial data is available to read
    this.on_time_change = function(first, last) {
        const self = this

        // Update the cached time range
        this.time.first = first
        this.time.last = last

        // After updating the cached time, refresh the widgets
        this._refresh_all_widgets()
    }

    // Attempt to refresh all widgets asynchronously.
    // This will send some refresh requests.
    this._refresh_all_widgets = function({bpfilter_changed=false}={}) {
        const refresh_version = this._get_refresh_version()
        // Make requests for new data for all widgets
        for (const pwidget of this.pwidgets) {
            this._refresh_widget(pwidget, {refresh_version:refresh_version, bpfilter_changed: bpfilter_changed})
        }
    }


    // SETUP

    // Perform an update of the resizable items in the widget area to enable resizing and apply new-size constraints.
    // Also notify all widgets that they are resized for convenience because it is usually after a resize or
    // new-widget-creation that this function is called.
    this.enable_widget_resize = function() {
        let pw = $('.plato-widget')
        const widgetArea = $('#widget-area')
        const maxWidth = widgetArea.width() - 2 // Shrink for border
        pw.resizable({
            minWidth: maxWidth,
            maxWidth: maxWidth,
            minHeight: this.min_widget_height,
            start: function (evt, ui) {
                const pwidget = $(this)[0].pwidget
                if (typeof pwidget != 'undefined') {
                    pwidget.on_resize_started()
                }
            },
            stop: function (evt, ui) {
                const pwidget = $(this)[0].pwidget
                if (typeof pwidget != 'undefined') {
                    pwidget.on_resize_complete()
                }
            },
        })

        for (let pwidget of this.pwidgets) {
            $(pwidget.element).width(maxWidth)
            pwidget.on_resize_complete()
        }
    }



    // Controls

    this.zoom_out_all = function() {
        this.time_slider.zoom_out_all()
    }

    this.logout = function() {
        location.href = '/plato/logout'
    }

    this.download_layout = function() {
        const json = this._get_json_layout()
        download_to_client(json, `plato-layout-${Math.floor(performance.now())}.txt`, 'text/plain')
    }

    this.save_as_to_server = function() {
        this.save_layout_dialog.dialog('open')
    }

    this._save_layout_dialog_submit = async function() {
        const file_name = $('#server-save-filename')[0].value
        const overwrite = $('#server-save-overwrite').is(':checked')
        const json = this._get_json_layout()
        try {
            await this.layout_io.save(file_name, json, overwrite)
            this._refresh_layouts() // Get an updated list of the saved layouts
            this.save_layout_dialog.dialog("close");
        } catch(error) {
            this.show_error_popup('Error saving layout: ', error)
        }
    }

    // Select a new units. Update the dropdown box
    this.select_units = function(units) {
        // Validate units
        if (units == null || !this.data_manager.all_units.includes(units)) {
            this.show_error_popup('Invalid Units: ' + units,
                                  'A saved layout or other issue may have specified units that are not supported by any data-sources. Defaulting to another units')
            units = this.data_manager.default_unit
        }

        this.time.units = units
        $('#select-current-units')[0].value = this.time.units

        const range = this.data_manager.get_units_time_range(this.time.units)
        this.time_slider.reset_extents(range.first, range.last)

        // TODO: Convert from old units to new units. This will invoke on_time_change which will refresh all widgets
        // In the mean time, just refresh all widgets directly
        this._refresh_all_widgets()
    }


    this.autosave = function() {
        const json = this._get_json_layout()
        this.layout_io.autosave(json)
    }

    this.load_layout = function() {
        this.load_layout_dialog.dialog('open')
    }

    this.clear_layout = function() {
        // TODO: Use a dialog box to prompt
        this._delete_widgets(true) // also moves to trash
    }

    this._load_layout_dialog_submit = function() {
        const file_selecter = $('#load-layout-file')[0]

        const files = file_selecter.files; // FileList object
        if (files.length == 0) {
            // TODO: Show error
            return // Not a valid file
        }

        const f = files[0]
        this._load_new_layout_from_file(f)
        this.load_layout_dialog.dialog("close");

        return true; // Always accept
    }

    this._load_new_layout_from_file = function (file) {
        // TODO: Make page uninteractive until done
        const reader = new FileReader();

        reader.onerror = function(e) {
            time_error(e)
        }
        reader.onload = (e) => {
            const data = reader.result
            // Load the new layout
            const json = JSON.parse(data)
            this._load_json_layout(json)

        }
        reader.readAsText(file)
    }

    this._load_new_layout_from_name = async function(layout_name, append=false) {
        // TODO: Make page uninteractive until done
        try {
            const layout = await this.layout_io.load(layout_name)
            this._load_json_layout(layout, append)
        } catch(error) {
            this.show_error_popup('Error loading layout:', error.message, error.stack)
        }
    }

    this._delete_layout_from_name = async function(layout_name) {
        // TODO: Use a styled confirm modal dialog
        if (confirm(`Permanently delete this layout "${layout_name}" ?`)) {
            try {
                await this.layout_io.delete(layout_name)
                time_log(`Layout ${layout_name} deleted`)
                this._refresh_layouts()
            }catch(error) {
                this.show_error_popup('Error deleting layout:', error.message, error.stack)
            }
        }
    }

    // Get user's layouts from the server and then display them as a list, replacing the content of current layouts list
    this._refresh_layouts = function() {
        this.layout_io.enumerate().then((layouts) => {
                // TODO: List the layouts in the bin
                const bin = $('#layout-bin')
                bin.html('')
                for (const item of layouts) {
                    if (item.name == LayoutIO.AUTOSAVE_NAME)
                        continue

                    const name = squish_string(item.name, 30, 'tail')

                    const date = ('updateTime' in item) ? item.updateTime : '(unknown date)'
                    const el = $(`<div class="layout-bin-item">
                        <span class="layout-bin-heading" title="${item.name}">${name}</span><br/>
                        ${date}<br/>
                        <span class="num-widgets">?</span> widgets<br/>
                        <div style="box-sizing:border-box; padding:0px; margin:3px; border-top:1px solid #d0d0d0; display:grid; width:calc(100%-6px); grid-template-columns: auto auto auto auto; grid-template-areas: 'a b c d';">
                            <div style="grid-area:a;">
                                <a class="load-layout" href="javascript:void(0);" title="Replace current layout with this layout">load</a>
                            </div>
                            <div style="grid-area:b; text-align:right; padding-right:4px;">
                                <a class="append-layout" href="javascript:void(0);" title="Append to current layout">append</a>
                            </div>
                            <div style="grid-area:c; text-align:right; padding-right:4px;">
                                <a class="share-layout" href="javascript:void(0);" title="Create a link so someone else can view this layout. They cannot modify it.">share</a>
                            </div>
                            <div style="grid-area:d; text-align:right; padding-right:4px;">
                                <a class="delete-layout" href="javascript:void(0);" style="color:red;" title="Delete this layout">delete</a>
                            </div>
                        </div>
                    </div>`)

                    // Append and wire up events after full construction to be sure everything is available
                    bin.append(el)
                    el.find('.load-layout').on('click',() => {
                        this._load_new_layout_from_name(item.name)
                    })
                    el.find('.append-layout').on('click', () => {
                        this._load_new_layout_from_name(item.name, true)
                    })
                    el.find('.delete-layout').on('click', () => {
                        this._delete_layout_from_name(item.name)
                    })
                    el.find('.share-layout').on('click', () => {
                        const href = window.location.href
                        const qmidx = href.indexOf('?')
                        const base = qmidx > 0 ? href.slice(0, qmidx) : href
                        const url = `${base}?loadLayoutUser=${encodeURIComponent(this.layout_io.username)}&loadLayoutName=${encodeURIComponent(item.name)}`
                        $('#share-layout-url').html(url)
                        this.share_layout_dialog.dialog('open')
                    })

                    try {
                        // Finally parse the layout and populate content so that if this fails the item is still there and can be deleted
                        const layout = JSON.parse(item.layout)
                        const num_widgets = (typeof layout.widgets != 'undefined') ? layout.widgets.length : 'n/a' // For displaying test data
                        el.find('.num-widgets').html(num_widgets)
                    } catch(error) {
                        time_error(`Error examining layout "${item.name}"\n${item.layout}\n ${error.message}`, error.stack)
                        el.find('.num-widgets').html('<span class="smalltext errortext">Error: Layout possibly corrupt</span>')
                    }
                }
            }).catch((error) => {
                this.show_error_popup('Error querying layouts:', error.message, error.stack)
            })
    }

    // (Save)
    // Get json representing the widget layout and general configuration of the workspace
    this._get_json_layout = function() {
        layout = { time_range : [ this.time.first, this.time.last ],
                   widgets: [], // ordered
                   panels: {
                       data_source: [
                           $("#data-source-panel-container").height(),
                           $("#data-source-panel-container").is(":visible")],
                       widget_panel: [
                           $("#widget-panel-container").height(),
                           $("#widget-panel-container").is(":visible")],
                       branch_filtering: [
                           $("#branch-filtering-panel-container").height(),
                           $("#branch-filtering-panel-container").is(":visible")],
                       trashed_widgets: [
                           $("#trashed-widgets-panel-container").height(),
                           $("#trashed-widgets-panel-container").is(":visible")],
                       my_layout: [
                           $("#my-layouts-panel-container").height(),
                           $("#my-layouts-panel-container").is(":visible")],
                   },
                   selected_units: this.time.units,
                   branch_predictor_filtering: this.branch_filt_ctrl.generate_filter_object(),
                   animation: {
                        steps_per_second: $('#animation-steps-per-second')[0].value,
                        step_size: $('#animation-step-size')[0].value,
                   }
                 }

        const widgets = $('#widget-area').find('.plato-widget')
        for (const w of widgets) {
            // These are ordered top to bottom. Save them.

            // Skip anything without a widget (may be in the process of being delete)
            if (typeof w.pwidget != 'undefined') {
                layout['widgets'].push(this._get_widget_config_dict(w.pwidget))
            }
        }

        const json = JSON.stringify(layout, null, 4) // Nice readable formatting
        return json
    }

    // Produces a configuration dict for a specific widget
    this._get_widget_config_dict = function(pwidget) {
        const name = pwidget.element.getAttribute('plato-widget-name')
        const typename = pwidget.element.getAttribute('plato-widget-type')

        const wdata = { 'name': name,
                        'typename': typename,
                        'config': pwidget.get_internal_config_data(), // viewer-level config
                        'widget_config': pwidget.get_config_data(), // widget's custom config
                      }

        return wdata
    }

    this._load_json_layout = function(json, append=false) {
        if (!append) {
            // Clear all existing widgets in order to replace them
            this._delete_widgets()
        }

        // Load new widgets
        for (let wd of json.widgets) {
            this._create_widget_from_config_dict(wd)
        }

        // Restore units
        if (typeof json.selected_units == 'undefined') {
            if (this.time.units == null) {
                time_error('No time units in layout and none selected automatically from list. How?')
            }
        } else {
            this.select_units(json.selected_units)
        }

        $("#data-source-panel-container").height(json.panels.data_source[0])
        if (json.panels.data_source[1]) {
            $("#data-source-panel-container").show()
        } else {
            $("#data-source-panel-container").hide()
        }
        $("#widget-panel-container").height(json.panels.widget_panel[0])
        if (json.panels.widget_panel[1]) {
            $("#widget-panel-container").show()
        } else {
            $("#widget-panel-container").hide()
        }
        $("#branch-filtering-panel-container").height(json.panels.branch_filtering[0])
        if (json.panels.branch_filtering[1]) {
            $("#branch-filtering-panel-container").show()
        } else {
            $("#branch-filtering-panel-container").hide()
        }
        $("#trashed-widgets-panel-container").height(json.panels.trashed_widgets[0])
        if (json.panels.trashed_widgets[1]) {
            $("#trashed-widgets-panel-container").show()
        } else {
            $("#trashed-widgets-panel-container").hide()
        }
        $("#my-layouts-panel-container").height(json.panels.my_layout[0])
        if (json.panels.my_layout[1]) {
            $("#my-layouts-panel-container").show()
        } else {
            $("#my-layouts-panel-container").hide()
        }

        // Restore animation settings
        $('#animation-steps-per-second')[0].value = json.animation.steps_per_second || '5'
        $('#animation-step-size')[0].value = json.animation.step_size || '1000'

        // Restore bp filter settings
        if (typeof json.branch_predictor_filtering != 'undefined') {
            const filt = json.branch_predictor_filtering
            this.branch_filt_ctrl.restore_ui_from_filter_object(filt)
        }

        // Restore time range
        const time_range = json.time_range
        this.time_slider.set_selection(time_range[0], time_range[1])

        this._post_add_widgets()
    }

    // Call this whenever widgets are added
    this._post_add_widgets = function () {
        // Enable resizing on all the new widgets & call initial resize events
        this.enable_widget_resize()

        // Render everything when data is ready
        //this.data_provider.await_connection(() => { this.on_data_ready() })
        this.on_data_ready()
    }

    // Creates a new widget from its dictionary (usually from parsed json).
    // Takes output of _get_widget_config_dict
    // A batch of these calls must be followed by a call to this._post_add_widgets() to deal with re-styling any new widgets added.
    this._create_widget_from_config_dict = function (wd) {
        const widget_name = wd.name
        const widget_type = wd.typename
        const cfg = wd.config
        const widget_cfg = wd.widget_config

        // Find factory by type
        const factory = this.widget_factory[widget_type]
        if (typeof factory == 'undefined') {
            // TODO: Show error dialog
            time_error(`Failed to load widget ${widget_name} type ${widget_type} because this type does not have a factory`)
            return // Skip this - don't know how to construct
        }

        // Try and resolve the widget's data-source to something we have open if possible
        // TODO: This config info is written inside the BaseWidget. It should be written by the viewer since that is who reads it
        const construct_options = {widget_name: widget_name,
                                   request_processor: false, // we'll do this ourselves after loading saved config
                                   }
        const guess_data_id = this.data_manager.get_data_source_id_from_name_hint(cfg.data.data_source_name_hint)
        if (this.data_manager.has_data(cfg.data.data_id)) {
            construct_options.data_id = cfg.data.data_id
        } else if (guess_data_id != null) {
            construct_options.data_id = guess_data_id
        }

        const pwidget = this.add_new_widget_from_factory(factory, construct_options)

        // Apply viewer-level config
        pwidget.apply_internal_config_data(cfg)

        // Apply widget-config. Use a cloned value just in case widgets do not make their own copy and modify shared dicts
        widget_cfg_clone = JSON.parse(JSON.stringify(widget_cfg))
        pwidget.apply_config_data(widget_cfg_clone)

        // Load processor after applying configuration since processor choice could depend on configuration
        // Put in a request for the data-processor backing the widget.
        // This will assign the processor to the widget once the information arrives
        if (pwidget.data_id != null) {
            this.data_manager.load_processor(pwidget)
        }

        return pwidget
    }

    // Create a plato widget from a factory and optional name
    this.add_new_widget_from_factory = function (factory, { widget_name = null, data_id = null, request_processor = true } = {}) {
        const widget_area = $('#widget-area')
        const widget_area_trailer = widget_area.find('#widget-area-trailer')

        if (widget_name == null)
            widget_name = 'Widget' + this.next_widget_id

        const widget_type = factory.typename

        // Add new element to DOM
        const jw = $(`<div class="plato-widget" plato-widget-name="${widget_name}" plato-widget-type="${widget_type}"></div>`)
        jw.insertBefore(widget_area_trailer)

        // Create plato widget object and attach to element
        // This should be the only place where widgets are constructed since the following steps in this function are
        // required to properly initialize one.
        const widget_id = this.next_widget_id++
        const el = jw[0]
        let pwidget = new factory(el, widget_id)

        // Attach associations to the widget
        pwidget.viewer = this

        // Add the factory to the widget for reference
        pwidget.factory = factory

        // Enable dropping based on the widget factories data_type
        pwidget.make_droppable()

        // Associate the element with the widget so we can go DOM-element -> widget
        el.pwidget = pwidget

        // Track the widget
        this.pwidgets.push(pwidget)

        // TODO: Use a method to perform this association
        if (data_id != null) {
            pwidget.assign_data(data_id, this.data_manager.get_data(data_id))
        } else {
            // We're opting not to assign first data id to this widget since it is less confusing to require users to
            // explicitly set the data-source.
            //this._assign_existing_data_id(pwidget)
        }

        if (request_processor) {
            // Put in a request for the data-processor backing the widget.
            // This will assign the processor to the widget once the information arrives
            if (pwidget.data_id != null) {
                this.data_manager.load_processor(pwidget)
            }
        }

        return pwidget
    }

    // Delete all widgets
    this._delete_widgets = function (trash=false) {
        while(this.pwidgets.length > 0) {
            this.delete_widget(this.pwidgets[0], trash)
        }

        // NOTE: do not ever reset widget id. This helps guarantee that async callbacks will end up at the right widget if
        //       one is deleted and replaced later.
        //this.next_widget_id = 0
    }

    this.trash_widget = function (pwidget) {
        this.delete_widget(pwidget, true)
    }

    // Make a new widget with the same config right below this one
    this.clone_widget = function (pwidget) {
        const widget_config = this._get_widget_config_dict(pwidget)
        const new_pwidget = this._create_widget_from_config_dict(widget_config)
        $(new_pwidget.element).detach().insertAfter($(pwidget.element)) // Insert after element from which it was cloned
        this._post_add_widgets()
    }

    // Delete a single specific widget
    this.delete_widget = function (pwidget, trash=false) {
        const self = this

        // Find widget
        for (let i = 0; i < this.pwidgets.length; i++) {
            if (this.pwidgets[i] == pwidget) {
                this.pwidgets.splice(i,1)

                pwidget.expire()
                pwidget.element.pwidget = null // Remove reference from DOM element

                if (!trash) {
                    $(pwidget.element).remove() // Remove widget from DOM and destroy
                } else {
                    this.deactivate_widgets('Trashing widget', 'Please wait')

                    // NOTE: Cannot actually delete element until image is captured. Detaching does not work either

                    // Move to widget trash bin
                    const trash_item = $(`<div class="widget-trash-item">${pwidget.name} (${pwidget.typename})</div>`)
                    trash_item.click(function(event) { self._restore_trashed_widget(trash_item[0]) })
                    trash_item[0].widget_config = this._get_widget_config_dict(pwidget)
                    $('#widget-trash-bin').prepend(trash_item)

                    function do_finally() {
                        $(pwidget.element).remove() // Remove widget from DOM and destroy
                        self.reactivate_widgets()
                    }

                    if (pwidget.is_thumbnail_safe()) {
                        // Make a thumbnail of the widget so it can be seen in the trash
                        // TODO: Need to find a way to downsample since this is a lot of data. Possibly by an intermediate canvas.
                        domtoimage.toJpeg(pwidget.element, { quality: 0.6, bgcolor: '#ffffff' })
                            .then(function (data_url) {
                                trash_item.tooltip({
                                    items: '.widget-trash-item',
                                    position: { my: 'left', at: 'right middle' },
                                    content: function() {
                                        return $(`<span class="smalltext">preview</span><br/><img src="${data_url}" style="width:200;"/>`)
                                    }
                                })
                                // TODO: This is a bug. WHen clearing the trashed state, this may remove the
                                //       "Trashing Widget/Please Wait" box from the screen while other widgets are being
                                //       deleted. More stat is needed.
                                do_finally()
                            }).catch(function (error) {
                                time_error('Error making thumbnail image of widget: ', error)
                                do_finally()
                            });
                    } else {
                        do_finally()
                    }
                }

                return
            }
        }

        time_error(`Attempted to delete widget ${pwidget.name} but could not find it in pwidgets list`)
    }

    this.reactivate_widgets = function () {
        $('#widgets-busy-div').css('display', 'none')
    }

    this.deactivate_widgets = function (msg, submsg='') {
        $('#widgets-busy-div').css('display', 'block')
        $('#widgets-busy-div').find('h1').html(msg)
        $('#widgets-busy-div').find('p').html(submsg)
    }

    // Calculate a unique value representing the 'version' of the current gui state (e.g. units, time, filters, etc.)
    // This could be a counter or a hash as long as we can detect when state has changes. Hash is preferred since it
    // will have fewer spurious changes.
    this._get_refresh_version = function() {
        // Note that a hash-code is safe here because this is used within this session on a single machine only, so
        // random but consistent ordering of keys in strings are ok as long as the same config produces the same hash
        // and any change results in a different hash.
        const version = [this.time.first, this.time.last, this.time.units,
                         this.branch_filt_ctrl.filter_object_version].toString().hashCode()

        return version
    }

    // Begin a refresh of this widget by sending a data request which will trigger a redraw when data comes back.
    //
    // pwidget          widget to refresh.
    // force_sync       sync with the global slider and refresh whether the widget is in manual or auto sync mode. Also
    //                  uses new configuration (i.e. filters)
    // force_no_sync    refresh (but do not sync with the global slider) whether the widget is in manual or auto sync
    //                  mode. Still uses new
    //                  configuration (i.e. filters)
    // force_sync_all   sync with the entire time range whether the widget is in manual or auto sync
    // refresh_version  specify a version or let it automatically update.
    // bpfilter_changed has the branch predictor filter changed since the last update? This should really be about any
    //                  filters or other global settings changing.
    // TODO: Refactor these parameters to be more intuitive. Should have "force", "sync_time" and "sync_other" all separate. Lots of call-sites to update though.
    this._refresh_widget = function (pwidget, {force_sync=false, force_no_sync=false, force_sync_all=false, refresh_version=null, bpfilter_changed=false} = {}) {
        // Skip widgets not ready for data
        if (!pwidget.can_request())
            return // Widget not ready or disabled

        const custom_update = pwidget.get_custom_update_range()

        // Build a version string if we don't have one. This includes filter and range information
        refresh_version = refresh_version == null ? this._get_refresh_version() : refresh_version

        if (bpfilter_changed)
            pwidget.global_filter_configuration_changed()


        // See if we have to sync
        const any_forced_sync = force_no_sync || force_sync || force_sync_all
        if (!any_forced_sync && !pwidget.should_follow_global() && custom_update == null) {
            const last_update = pwidget.get_last_update()
            if (pwidget.refresh_version != refresh_version) {
                pwidget.mark_stale()
            }
            return
        }

        // Use the custom update info if given
        let units
        let first
        let last
        if (!any_forced_sync && custom_update != null) {
            first = custom_update.first
            last = custom_update.last
            units = custom_update.units
        } else if (force_sync_all) {
            first = this.time_slider.min()
            last = this.time_slider.max()
            units = this.time.units
        } else if (force_no_sync) {
            // Update using the last settings
            const last_update = pwidget.get_last_update()
            first = last_update.first
            last = last_update.last
            units = last_update.units
        } else { // force_sync or normal widget-in-follow-mode sync
            first = this.time.first
            last = this.time.last
            units = this.time.units
        }

        const uid = this._request_uid++

        // Constrain the request per widget depending on its data-source
        const ok = this.data_provider.request(pwidget, first, last, units,
                                              this.branch_filt_ctrl.filter_object, uid, (json, actual_range, latency) => {
            if (!pwidget.is_expired()) {
                if (json == null) {
                    // TODO: If this comes back with an error, re-trigger loading of the data-sources and processors
                }

                const server_latency = json != null ? json.durationMs : null

                // Push data to buffer to be consumed in next draw
                // Note that this is the unconstrained data since widgets should draw based on that range. so that
                // datasources with less data than others are shown with the missing pieces when viewing stacked line
                // plots.
                pwidget.response_buffer.push({json:json, first: first, last: last, units: units, uid: uid,
                                              timings: { request_latency: latency, server_latency: server_latency,
                                                         buffer_time: performance.now() },
                                              actual_range: actual_range,
                                              })


                // Store new refresh version
                pwidget.refresh_version = refresh_version

                // Ensure a draw is scheduled that includes this widget
                this.draw_scheduler.schedule(pwidget)
            } else {
                time_log('Got a response for expired widget')
            }
        })
    }

    this._restore_trashed_widget = function (element) {
        const cfg = element.widget_config
        $(element).remove()
        this._create_widget_from_config_dict(cfg)
        this._post_add_widgets()
    }


    this.show_error_popup = function(msg, details='', stack='') {
        this.popup_div.css('display', 'block')
        this.popup_div.find('#message-heading').html(msg)
        this.popup_div.find('#message-details').html(details)
        this.popup_div.slideDown()

        // Disappear after a while
        // TODO: If another popup call replaces us, the div will disappear earlier than expected for the 2nd msg.
        //       This just needs some polish.
        setTimeout(()=>{
            this.popup_div.slideUp('fast')
        }, 10000)

        this.popup_div.find('#close-popup-button').off().on('click', () => {
            this.popup_div.slideUp('fast')
        })

        // Log the error in the console too
        time_error(msg + ': ' + details + ':' + stack)
    }

    // Widget -> Viewer Interface
    // ============================================================

    this.reassign_data = function(pwidget, data_id) {
        pwidget.assign_data(data_id, this.data_manager.get_data(data_id))
    }

    this.reload_widget_processor = function(pwidget) {
        this.data_manager.load_processor(pwidget)
    }

    // Widget requests an update out-of-band which will arrive through a separate interface receive_oob_data
    this.request_oob_data = function(pwidget, callback, {all_data=true, constrain=true, kwargs={}}={}) {
        var first, last, units
        if (all_data) {
            first = this.time_slider.min()
            last = this.time_slider.max()
            units = this.time.units
        } else {
            first = this.time.first
            last = this.time.last
            units = this.time.units
        }

        const [constrained_first, constrained_last] = constrain ? pwidget.data_source.constrain(units, first, last) : [first, last]

        const ok = this.data_provider.request_oob(pwidget, constrained_first, constrained_last, units,
                                                  kwargs,
                                                (json, latency) => {
                                                    pwidget.receive_oob_data(callback, json)
                                                })

        if (!ok) {
            time_error('Viewer needs to reconnect')

            // TEMP: restructure this logic?
            this.data_provider.await_connection(() => { this.on_data_ready() })
        }
        return ok
    }

    // Widget wants new data because it changed. Trigger a request/response/draw cycle
    this.refresh_widget = function (pwidget, {force_sync=false, force_no_sync=false, force_sync_all=false}={}) {
        this._refresh_widget(pwidget, {force_sync:force_sync, force_no_sync:force_no_sync, force_sync_all:force_sync_all})
    }

    // A widget is requesting we move the global time slider
    this.widget_chose_time = function(pwidget, first, last) {
        // Sanitize
        let int_first = parseInt(first)
        let int_last = parseInt(last)
        if (isNaN(int_first) || isNaN(int_last)) {
            time_warn(`Widget ${pwidget.name} tried to set time with invalid values: ${first} ${last}`)
        } else if (int_last < int_first) {
            time_warn(`Widget ${pwidget.name} tried to set time with reversed values: ${first} ${last}`)
        } else {
            this.time_slider.set_selection(int_first, int_last)
        }
    }

    // ============================================================
}

// DOM is ready to go. This is like body onload.
$(document).ready( function() {
    viewer.on_load()
})

// Window resized (does not happen at load-time)
$(window).resize( function() {
    viewer.on_resize()
})

//$(window).bind('beforeunload', function(){
//  return '' // Custom messages not supported here by chrome/firefox
//});
