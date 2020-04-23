
/*
 * Parses numbers sequentially from a binary buffer.
 *
 * This is part of a prototype and support is EXTREMELY limited for 64-bit integers since javascript does not support them.
 */
function BinaryBuffer(acr) {
    this.acr = acr;
    this.index = 0

    // Async
    this.makeReady = function(size, onReady) {
        const self = this
        this.acr.loadMoreBuffer(size, function() {
            self.acr.drop(self.index)
            self.index = 0
            onReady()
        })
    }

    // Amount currently buffered and remaining.
    this.remaining = function() { 
        return this.acr.getBufferSize() - this.index;
    }

    this.fullyBuffered = function() {
        return this.acr.atEnd()
    }

    this.readInt8 = function() {
                
        let lo = this.acr.buffer.charCodeAt(this.index+0)

        if (lo >= 0x80) { // negative
            lo = -(0xff - lo + 1)
        }

        this.index += 1

        // TEMP:
        if (this.index > this.acr.getBufferSize()) {
            throw 'error: ' + this.index + ' > ' + this.acr.getBufferSize()
        }

        return lo
    }

    this.readInt16 = function() {
                
        let lo = this.acr.buffer.charCodeAt(this.index+1)
        lo = (lo << 8) | this.acr.buffer.charCodeAt(this.index+0)

        if (lo >= 0x8000) { // negative
            lo = -(0xffff - lo + 1)
        }

        this.index += 2

        // TEMP:
        if (this.index > this.acr.getBufferSize()) {
            throw 'error: ' + this.index + ' > ' + this.acr.getBufferSize()
        }

        return lo
    }

    this.readInt32 = function() {
        
        let lo = this.acr.buffer.charCodeAt(this.index+3)
        lo = (lo << 8) | this.acr.buffer.charCodeAt(this.index+2)
        lo = (lo << 8) | this.acr.buffer.charCodeAt(this.index+1)
        lo = (lo << 8) | this.acr.buffer.charCodeAt(this.index+0) // will properly detect sign bit

        this.index += 4

        // TEMP:
        if (this.index > this.acr.getBufferSize()) {
            throw 'error: ' + this.index + ' > ' + this.acr.getBufferSize()
        }

        return lo
    }

    this.readUInt32 = function() {
                
        let lo = this.acr.buffer.charCodeAt(this.index+3)
        lo = (lo << 8)  | this.acr.buffer.charCodeAt(this.index+2)
        lo = (lo << 8)  | this.acr.buffer.charCodeAt(this.index+1)
        lo = (lo * 256) + this.acr.buffer.charCodeAt(this.index+0) // mult to not become negative!

        this.index += 4

        // TEMP:
        if (this.index > this.acr.getBufferSize()) {
            throw 'error: ' + this.index + ' > ' + this.acr.getBufferSize()
        }

        return lo
    }

    this.readInt64AsDouble = function() {
        
        // BROKEN!
        let hi = this.acr.buffer.charCodeAt(this.index+7);
        hi = (hi << 8)  | this.acr.buffer.charCodeAt(this.index+6)
        hi = (hi << 8)  | this.acr.buffer.charCodeAt(this.index+5)
        hi = (hi * 256) + this.acr.buffer.charCodeAt(this.index+4) // mult to keep signed
        let lo = this.acr.buffer.charCodeAt(this.index+3)
        lo = (lo << 8)  | this.acr.buffer.charCodeAt(this.index+2)
        lo = (lo << 8)  | this.acr.buffer.charCodeAt(this.index+1)
        lo = (lo * 256) + this.acr.buffer.charCodeAt(this.index+0) // mult to keep signed

        
        let neg = hi >= 0x80000000
        if (neg) {
            throw 'negative 64-bit numbers are not supported by this library to keep it simple'
        }

        // Because of floating point precision we cannot handle > 2**53
        let MAX_HI = 2097152
        if (hi > MAX_HI) {
            throw 'double cannot ensure integer precision above 2^53: got hi word ' + hi + ' but max should be <= ' + MAX_HI
        }

        let n = hi * 4294967296.0
        n += lo

        this.index += 8

        // TEMP:
        if (this.index > this.acr.getBufferSize()) {
            throw 'error: ' + this.index + ' > ' + this.acr.getBufferSize()
        }

        return n
    }
}



function AsyncChunkedReader(file) {
    this.file = file
    this.filesize = Number(this.file.size) // cache for performance

    this.filepos = 0
    this.buffer = null

    // Is the loaded `buffer` data from the very end of the file
    this.atEnd = function() {
        return this.filepos == this.filesize
    }

    // Get size of currently buffered data
    this.getBufferSize = function() { 
        if (this.buffer == null)
            return 0
        return this.buffer.length
    }

    // Drop first n characters from buffer
    this.drop = function(n) { 
        this.buffer = this.buffer.slice(n)
    }

    this.loadMoreBuffer = function(ensure_size, onloaded) {
        // TODO: stop this from being called until he previous one is complete just in case
        const self = this
        const reader = new FileReader();
        const chunkSize = 4194304 * 8
        const end = Math.min(this.filesize, this.filepos + Math.max(ensure_size, chunkSize))
        const slice = this.file.slice(this.filepos, end)
        this.filepos = end // assume correct read
        reader.onload = function(e) {
            const data = e.target.result
            if (self.buffer == null)
                self.buffer = data
            else 
                self.buffer = self.buffer + data
            onloaded()

        }
        reader.readAsBinaryString(slice)
    }
}

// Fit a string into the AsyncChunkedReader interface for testing
function AsyncStringReader(string) {
    this.buffer = string

    this.atEnd = function() {
        return true
    }

    // Get size of currently buffered data
    this.getBufferSize = function() { 
        return this.buffer.length
    }

    // Drop first n characters from buffer
    this.drop = function(n) { 
        this.buffer = this.buffer.slice(n)
    }

    this.loadMoreBuffer = function(ensure_size, onloaded) {
        onLoaded()
    }
}


// Run some tests (always run on loading this file)
let TEST

TEST = (new BinaryBuffer(new AsyncStringReader("}}}}}}\0\0"))).readInt64AsDouble();
if (TEST != 137977929760125) { 
    throw TEST;
}

TEST = (new BinaryBuffer(new AsyncStringReader("ABCDEF\0\0"))).readInt64AsDouble();
if (TEST != 77263311946305) { 
    throw TEST;
}

TEST = (new BinaryBuffer(new AsyncStringReader("\0\0\0\0\0\0\0\0"))).readInt64AsDouble();
if (TEST != 0) { 
    throw TEST;
}

let b1 = new BinaryBuffer(new AsyncStringReader("\0\0\0\0AAAA\0AB\x80"))
TEST = b1.readInt32();
if (TEST != 0) { 
    throw TEST;
}

TEST = b1.readInt32();
if (TEST != 1094795585) { 
    throw TEST;
}

TEST = b1.readInt32();
if (TEST != -2143141632) { 
    throw TEST;
}
