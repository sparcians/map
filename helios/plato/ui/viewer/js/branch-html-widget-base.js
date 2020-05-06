// branch-predictor HTML widget base class that includes common functionality
class BranchHtmlWidgetBase extends BaseWidget {
    constructor(el, id, default_height) {
        super(el, id, default_height)

        this.jbody.html(`
        <div style="display:grid; width:100%; height:100%; margin:0px; padding:0px; overflow:hidden; grid-template-rows:auto 1fr; grid-template-areas: 'top' 'bottom';">
            <div id='header-area' style="grid-area: top;">
                <span id="last-refreshed"></span>&nbsp;&nbsp;
                <span id="last-query"></span>
                <div class="widget-title-control-group" style="float:right">
                    Scroll to:
                    <a href="javascript:void(0);" id="scroll-to-top">top</a>
                    <a href="javascript:void(0);" id="scroll-to-bottom">bottom</a>
                    &nbsp;&nbsp;
                </div>
            </div>
            <div id='table-area' style="overflow:scroll; height:100%; grid-area: bottom;">
            </div>
        </div>`)

        this.jbody.find('regenerate_button')
        this.table_area = this.jbody.find('#table-area')
        this.header_area = this.jbody.find('#header-area')

        this.table_content = ''
        this.empty_table_content = `
            <div style="color: var(--missing-stuff-text-color); padding: 20px;">
                This widget operates in 'manual' mode. It requires a click on the <u>'resync'</u> button in this widget's
                toolbar to initiate a refresh at the current time-slider range.
            </div>`
        this.table_area.html('')

        this.header_area.find('#scroll-to-top').on('click', () => {
            this.table_area.animate({ scrollTop: 0 }, "fast");
        })

        this.header_area.find('#scroll-to-bottom').on('click', () => {
            this.table_area.animate({ scrollTop: this.table_area[0].scrollHeight }, "fast");
        })
    }

    can_follow() {
        return false // Do not allow this widget to follow the global time. This type of view is too slow to generate and transmits tons of data
    }

    is_thumbnail_safe() {
        return false // Do not allow thumbnails because the large table will cause am annoying hang in chrome
    }

    // Called upon start of widget resize
    on_resize_started() {
    }

    // Called upon end of widget resize
    on_resize_complete() {
        this.render()
    }

    on_update_data(json) {
        if (json == null) {
            this.table_content = ''
            return
        }

        const date = new Date();
        const s = `<span class="smalltext">Last refreshed:</span> ${date.getMonth()+1}/${date.getDate()} ${date.getHours()}:${date.getMinutes().toString().padStart(2,'0')}`
        this.header_area.find('#last-refreshed').html(s)

        const last_update = this.get_last_update()
        const msg = `<span class="smalltext">Last query:</span> [${numberWithCommas(last_update.first)} - ${numberWithCommas(last_update.last)}] ${last_update.units}`
        this.header_area.find('#last-query').html(msg)

        const length = json.processorSpecific.stringLength
        const offset = json.processorSpecific.stringBlobOffset

        const arr = new Uint8Array(json.raw_binary_data.slice(offset))
        this.table_content = new TextDecoder("utf-8").decode(arr)
    }

    // Called when needs to re-render
    on_render() {
        // Note that if this is done in on_update_data, the table tends to get clipped. It's possible that it can be
        // done, but maybe something else needs to be updated.
        if (this.table_content == '') {
            this.table_area.html(this.empty_table_content)
        } else {
            this.table_area.html(this.table_content)
        }
    }
}
