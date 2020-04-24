
// Queue for data responses from server that allow older data to be skipped if multiple items arrive in succession if
// permitted (this is incomplete). Performs a callback for buffered data when 'apply' is called.
class ResponseBuffer {
    constructor() {
        this.latest = null
    }

    has_data() {
        return this.latest != null
    }

    clear() {
        this.latest = null
    }

    // Push a new item to the buffer
    push(item) {
        //if (this.latest != null)
        //    time_log('Dropped an old response in response queue! ', this.latest)

        this.latest = item
    }
}
