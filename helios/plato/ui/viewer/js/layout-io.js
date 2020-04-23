// Handles saving and loading of layouts on the server
class LayoutIO {
    constructor(prefix, username, error_msg_func) {
        this.prefix = prefix
        this.username = username
        this.error_msg_func = error_msg_func

        this.ws = new WebSocketWrapper((s) => { this._ondata(s) }, prefix, '/ws/utility')

        this.seqn = 0
        this.response_handlers = {}
    }

    _ondata(msg) {
        const obj = JSON.parse(msg)

        const handler = this.response_handlers[obj.reqSeqNum]
        delete this.response_handlers[obj.reqSeqNum]

        handler(obj)
    }

    await_connection(callback) {
        this.ws.await_connection(callback)
    }

    enumerate() {
        const cmd = {
            reqSeqNum: this.seqn,
            user: this.username,
            command: 'getAllLayouts',
            version: LayoutIO.VERSION,
        }

        let resolve
        let reject
        const promise = new Promise((res, rej) => { resolve = res; reject = rej; })
        this.response_handlers[this.seqn] = (json) => {
            if (json.result == 'success')
                resolve(json.layouts)
            else
                reject(new Error(`${json.error}`))
        }
        this.seqn++

        this.ws.await_connection(()=>{ this.ws.send(JSON.stringify(cmd)) })

        return promise

        //Receive:
        //{"reqSeqNum": 123, "command": "getAllLayouts", "result": "success", "error": "", "layouts": {"mylayout": "{'a': 1, 'b': 2, 'c': [1, 2, 3, 4, 5, 6, 7, 8]}", "mylayout2": "{'a': 1, 'b': 2, 'c': [1, 2, 3, 4, 5, 6, 7, 8]}", "mylayout3": "{'a': 1, 'b': 2, 'c': [1, 2, 3, 4, 5, 6, 7, 8]}"}}

    }

    // TODO: Move await_connection into all these requests (instead of caller having to do it)
    save(name, content_str, overwrite=false) {
        const cmd = {
            reqSeqNum: this.seqn,
            user: this.username,
            command: 'saveLayout',
            name: name,
            overwrite: overwrite,
            content: content_str,
            version: LayoutIO.VERSION,
        }

        let resolve
        let reject
        const promise = new Promise((res, rej) => { resolve = res; reject = rej; })
        this.response_handlers[this.seqn] = (json) => {
            if (json.result == 'success')
                resolve(true)
            else
                reject(new Error(`${json.error}`))
        }
        this.seqn++

        this.ws.await_connection(()=>{ this.ws.send(JSON.stringify(cmd)) })

        return promise
    }

    // Load a layout by name for this user. If username_override is set, load another users' layout (cannot save over
    // other users' layouts though).
    load(name, username_override=null) {
        const username = username_override == null ? this.username : username_override
        const cmd = {
            reqSeqNum: this.seqn,
            user: username,
            command: 'loadLayout',
            name: name,
            version: LayoutIO.VERSION,
        }

        let resolve
        let reject
        const promise = new Promise((res, rej) => { resolve = res; reject = rej; })
        this.response_handlers[this.seqn] = (json) => {
            if (json.result == 'success')
                resolve(JSON.parse(json.content))
            else
                reject(new Error(`${json.error}`))
        }
        this.seqn++

        this.ws.await_connection(()=>{ this.ws.send(JSON.stringify(cmd)) })

        return promise
    }

    delete(name) {
        const cmd = {
            reqSeqNum: this.seqn,
            user: this.username,
            command: 'deleteLayout',
            name: name,
        }

        let resolve
        let reject
        const promise = new Promise((res, rej) => { resolve = res; reject = rej; })
        this.response_handlers[this.seqn] = (json) => {
            if (json.result == 'success')
                resolve(true)
            else
                reject(new Error(`${json.error}`))
        }
        this.seqn++

        this.ws.await_connection(()=>{ this.ws.send(JSON.stringify(cmd)) })

        return promise
    }

    // Save the latest layout to the autosave
    autosave(content_str) {
        return this.save(LayoutIO.AUTOSAVE_NAME, content_str, true)
    }

    // Load the latest autosave
    load_autosave() {
        return this.load(LayoutIO.AUTOSAVE_NAME)
    }
}

LayoutIO.AUTOSAVE_NAME = '~autosave~'
LayoutIO.VERSION = 1