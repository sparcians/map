
// Range of time including units
class TimeRange {
    constructor(json) {
        this.units = json.units
        this.first = json.first
        this.last = json.last
    }
}

// Represents the client's view of a single data set on the server (e.g. hdf5 file, simdb, etc.)
class DataSet {
    constructor(json, ui_color, ui_letter) {
        this.directory = json.directory
        this.data_id = json.dataId
        this.type_id = json.typeId
        this.name = json.name
        this.time_ranges = []
        this.stats = json.stats
        this.type_specific = json.typeSpecific
        this.time_range_by_units = {}
        this.ui_color = ui_color
        this.ui_letter = ui_letter

        for (const trng of json.timeRange) {
            const time_range = new TimeRange(trng)
            this.time_ranges.push(time_range)
            this.time_range_by_units[time_range.units] = time_range
        }

        this._stats_list = []
        this._stats_by_name = {}
        if (typeof this.stats != 'undefined' && this.stats) {
            for (const stat of this.stats) {
                this._stats_list.push(stat)
                this._stats_by_name[stat.name] = stat
            }
        }
    }

    // Get full information about a stat given its name
    get_stat_info(name) {
        return this._stats_by_name[name]
    }

    // Constrain time with units to available range
    constrain(units, first, last) {
        const trng = this.time_range_by_units[units]
        if (typeof trng == 'undefined') {
            time_error(`Datasource ${this.data_id} ${this.directory} ${this.name} has no support for units: ${units}`)
        }
        first = Math.max(first, trng.first)
        last = Math.min(last, trng.last)

        return [first, last]
    }

    // Get substitutions for a variable dependent on data type
    get_substitutions(variable) {
        if (this.type_id == DataTypes.SHP_TRAINING_TRACE) {
            if (variable == 'table') {
                const arr = []
                for (let i = this.type_specific.shape.table.min; i <= this.type_specific.shape.table.max; i++) {
                    arr.push(i)
                }
                return arr
            }
            return []
        } else {
            time_error('Data source does not know any substitutions: ', this.type_id)
            return []
        }
    }

    // Get the time range for specific units.
    // Returns null if units not known
    time_range_for(units) {
        for (const tr of this.time_ranges) {
            if (tr.units == units)
                return tr
        }

        return null
    }

    all_units() {
        const units = new Set()
        for (const trng of this.time_ranges) {
            units.add(trng.units)
        }
        return units
    }

    // List of all stat objects
    all_stats() {
        return this._stats_list
    }
}

// Tracks all data sources known to us. Loads data-sources by path and loads processors by data-source/type in order to
// associate them with widgets.
class DataManager {

    constructor(prefix, primary_data_dir, error_msg_func, status_msg_func) {
        this.prefix = prefix
        this.data_dir = primary_data_dir // Primary
        this.error_msg_func = error_msg_func
        this.status_msg_func = status_msg_func

        this.ws = new WebSocketWrapper((s) => { this._ondata(s) }, prefix, '/ws/sources')

        // Data known to be available on the server
        this.datas = {} // data_id-> DataSet
        this._known_sources = new Set()
        this._promise = null

        this.all_units = null
        this.common_units = null
        this.num_sources = 0
        this.req_id = 0

        this._status = 'none'

        this._processor_reqs = {} // outstanding callbacks for processor IDs

        this._num_directories = 0
        this._directory_info = {}

        // File loading
        this._file_queue = []
    }

    status() {
        return this.ws.status()
    }

    get_num_sources() {
        return this.num_sources
    }
    
    has_data(data_id) {
        return data_id in this.datas
    }

    // Helper to check if there is a datasource with a given name.
    // This is used when the specific data-id the viewer is looking for is missing, so it is trying to find a match by
    // name (which is usually the leaf filename inside a data directory)
    get_data_source_id_from_name_hint(name) {
        for (const data_id in this.datas) {
            if (this.datas[data_id].name == name)
                return data_id
        }

        return null
    }

    // Find the time range of all data-sources for a given `units`
    get_units_time_range(units) {
        const ret = {first: null, last: null}

        for (const data in this.datas) {
            const range = this.datas[data].time_range_for(units)
            if (range != null) {
                if (ret.first == null || ret.first > range.first)
                    ret.first = range.first
                if (ret.last == null || ret.last < range.last)
                    ret.last = range.last
            }
        }

        return ret
    }

    get_data(data_id) {
        if (data_id in this.datas) {
            return this.datas[data_id]
        }
        return null
    }

    // Add a list of files. Note that this cannot be called again until the promise is completed.
    append_sources(files=[]) {
        // Do not clear data items
        if (files.length == 0)
            return false

        if (typeof files == 'string') {
            files = [files]
        }

        time_log(`Loading new sources: ${files}`)

        this._file_queue.splice(this._file_queue.length, 0, ...files)

        // Kick off a new dequeue event if there was not one running
        if (this._promise == null) {
            this._promise = new Promise((res,rej)=>{this._resolve = res; this._reject = rej;})
            this.ws.await_connection(() => {
                this.req_id++
                this._dequeue_and_send_file_request()
            })
        }

        return this._promise
    }

    // Finds first data (random order) with a given data-type id. Returns data_id if found and null if none found
    find_data_of_type(type) {
        for(const data_id in this.datas) {
            const data = this.datas[data_id]
            if (data.type_id == type)
                return data_id
        }
        return null
    }

    // Load a processor for a specific widget and attach it's information when ready. This has no response and takes
    // place on the server-side. When complete, the new processor is assigned to the widget and it is notified that it
    // can begin requesting data.
    load_processor(pwidget) {
        if (!this.ws.check_open()) {
            return false
        }

        this.req_id++

        console.assert(typeof pwidget.factory.processor_type != 'undefined')
        console.assert(pwidget.data_id != null)

        const proc_kwargs = pwidget.get_processor_kwargs()

        const json = JSON.stringify({
            reqSeqNum: this.req_id,
            command: "getProcessor",
            processor: pwidget.factory.processor_type,
            dataId: pwidget.data_id,
            kwargs: proc_kwargs,
        })

        this._processor_reqs[this.req_id] = pwidget
        pwidget.notify_loading_processor()

        this.ws.send(json)

        time_log(`Requesting processor for widget id ${pwidget.widget_id} type ${pwidget.factory.processor_type} data ${pwidget.data_id} ${proc_kwargs}`)

        return true
    }

    // Send the next internally queued file request
    _dequeue_and_send_file_request() {
        const file = this._file_queue[0]
        this._file_queue.splice(0,1)

        const json = JSON.stringify({
            "reqSeqNum": this.req_id,
            "command": "loadFile",
            "file" : file,
        });

        this.ws.send(json)

        this.status_msg_func(`Retrieving ${file}`)
        time_log(`Requested source: ${file}`)
    }

    // Handle server response - already loaded into buffer `msg`
    _ondata(msg) {
        //time_log('DataManager _ondata: ', msg)
        const json = JSON.parse(msg)

        if (json.command == 'loadFile') {
            // Update wait time
            if ('waitEstimate' in json) {
                // TODO: reflect the wait estimate somewhere
            }

            if ('sources' in json) {
                for (const src of json.sources) {
                    if (this.datas[src.dataId] === undefined) {
                        this._add_source(src)
                    }
                }
            }

            if (json.result == 'partial') {
                this._status = 'partial'
            } else if (json.result == 'in-progress') {
                this._status = 'in-progress'
            } else {
                if (json.result == 'complete') {
                    // Done with this result
                    if (this._file_queue.length > 0)
                        this._dequeue_and_send_file_request() // Continue with next request
                    else
                        this._handle_file_append_complete()

                } else if (json.result == 'error') {
                    this._status = 'error'
                    this._reject(json.errorMessage)
                    this._reject = null
                    this._resolve = null
                }
            }
        } else if (json.command == 'getProcessor') {
            const pwidget = this._processor_reqs[json.reqSeqNum]
            if (typeof pwidget == 'undefined') {
                time_error('Got getProcessor response for undefined reqSeqNum: ', json.reqSeqNum)
                return
            }

            // TODO: handle in-progress messages and update waiting state in client

            if (json.result == 'complete') {
                if (!pwidget.is_expired()) {
                    pwidget.notify_loading_processor_done()
                    pwidget.assign_processor(json.processorId)
                }
                delete this._processor_reqs[json.reqSeqNum]
            } else if (json.result == 'error') {
                this.error_msg_func('Error getting processor', pwidget.name + ': ' + json.errorMessage)
            } else if (json.result.indexOf('error') >= 0) {
                this.error_msg_func('Error getting processor', pwidget.name + ': ' + json.result)
            }

        }
    }

    // A file-load request has completed
    _handle_file_append_complete() {
        this.all_units = new Set()
        this.common_units = null
        for (const id in this.datas) {
            const src = this.datas[id]
            const src_units = src.all_units()

            for (const unit of src_units) {
                this.all_units.add(unit)
            }

            if (this.common_units == null) {
               this.common_units = new Set(this.all_units) // copy
            } else {
                // intersect
                this.common_units = new Set([...this.common_units].filter(x => src_units.has(x)))
            }
        }

        this.common_units = Array.from(this.common_units)
        this.all_units = Array.from(this.all_units)

        if (this.common_units.length > 0)
            this.default_unit = this.common_units[0]
        else if (this.all_units.length > 0)
            this.default_unit = this.all_units[0]
        else
            this.default_unit = null

        this._status = 'complete'
        this._resolve(true)
        this._reject = null
        this._resolve = null
        this._promise = null
    }

    // Add a new source to the data-set
    _add_source(src) {
        this.num_sources++
        const [ui_color, ui_letter] = this._get_color_and_letter(src.directory)
        const ds = new DataSet(src, ui_color, ui_letter)
        this.datas[src.dataId] = ds
    }

    // Create an appropriate color and letter combination for a NEW data source in a particular directory to help
    // identify the dataset in the ui.
    _get_color_and_letter(directory) {
        let dir_info
        if (directory in this._directory_info) {
            dir_info = this._directory_info[directory]
        } else {
            let color
            if (this._num_directories < DataManager.color_list.length)
                color = DataManager.color_list[this._num_directories]
            else
                color = '#ffffff' // fallback color

            this._num_directories++
            dir_info = {color: color, sources: 0}
            this._directory_info[directory] = dir_info
        }

        // Next letter for this directory
        const letter = String.fromCharCode('A'.charCodeAt(0) + dir_info.sources++)

        return [dir_info.color, letter]
    }
}

// Colors assigned to each data-source
// Red and blue are color-blind friendly, but colors beyond that might require tweaking shades and using different icon
// shapes (icons.css).
DataManager.color_list = [
    '#f66', // red
    '#2af', // blue
    '#2d2', // green
    '#f0f', // purple
    '#f90', // orange
]
