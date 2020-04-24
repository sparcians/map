// Filter control panel
// Configurable to produce a branch filter configuration object
class BranchFilterControlPanel {
    constructor(el, filter_changed_callback, error_msg_func) {
        this.jel = $(el)
        this.filter_changed_callback = filter_changed_callback
        this.error_msg_func = error_msg_func
        this.branches = {}
        this.targets = {}
        this.filter_object = {} // Filter object available to consumers
        this.filter_object_version = 0 // Local, session-only version of filter object for determining "stale" views
        this._do_not_apply_filter = true

        // Lay out filter panel structure
        this.jel.html(`
            <div style="width:100%; height:100%; text-align:center;" class="no-margin-no-padding">
            <span class="smalltext">Branch Addr Filter: </span><span class="smalltext">(half-addr)</span>
            <input value="" style="width:130px; font-family:monospace; font-size:12px;" type="text" id="address-text" title="Enter a new address in hex. Use a '-' anywhere to indicate don't-care in order to create a mask at nibble granularity" /><input type="button" id="add-address" value="Add">
            <div id="branch-filter-bin">
            </div>
            <span class="smalltext">Branch Target Filter: </span><span class="smalltext">(half-addr)</span>
            <input value="" style="width:130px; font-family:monospace; font-size:12px;" type="text" id="target-text" title="Enter a new address in hex. Use a '-' anywhere to indicate don't-care in order to create a mask at nibble granularity" /><input type="button" id="add-target" value="Add">
            <div id="branch-target-filter-bin">
            </div>
            <span class="mediumtext">Branch Class Filter:</span><br/>
            <table class="branch-class-filter-layout no-margin-no-padding" cellpadding=1 cellspacing=0>
                <tbody>
                    <tr>
                        <td>
                            <label style="" for="filter-indirect-indirect" class="smalltext">indirect</label>
                            <input id="filter-indirect-indirect" name="filter-directness-radio" type="radio" title="Indirect only">
                        </td>
                        <td>
                            <label style="" for="filter-indirect-direct" class="smalltext">direct</label>
                            <input id="filter-indirect-direct" name="filter-directness-radio" type="radio" title="Direct only">
                        </td>
                        <td>
                            <label style="" for="filter-indirect-dontcare" class="smalltext" checked>dontcare</label>
                            <input id="filter-indirect-dontcare" checked name="filter-directness-radio" type="radio" title="Do not care about indirect vs direct branches">
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <label style="" for="filter-conditional-unconditional" class="smalltext" checked>uncond</label>
                            <input id="filter-conditional-unconditional" name="filter-conditionality-radio" type="radio" title="Unconditional only">
                        </td>
                        <td>
                            <label style="" for="filter-conditional-conditional" class="smalltext" checked>cond</label>
                            <input id="filter-conditional-conditional" name="filter-conditionality-radio" type="radio" title="Conditional only">
                        </td>
                        <td>
                            <label style="" for="filter-conditional-dontcare" class="smalltext" checked>dontcare</label>
                            <input id="filter-conditional-dontcare" checked name="filter-conditionality-radio" type="radio" title="Do not care about conditional vs unconditional">
                        </td>
                    </tr>
                </tbody>
            </table>
            <a id="clear-all" href="javascript:void(0)">clear all</a><br/>
            </div>
        `)

        this._style_radio(this.jel.find('input:radio'))
        this.jel.find('input:radio').on('change', () => {
            this._apply_filter()
        })
        const address_box = this.jel.find('#address-text')
        address_box.on('keypress', (e) => {
            if (e.which == 13) {
                if (this.add(address_box[0].value, true, true))
                    address_box[0].value = ''
            }
        })
        const target_box = this.jel.find('#target-text')
        target_box.on('keypress', (e) => {
            if (e.which == 13) {
                if (this.add_target(target_box[0].value, true, true))
                    target_box[0].value = ''
            }
        })
        this.jel.find('#add-address').on('click', ()=>{
            if (this.add(address_box[0].value, true, true))
                address_box[0].value = ''
        })
        this.jel.find('#add-target').on('click', ()=>{
            if (this.add_target(target_box[0].value, true, true))
                target_box[0].value = ''
        })
        this.jel.find('#clear-all').on('click', ()=>{
            this.clear_all()
        })

        this.address_bin = this.jel.find('#branch-filter-bin')
        this.target_bin = this.jel.find('#branch-target-filter-bin')


        // After setup is complete, allow interactions to cause us to apply the filter (and invoke the callback)
        this._do_not_apply_filter = false
    }

    // Show a flash highlight effect to draw attention to this control panel. This is mainly for parts of the ui that
    // add branches to this filter besides the branch filter control panel itself. For example, clicking on a button in
    // a widget that is far away from the branch filter panel will want the user to see that the branch filter was
    // updated.
    flash() {
        this.jel.effect('highlight')
    }

    // Same as add(address) but adds a target instead
    add_target(address, include, enabled=true) {
        this.add(address, include, enabled, {is_target:true})
    }

    // Add a branch to the filter and the ui if not already present.
    // address: Supply the address as a string or AddressLocator subclass instance.
    // include: True to include (as opposed to exclude) the branch when filtering.
    // enabled: Enable this branch in the filter. Branches can be added but not enabled to keep things readily available
    //          when quickly looking at different combinations of different branches.
    add(address, include, enabled=true, {is_target=false}={}) {
        this._do_not_apply_filter = true
        try {
            address = this._prepare_address(address)
            if (address == null)
                return false

            const mapping = is_target ? this.targets : this.branches

            if (typeof mapping[address] != 'undefined') {
                mapping[address].effect('shake')
                return false // Already present
            }

            const branch_element = $(`
                    <div class="branch-filter-addr-item">
                        <input id="${address}-enabled" type="checkbox"><span class="address">${address.render()}</span>
                        <a href="javascript:void(0);">x</a>
                        <label style="float:right; top:4px;" for="${address}-excl" class="tinytext">exc</label>
                        <input id="${address}-excl" name="${address}-radio" type="radio" title="include this branch in results">
                        <label style="float:right; top:4px;" for="${address}-incl" class="tinytext">inc</label>
                        <input id="${address}-incl" name="${address}-radio" type="radio" title="exclude this branch from results">
                    </div>`)

            branch_element.find('#' + address.toString() + '-enabled').prop('checked', enabled)
            branch_element.find('#' + address.toString() + '-incl').prop('checked', include)
            branch_element.find('#' + address.toString() + '-excl').prop('checked', !include)
            branch_element.find('a').on('click', () => {
                this.remove(address, {is_target:is_target})
            })

            // Detect toggle of the enable/disable checkbox
            branch_element.find('input:checkbox').on('change', () => {
                this._apply_filter()
            })

            // toggle inclusiveness on change of both incl/excl radio options. toggling one does not fire the other's event
            branch_element.find('input:radio').on('change', () => {
                this._apply_filter()
            })

            // Apply jquery ui style to radio boxes
            this._style_radio(branch_element.find('input:radio'))

            branch_element.address = address
            if (is_target) {
                this.target_bin.prepend(branch_element)
            } else {
                this.address_bin.prepend(branch_element)
            }
            branch_element.effect('highlight')

            mapping[address] = branch_element
        } finally {
            this._do_not_apply_filter = false
        }

        this._apply_filter()

        return true
    }

    // Does this widget/filter contain a specific branch (regardless of include vs exclude, or enabled). Note that this
    // does not consider masked addresses. This is mainly for adding branches from other widgets (e.g branch profile).
    contains_branch(address) {
        address = this._prepare_address(address)
        if (address == null)
            return false // invalid, so not present

        return typeof this.branches[address] != 'undefined'
    }

    // Is this branch present and enabled
    branch_inclusive_and_enabled(address) {
        address = this._prepare_address(address)
        if (address == null)
            return false // invalid, so not present

        if (typeof this.branches[address] == 'undefined')
            return false

        const el = this.branches[address]

        const enabled = el.find('input:checkbox').is(":checked")
        const inclusive = el.find('#' + address.toString() + '-incl').is(":checked")

        return enabled && inclusive
    }

    // Enable a branch or masked branch already in the widget but that might not be enabled. Returns false if branch is
    // not in the widget.
    include_and_enable_existing(address) {
        address = this._prepare_address(address)
        if (address == null)
            return false // invalid, so not present

        if (typeof this.branches[address] == 'undefined')
            return false

        // Check the box
        const el = this.branches[address]

        el.find('#' + address.toString() + '-incl').prop('checked', true).change() // inclusive
        el.find('input:checkbox').prop('checked', true).change() // enabled

        this._apply_filter()

        return true
    }

    remove_target(address) {
        this.remove(address, {is_target:true})
    }

    remove(address, {is_target=false}={}) {
        const mapping = is_target ? this.targets : this.branches
        if (typeof mapping[address] != 'undefined') {
            const branch_element = mapping[address]
            branch_element.remove()

            delete mapping[address]

            this._apply_filter()
        }
    }

    clear_all() {
        this._do_not_apply_filter = true

        try {
            this.jel.find('#filter-conditional-dontcare').prop('checked', true).change()
            this.jel.find('#filter-indirect-dontcare').prop('checked', true).change()

            this.branches = {} // Reset object
            this.targets = {}
            this.address_bin.html('') // Reset display bin content
            this.target_bin.html('')
        } finally {
            this._do_not_apply_filter = false
        }

        this._apply_filter()
    }

    // Take a saved filter (from generate_filter_object) and restores ui content
    // Does not automatically invoke the callback with this new filter data
    restore_ui_from_filter_object(obj) {
        this.clear_all()

        this._do_not_apply_filter = true

        try {
            const classes = obj.classes
            if (!classes.hasOwnProperty('conditionality')) {
                this.jel.find('#filter-conditional-dontcare').prop('checked', true).change()
            } else {
                if (classes.conditionality == 'unconditional')
                    this.jel.find('#filter-conditional-unconditional').prop('checked', true).change()
                else
                    this.jel.find('#filter-conditional-conditional').prop('checked', true).change()
            }

            if (!classes.hasOwnProperty('directness')) {
                this.jel.find('#filter-indirect-dontcare').prop('checked', true).change()
            } else {
                if (classes.directness == 'direct')
                    this.jel.find('#filter-indirect-direct').prop('checked', true).change()
                else
                    this.jel.find('#filter-indirect-indirect').prop('checked', true).change()
            }

            for (const branch of obj.addresses) {
                const enabled = branch.hasOwnProperty('enabled') ? branch.enabled : true
                const addr = AddressLocator.from_object(branch.address)
                this.add(addr, branch.include, enabled)
            }

        } finally {
            this._do_not_apply_filter = false
        }

        this.filter_object = this.generate_filter_object(false)
        this.filter_object_version++
    }

    // Produce the filter object (convertible to json) that server-side processors can use to filter branches.
    // This filter is also used to save and restore the content of this control panel
    generate_filter_object(include_disabled=true) {
        const obj = {
            addresses: [],
            targets: [],
            classes: {},
        }

        if (this.jel.find('#filter-conditional-unconditional').prop('checked')) obj.classes.conditionality = 'unconditional'
        else if (this.jel.find('#filter-conditional-conditional').prop('checked')) obj.classes.conditionality = 'conditional'

        if (this.jel.find('#filter-indirect-direct').prop('checked')) obj.classes.directness = 'direct'
        else if (this.jel.find('#filter-indirect-indirect').prop('checked')) obj.classes.directness = 'indirect'

        // List all branches by address
        for (const k in this.branches) {
            const branch_element = this.branches[k]
            const address = branch_element.address
            const enabled = branch_element.find('#' + address.toString() + '-enabled').is(":checked")

            if (!enabled && !include_disabled) {
                continue
            }

            const branch_obj = {
                address: address.as_object(),
                include: branch_element.find('#' + address.toString() + '-incl').is(":checked"),
                enabled: enabled,
            }

            obj.addresses.push(branch_obj)
        }

        // List all targets
        for (const k in this.targets) {
            const branch_element = this.targets[k]
            const address = branch_element.address
            const enabled = branch_element.find('#' + address.toString() + '-enabled').is(":checked")

            if (!enabled && !include_disabled) {
                continue
            }

            const target_obj = {
                address: address.as_object(),
                include: branch_element.find('#' + address.toString() + '-incl').is(":checked"),
                enabled: enabled,
            }

            obj.targets.push(target_obj)
        }

        return obj
    }

    // Pulsate effect. Fade to yellow and back
    start_pulsate() {
        this.jel.animate({'background-color': 'yellow'},
                         { duration: 400,
                           complete: () => {
                                this.jel.animate({'background-color': ''},
                                { duration: 400,
                                   complete: () => { this.start_pulsate() } } )
                           } } )
    }

    // Stop pulsate effect
    stop_pulsate() {
        this.jel.css('background-color', '')  // Clear overridden animation state
        this.jel.stop() // stop animation
    }

    // Take an address input (string or AddressLocator subclass) and convert to a proper AddressLocator subclass if
    // needed. Log error and return null on error.
    _prepare_address(address) {
        try {
            if (typeof address == 'string') {
                if (address.indexOf('-') >= 0) {
                    address = MaskedAddress.from_string(address)
                }
            }

            if (! (address instanceof AddressLocator)) {
                address = new Address(address)
            }
        } catch (error) {
            let msg = error.message
            if (typeof error == 'string')
                msg = error
            this.error_msg_func('Error adding address/mask to branch filter', msg)
            return null
        }

        return address
    }

    _on_toggle_inclusive() {
        this._apply_filter()
    }

    // Apply the current filter state by generating a new filter object and invoking the filter_changed_callback given
    // at construction.
    _apply_filter(include_disabled=false) {
        if (this._do_not_apply_filter)
            return

        // Cache the new filter
        this.filter_object = this.generate_filter_object(include_disabled)
        this.filter_object_version++
        this.filter_changed_callback(this.filter_object, this.filter_object_version)
    }

    // Apply styling to radio boxes
    _style_radio(jel) {
        jel.checkboxradio({
            icon: false,
              classes: {
                'ui-checkboxradio': 'widget-title-group-option',
                'ui-checkboxradio-label': 'widget-title-group-option',
                'ui-checkboxradio-checked': 'selected',
            }
        })
    }
}
