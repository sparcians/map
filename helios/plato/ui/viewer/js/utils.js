// Format a number with commas.
// Handy utility from https://stackoverflow.com/questions/2901102/how-to-print-a-number-with-commas-as-thousands-separators-in-javascript
function numberWithCommas(x) {
    var parts = x.toString().split(".");
    parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    return parts.join(".");
}

// Convert a list of color integers [r,g,b] to a DOM color '#rrggbb' after converting integers to hex strings
function colorToHex(c) {
    let r = c[0].toString(16)
    if (r.length < 2)
        r = '0' + r
    let g = c[1].toString(16)
    if (g.length < 2)
        g = '0' + g
    let b = c[2].toString(16)
    if (b.length < 2)
        b = '0' + b

    return '#' + r + g + b
}

// Interpolate between two colors [r1,g1,b1] and [r2,g2,b2] using a fraction in range [0,1]
function interp_color(a, b, fraction) {
    const nc = [
        Math.round(Math.sqrt((a[0]**2 + (b[0]**2 - a[0]**2) * fraction))),
        Math.round(Math.sqrt((a[1]**2 + (b[1]**2 - a[1]**2) * fraction))),
        Math.round(Math.sqrt((a[2]**2 + (b[2]**2 - a[2]**2) * fraction)))
    ]
    return nc
}

// Convert a "rgb(rrr,ggg,bbb[,aaa])" format string to an array of 3 or 4 ints representing color components
function rgb_string_to_array(cs) {
    return cs.replace('rgb','').replace('(','').replace(')','').split(',').map(function(v) {
        return Number(v);
    })
}

// Download a file to the client - usually in browser configured download directory
function download_to_client(content, filename, content_type) {
    var a = $('<a>')[0]
    var file = new Blob([content], {type: content_type});
    a.href = URL.createObjectURL(file);
    a.download = filename;
    a.click();
}

// Async sleep helper. Use as: await sleep(ms)
function sleep(ms) {
    return new Promise(r => setTimeout(r, ms))
}

function time_log(...args) {
    let msg = ''
    args.forEach(arg => { msg += ' ' + arg})
    console.log('[' + performance.now().toFixed() + '] ' + msg)
}

function time_warn(...args) {
    let msg = ''
    args.forEach(arg => { msg += ' ' + arg})
    console.warn('[' + performance.now().toFixed() + '] ' + msg)
}

function time_error(...args) {
    let msg = ''
    args.forEach(arg => { msg += ' ' + arg})
    console.error('[' + performance.now().toFixed() + '] ' + msg)
}

// Squish a string down to a target length, inserting ellipses if needed.
// Target length must be >= 5
function squish_string(msg, target_length, preference='center') {
    console.assert(target_length >= 5)

    if (msg.length <= target_length)
        return msg

    const middle = '...'

    if (preference == 'center') {
        const keep = target_length - middle.length
        const keep_head = Math.floor(keep/2)
        const keep_tail = Math.ceil(keep/2)

        return msg.slice(0, keep_head) + middle + msg.slice(msg.length - keep_tail)
    } else if (preference == 'tail') {
        return middle + msg.slice(msg.length - (target_length - middle.length))
    } else if (preference == 'head') {
    return msg.slice(0, target_length - middle.length) + middle
    } else {
        return '???'
    }
}

function list_contains(l, value) {
    for (const item of l) {
        if (item == value)
            return true
    }
    return false
}

// Returns variable name from stat
function parse_stat_variable(stat_name) {
    const pattern_start = stat_name.indexOf('{')
    if (pattern_start >= 0) {
        const pattern_end = stat_name.indexOf('}')
        const variable = stat_name.substr(pattern_start + 1, pattern_end - pattern_start - 1)
        return variable
    }
    return null
}

// Read a page's GET URL variables and return them as an associative array.
// https://stackoverflow.com/questions/4656843/jquery-get-querystring-from-url
// Returns an object: {key1:value1, etc..}
function get_url_vars() {
    const vars = []
    let hash
    const hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for (let i = 0; i < hashes.length; i++) {
        hash = hashes[i].split('=');
        vars.push(hash[0]);
        vars[hash[0]] = hash[1];
    }
    return vars;
}

// Force a string to be a certain length. Squish it if needed. Or right-pad with spaces
function make_string_length(s, l) {
    if (s.length > l) {
        s = squish_string(s, l, 'tail')
    }

    while (s.length < l) {
        s = s + ' '
    }

    return s
}

// Java-style string hashing
// https://stackoverflow.com/questions/6122571/simple-non-secure-hash-function-for-javascript
Object.defineProperty(String.prototype, "hashCode", {
  enumerable: false,
  value: function() {
        var hash = 0;
        if (this.length == 0) {
            return hash;
        }
        for (var i = 0; i < this.length; i++) {
            var char = this.charCodeAt(i);
            hash = ((hash<<5)-hash)+char;
            hash = hash & hash; // Convert to 32bit integer
        }
        return hash;
   }
});

// Class for a datasource based on its datasource type
function data_source_class(data_type) {
    return 'data-source-bin-item-' + data_type
}

// Class for a widget based on its datasource type
function widget_data_class(data_type) {
    return 'new-widget-bin-item-' + data_type
}

// Class for a datasource based on its datasource type
function stat_data_class(data_type) {
    return 'stat-bin-item-' + data_type
}

// If a values is undefined, return an alternate otherwise return the input value itself.
function or_undefined(val, alternate) {
    return typeof val == 'undefined' ? alternate : val
}