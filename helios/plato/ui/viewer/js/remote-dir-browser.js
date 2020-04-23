
// Remote directory browser utility.
// Create the instance and invoke get(path) with the remote path you want to browse. This function returns a promise
// that is completed when the server results are returned (or is rejected with an exception on error).
// The server will return the subdirectories and data-sources found in that directory.
// This can be used directly by code or to back a ui-based remote file browser.
class RemoteDirBrowser {
    constructor(prefix, error_msg_func) {
        this._prefix = prefix
        this._error_msg_func = error_msg_func

        this._ws = new WebSocketWrapper((s) => { this._ondata(s) }, prefix, '/ws/sources')
        this._req_id = 0

        this.subdirectories = new Set()
        this.source_file_names = new Set()
    }

    // Get a directory listing for the chosen directory
    // Returns a promise an object containing the result keys:
    //   subdirectories
    //   source_file_names
    // Avoid sending another of these request until the previous promise is completed. It will break.
    get(dir) {
        this.datas = {} // Clear known data items
        this._status = 'none'
        const promise = new Promise((res,rej)=>{this._resolve = res; this._reject = rej;})

        this._ws.await_connection(() => {
            this._req_id++
            const json = JSON.stringify({
                "reqSeqNum": this._req_id,
                "command": "loadDirectory",
                "directory" : dir,
            });

            this._ws.send(json)
        })

        return promise
    }

    _ondata(msg) {
        const json = JSON.parse(msg)
        // Filter old messages
        if (json.reqSeqNum != this._req_id) {
            time_warn(`Rejecting old data-source message with id ${json.reqSeqNum} vs ${this._req_id}`)
            return
        }


        if ('subDirectories' in json) {
            for (const subdir of json.subDirectories) {
                time_log('subdirectory:', subdir)
                this.subdirectories.add(subdir)
            }
        }

        if ('sources' in json) {
            for (const src of json.sources) {
                if (src.dataId in this.datas == false) {
                    // Just track the existence of this these files
                    this.source_file_names.add(json.directory + '/' + src.name)
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
                this._status = 'complete'
                this._resolve({subdirectories: Array.from(this.subdirectories),
                               source_file_names: Array.from(this.source_file_names)})
            } else if (json.result == 'error') {
                this._status = 'error'
                this._reject({message:json.errorMessage, stack:''})
                this._reject = null
                this._resolve = null
            }
        }
    }
}