// Control panel for choosing a particular cell location in SHP. Meant to be embedded in widgets that display branch
// lists (profile or trace)
class GeometryFilterControls {

    constructor(owner) {
        owner = $(owner)

        this.onchange = () => {}

        this.bank = null
        this.table = null
        this.row = null

        this.el = $(`
            <div class="widget-title-control-group" style="float:right" title="Show only branches accessing the chosen table/bank/row. Leave blank for any. Press enter to apply changes">
                Show Accessing:
                bank<input class="bp-trace-local-filter-input passive" type="text" id="bank" value="*">
                table<input class="bp-trace-local-filter-input passive" type="text" id="table" value="*">
                row<input class="bp-trace-local-filter-input passive" type="text" id="row" value="*">
                <a href="javascript:void(0);">(clear)</a>
            </div>`)
        owner.append(this.el)

        this.el.find('.bp-trace-local-filter-input').on('keyup', (e) => { return this.keyup(e) })
                                                    .on('focus', (e) => { return this.focus(e) })
                                                    .on('blur', (e) => { return this.blur(e) })
                                                    .on('keydown', (e) => { return this.keydown(e)})

        this.el.find('a').on('click', () => { this.clear(); })
    }

    _assign_value(id, val) {
        let v = val == null ? null : parseInt(val)

        if (id == 'bank') {
            this.bank = v
        } else if (id == 'table') {
            this.table = v
        } else if (id == 'row') {
            this.row = v
        }
    }

    // Gets current value associated with an input name. null if none, and a number otherwise
    _get_value(id, val) {
        if (id == 'bank') {
            return this.bank
        } else if (id == 'table') {
            return this.table
        } else if (id == 'row') {
            return this.row
        }

        throw Error(`No known value for id ${id}`)
    }

    _validate(input) {
        if (input[0].value.match(GeometryFilterControls.re_numeric)) {
            input.removeClass('invalid-input-value')
            return true
        }

        if (input[0].value.match(GeometryFilterControls.re_empty) ||
            input[0].value.match(GeometryFilterControls.re_asterisk))
        {
            input.removeClass('invalid-input-value')
            return true
        }

        input.addClass('invalid-input-value')
        return false
    }

    _apply(input) {
        let val
        let display_val
        if (input[0].value.match(GeometryFilterControls.re_empty) ||
            input[0].value.match(GeometryFilterControls.re_asterisk))
        {
            val = null
            display_val = '*'
        } else {
            const valid = input[0].value.match(GeometryFilterControls.re_numeric)
            val = valid ? parseInt(input[0].value) : null
            display_val = valid ? val.toString() : '*'
        }

        input[0].value = display_val

        this._assign_value(input[0].id, val)

        this.onchange()
    }

    _try_apply(input) {
        if (!this._validate(input)) { // enter
            // Reject the change
            input.effect('shake', {distance: 5})
            return false
        } else {
            this._apply(input)
            input.blur()
            input.effect('highlight')
            return true
        }
    }

    // Remove the style associated with being actively edited
    _remove_active_style(input) {
        input.removeClass('invalid-input-value')
        input.addClass('passive')
    }

    // Revert content of textbox to the prior value
    _revert(input) {
        const val = this._get_value(input[0].id)
        input[0].value = val == null ? '*' : val.toString()
    }

    keyup(evt) {
        const input = $(evt.target)
        if (evt.keyCode == 13) {
            this._try_apply(input)
        } else if (evt.keyCode == 27) { // escape
            // revert
            this._revert(input)
            input.blur()
        } else {
            this._validate(input) // just refresh the error coloring
        }
    }

    keydown(evt) {
        const input = $(evt.target)
        if (evt.keyCode == 9) { // tab
            // Try and accept before allowing the textbox to blur
            if (!this._try_apply(input)) {
                this._remove_active_style(input)
            }
        }
    }

    focus(evt) {
        const input = $(evt.target)
        input.removeClass('passive')
    }

    blur(evt) {
        const input = $(evt.target)
        this._remove_active_style(input)

        // revert to prior value
        this._revert(input)
    }

    clear() {
        this._assign_value('bank', null)
        this.el.find('#bank')[0].value = '*'
        this._assign_value('table', null)
        this.el.find('#table')[0].value = '*'
        this._assign_value('row', null)
        this.el.find('#row')[0].value = '*'

        this.onchange()
    }

    get_config_data() {
        return {
            'bank': this.bank,
            'table': this.table,
            'row': this.row,
        }
    }

    apply_config_data(d) {
        const bank = or_undefined(d.bank, null)
        this._assign_value('bank', bank)
        this.el.find('#bank')[0].value = bank == null ? '*' : bank.toString()

        const table = or_undefined(d.table,null)
        this._assign_value('table', table)
        this.el.find('#table')[0].value = table == null ? '*' : table.toString()

        const row = or_undefined(d.row,null)
        this._assign_value('row', row)
        this.el.find('#row')[0].value = row == null ? '*' : row.toString()
    }
}

GeometryFilterControls.re_numeric = /^[0-9]+$/g
GeometryFilterControls.re_asterisk = /^ *\* *$/g
GeometryFilterControls.re_empty = /^ *$/g
