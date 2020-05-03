// Describes a simple minimal heatmap widget


// TEMP: Testing stuff

ydim = 32
xdim = 1024

xValues = [...Array(xdim).keys()];
yValues = [...Array(ydim).keys()];

function zeroZ() {
    z = [];
    for(const y of yValues) {
        z.push([]);
        zy = z[z.length - 1]
        for (const x of xValues) {
            zy.push(0);
        }
    }
    return z;
}


// End of temp data

// HeatMap widget. Construct with the DOM div for the heatmap
class MinimalHeatMapWidget extends BaseWidget {
    constructor(el, id) {
        super(el, id, 300)
        this.canvas_name = this.name + '-heatmap-canvas'

        // Set up the heatmap grid of plots
        const content = `
            <canvas id="${this.canvas_name}" style="width:100%; height:100%"></canvas>
        `
        this.jbody.html(content)
        this.canvas = $(el).find('#' + this.canvas_name)[0]

        // Initial data
        // TODO: Get this from server
        this.zValues = zeroZ()
        this.xdim = xdim
        this.ydim = ydim
    }

    // Called upon start of widget resize
    on_resize_started() {
    }

    // Called upon end of widget resize
    on_resize_complete() {
        this.canvas.width = this.jbody.width()
        this.canvas.height = this.jbody.height()
        this.ctx = this.canvas.getContext('2d')
        const vscale = Math.floor(this.canvas.height / this.ydim)
        this.ctx.scale(1,vscale) // vertical scale

        this.render()
    }

    on_update_data(data) {
        this.zValues = data
    }

    // Called when needs to re-render
    on_render() {
        if (this.zValues == null) {
            // Do not plot
            // TODO: Clear data
            return
        }

        const vscale = 2

        const w = this.canvas.width
        const h = this.canvas.height

        for (const y of yValues) {
            for (const x of xValues) {
                const v = this.zValues[y*xdim + x]
                const value = Math.min(255, Math.floor(v*50))
                // TODO: Use a colormap along with min/max info to do this
                this.ctx.fillStyle = colorToHex([value, value, value, 255])
                this.ctx.fillRect(x,y,1,1)
            }
        }
    }

    get_request_kwargs() {
        return {stat_col: 'thrash_1'}  // TODO: use actual stats from this.data_source
    }

    // New data-source
    on_assign_data() {
        // TODO: Behave like HeatmapWidget. perhaps share code
    }

    // Render using canvas image and up-sampling w/ interpolation
    /*on_render() {
        const vscale = 6

        const w = this.canvas.width
        const h = this.canvas.height

        const temp_canvas = $('<canvas>').attr('width',xdim).attr('height',ydim)[0]
        const temp_ctx = temp_canvas.getContext('2d')
        const img_data = temp_ctx.createImageData(xdim,ydim)
        const imgd = img_data.data
        for (const y of yValues) {
            const zy = this.zValues[y]
            for (const x of xValues) {
                const v = zy[x]
                imgd[(((y)*xdim + x) * 4)+0] = Math.min(255, (v+0.5)*255)
                imgd[(((y)*xdim + x) * 4)+1] = Math.min(255, (v+0.5)*255)
                imgd[(((y)*xdim + x) * 4)+2] = Math.min(255, (v+0.5)*255)
                imgd[(((y)*xdim + x) * 4)+3] = 255
            }
        }
        temp_ctx.putImageData(img_data, 0, 0)

        const ctx = this.canvas.getContext('2d')
        ctx.scale(1, 2)
        ctx.drawImage(temp_canvas, 0, 0)

    }*/
}
MinimalHeatMapWidget.typename = 'bp-minheatmap'
MinimalHeatMapWidget.description = 'A minimal version of the branch predictor heatmap'
MinimalHeatMapWidget.processor_type = Processors.SHP_HEATMAP
MinimalHeatMapWidget.data_type = DataTypes.SHP_TRAINING_TRACE