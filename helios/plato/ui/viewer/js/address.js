
// Ensures an 0x prefix on a string representing a hex number if one is not there yet.
// Sometimes BigInt.toString includes a 0x... other times it does not.
function ensure_0x_prefix(s) {
    return '0x' + s.replace('0x', '')
}

// Base class for other types that represent an instruction address (e.g. addresses, masked addresses, patterns, etc.)
class AddressLocator {
}

// 64b Address representation
// Note that this NEVER operates on standard integers because javascript cannot represent u64 with complete precision.
// BigInt is used instead.
class Address extends AddressLocator {
    constructor(addr) {
        super()
        if (typeof addr == 'string') {
            if (addr.indexOf('0x') > 0) { // ok if not present or index is 0l
                throw 'Hex address string contained "0x" not at first position. Either exclude it or include it at the first position'
            }
            addr = ensure_0x_prefix(addr)
        }
        this.addr = BigInt(addr)
    }

    as_object() {
        return { type: 'Address',
                 addr: ensure_0x_prefix(this.addr.toString(16)) // convert to string to serialization. bigint not supported by json
                }
    }

    toString() {
        return '0x' + this.addr.toString(16)
    }

    // Render to HTML
    render() {
        return this.addr.toString(16).replace('0x','')
    }

    valueOf() {
        this.addr
    }
}

// 64b Address masked at nibble-granularity
class MaskedAddress extends AddressLocator {
    constructor(addr, mask) {
        super()
        if (!(addr instanceof Address)) {
            this.addr = new Address(addr)
        } else {
            this.addr = addr
        }
        if (mask instanceof BigInt) {
            this.mask = BigInt(ensure_0x_prefix(mask))
        } else {
            this.mask = mask
        }
    }

    as_object() {
        return { type: 'MaskedAddress',
                 addr: ensure_0x_prefix(this.addr.toString(16)), // convert to string to serialization. bigint not supported by json
                 mask: ensure_0x_prefix(this.mask.toString(16))
                }
    }

    toString() {
        return ensure_0x_prefix(this.addr.toString(16))
    }

    // Render to HTML
    render() {
        const addr = this.addr.toString(16).replace('0x', '')
        const mask = this.mask.toString(16).replace('0x', '')
        let out = ''
        for (const i in addr) {
            const a = addr[i]

            const j = mask.length - (addr.length - i)
            const m = mask[j]

            // Mask is either f or 0
            if (m == 'f') {
                out += a
            } else {
                out += '<span class="address-dontcare">' + '-' + '</span>'
            }
        }

        return out
    }

    valueOf() {
        this.addr
    }
}

AddressLocator.from_object = function(o) {
    if (o.type == 'MaskedAddress') {
        return new MaskedAddress(o.addr, o.mask)
    } else if (o.type == 'Address') {
        return new Address(o.addr)
    } else {
        throw 'Unknown type of address object: ' + o.type
    }
}

// Takes a hex string containing 'x' symbols optionally and forms a masked address
MaskedAddress.from_string = function(s) {
    const pattern = new Address(s.replace(/-/g,'0'))

    let m = s.replace('0x','').replace(/[0-9a-f]/g, 'f').replace(/-/g,'0')
    while (m.length < 16) {
        m = 'f' + m // prepend f to make sure upper nibbles match the implicit 0s in the mask
    }
    const mask = BigInt('0x' + m)

    return new MaskedAddress(pattern, mask)
}