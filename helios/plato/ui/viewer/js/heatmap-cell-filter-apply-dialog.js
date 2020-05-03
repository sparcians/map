// Dialog for selecting which other widgets to apply some state to.
// TODO: This should be refactored to not be specific to these heatmap settings (i.e. move that into a subclass).
class ApplyCellFilterToWidgetsDialog {
    constructor() {
        this.dlg_div = $('#apply-widget-state').clone()

        this.state_div = this.dlg_div.find('.state-description')
        this.state_div.html('')

        this.widget_list = this.dlg_div.find('.active-widget-list')
        this.widget_list.html('')

        this.num_selected = this.dlg_div.find('#num-selected')

        this._last_applied = [] // widgets to which the new state was last applied through this dialog

        // Build the dialog once.
        this.dlg_div.dialog({
            autoOpen: false,
            width: 600,
            height: 500,
            modal: true,
            buttons: {
                "Ok": () => {
                    this._attempt()
                },
                Cancel: () => {
                    this._exit()
                }
            },
            open: function (event, ui) {
                $(this).css('height', 'auto') // Make sure to resize vertically

                // This cannot be done because it breaks auto-height.
                //$(this).css('position', 'fixed') // keep the dialog in place while the user scrolls the page looking at widgets
            },
        })

        this.dlg_div.keypress((e) => {
            if (e.keyCode == $.ui.keyCode.ENTER) {
                this._attempt()
            } else if (e.keyCode == $.ui.keyCode.ESCAPE) {
               this._exit()
            }
        })
    }

    show(widget_list, bank, table, row) {

        // Describe filtering
        this.state_div.html(`Filter on access to bank shp table ${bank}, Table ${table}, Row ${row}`)

        // Store cell coords for when accepted
        this._bank = bank
        this._table = table
        this._row = row

        // Show the dialog (required to start styling the elements w/ jquery - or so it seems)
        this.dlg_div.dialog('open')

        // Make a list of widgets
        this.widget_list.html('')
        for (const pwidget of widget_list) {

            const id = `select-${pwidget.widget_id}`

            const label = $(`<label for="${id}" class="smalltext" style="margin: 4px; padding: 2px;">${pwidget.name} (${pwidget.typename}) #${pwidget.widget_id}</label>`)
            const checkbox = $(`<input type="checkbox" id="${id}" name="${id}" />`)

            this.widget_list.append(label)
            label.prepend(checkbox)
            this.widget_list.append($('<br/>'))

            // Disabled because this prevents clicks from working for just one checkbox sometimes for no clear reason.
            // Tabbing and pressing space works so it seems like something intercepts the clicks.
            //checkbox.checkboxradio({
            //    icon: false
            //});

            checkbox[0].pwidget = pwidget

            checkbox.prop('checked', list_contains(this._last_applied, pwidget)).change()
            checkbox.on('change', () => { this._update_selection_count() })
        }

        this._update_selection_count()
    }

    _get_checked_widgets() {
        const selections = []
        for (const _el of this.widget_list.find('input[type=checkbox]')) {
            if($(_el).is(':checked')) {
                selections.push(_el.pwidget)
            }
        }
        return selections
    }

    _update_selection_count() {
        this.num_selected.html(this._get_checked_widgets().length.toString())
    }

    // Attempt to apply the settings
    _attempt() {
        const selections = this._get_checked_widgets()

        this.widget_list.html('') // Remove options so names of checkboxes are not conflicting with other dialogs

        for (const pwidget of selections) {
            try {
                pwidget.geometry_filters.apply_config_data({ bank: this._bank, table: this._table, row: this._row })
                viewer.refresh_widget(pwidget, {force_sync: true}) // Refresh with current state (i.e. do not change time range)
            } catch (ex) {
                time_error(ex, ex.stack)
            }
        }

        this._last_applied = selections
        this.dlg_div.dialog('close')
    }

    // Exit the dialog
    _exit() {
        this.widget_list.html('') // Remove options so names of checkboxes are not conflicting with other dialogs

        this.dlg_div.dialog('close')
    }
}
