import wx
import wx.lib.dragscroller as drgscrlr
import os
import colorsys # For easy HSL to RGB conversion
import sys
import time
import logging

from .selection_manager import Selection_Mgr
from .input_decoder import Input_Decoder
from .hover_preview import HoverPreview, HoverRedrawEvent, EVT_HOVER_REDRAW
from functools import partial
from . import autocoloring
import model.highlighting_utils as highlighting_utils

# Import Argos transaction database module from SPARTA
# #__MODULE_ENV_VAR_NAME = 'RENDERED_MODULE_DIR'
# #added_path = os.environ.get(__MODULE_ENV_VAR_NAME, None)

try:
    import core
except ImportError as e:
    print('Argos failed to import module: "renderer". Argos requires Make'.format(), file = sys.stderr)
    print('Exception: {0}'.format(e), file = sys.stderr)
    sys.exit(1)


class Layout_Canvas(wx.ScrolledWindow):

    __AUTO_CANVAS_SIZE_MARGIN = 40 # Pixels

    MIN_WIDTH = 500
    MIN_HEIGHT = 500

    # # Construct a Layout_Canvas
    #  @param parent Parent Layout_Frame
    #  @param context Associated Layout_Context
    #  @param dialog Element Properties Dialog for this canvas/frame
    #  @param ID Window ID
    def __init__(self,
                 parent,
                 context,
                 dialog,
                 ID = -1,
                 pos = wx.DefaultPosition,
                 size = wx.DefaultSize,
                 style = wx.NO_FULL_REPAINT_ON_RESIZE):

        self.__win = wx.ScrolledWindow.__init__(self, parent)
        self.__renderer = core.Renderer()
        assert len(autocoloring.REASON_BRUSHES) > 0 and len(autocoloring.BACKGROUND_BRUSHES) > 0
        self.SetRendererBrushes()
        self.__renderer.setExtensions(context.GetExtensionManager())
        self.__dragscrl = drgscrlr.DragScroller(self, rate = 300, sensitivity = .01)
        self.__WIDTH = 2000
        self.__HEIGHT = 2000

        # full canvas scale
        self.__canvas_scale = 1.0

        self.__SCROLL_RATE = 20
        self.__scroll_ratios = (0, 0)
        self.__UpdateScrollbars()
        self.__parent = parent
        self.__context = context
        self.__layout = context.GetLayout()
        self.__dlg = dialog
        self.__draw_grid = False
        #TODO, make neat ####################################
        self.__gridsize = 14
        self.__gridlines = []
        self.__UpdateGrid(self.__gridsize)
        #End TODO ###########################################
        self.__selection = Selection_Mgr(self, dialog)
        self.__user_handler = Input_Decoder(self, self.__selection)
        self.__buffer = None
        self.__backbuffer = None
        # used to store original image segment when hover text drawn
        self.__dirty_position = None
        self.__clean_buffer = None

        self.__schedule_line_draw_style = 4 # classic
        self.__schedule_scale = 1.0

        # used for color highlighting of transactions
        self.__colored_transactions = {}

        self.__hover_preview = HoverPreview(self, context)
        # Load images
        # # \todo Use a image map
        self.__mongoose_image = self.GetMongooseLogo()

        try:
            self.__fnt_layout = wx.Font(12, wx.FONTFAMILY_MODERN, wx.NORMAL, wx.NORMAL, face = 'Monospace')
        except:
            # Pick a fallback generic modern font (not by name)
            self.__fnt_layout = wx.Font(12, wx.FONTFAMILY_MODERN, wx.NORMAL, wx.NORMAL)

        # set up font
        temp_dc = wx.MemoryDC()
        temp_dc.SetFont(self.__fnt_layout)
        self.__renderer.setFontFromDC(temp_dc)
        del temp_dc

        # Disable background erasing
        def disable_event(*pargs, **kwargs):
            pass

        self.Bind(wx.EVT_ERASE_BACKGROUND, disable_event)
        self.Bind(wx.EVT_PAINT, self.OnPaint)
        self.Bind(wx.EVT_SIZE, self.OnSize)
        # Ensure that the buffers are setup correctly
        self.OnSize(None)
        # Bindings for mouse events
        self.Bind(wx.EVT_LEFT_DOWN,
                  partial(self.__user_handler.LeftDown, canvas = self,
                          selection = self.__selection, dialog = self.__dlg))
        self.Bind(wx.EVT_MOTION,
                  partial(self.__user_handler.MouseMove, canvas = self,
                  selection = self.__selection, context = self.__context,
                  mouse_over_preview = self.__hover_preview))
        self.Bind(wx.EVT_LEFT_DCLICK,
                  partial(self.__user_handler.LeftDouble, canvas = self,
                          selection = self.__selection, dialog = self.__dlg))
        self.Bind(wx.EVT_LEFT_UP,
                  partial(self.__user_handler.LeftUp, canvas = self,
                          selection = self.__selection, dialog = self.__dlg))
        self.Bind(wx.EVT_RIGHT_DOWN,
                  partial(self.__user_handler.RightDown, canvas = self, drgscrl = self.__dragscrl))
        self.Bind(wx.EVT_RIGHT_UP,
                  partial(self.__user_handler.RightUp, canvas = self, drgscrl = self.__dragscrl))
        # Bindings for key events
        self.Bind(wx.EVT_KEY_DOWN,
                  partial(self.__user_handler.KeyDown, canvas = self,
                          selection = self.__selection, dialog = self.__dlg,
                          context = self.__context))
        self.Bind(wx.EVT_KEY_UP,
                  partial(self.__user_handler.KeyUp, canvas = self,
                          selection = self.__selection, dialog = self.__dlg,
                          context = self.__context))

        self.Bind(wx.EVT_MOUSEWHEEL, partial(self.__user_handler.MouseWheel, canvas = self))

        # required to actually receive key events
        self.SetFocus()

        self.Bind(wx.EVT_SCROLLWIN, self.Scrollin)

        self.Bind(EVT_HOVER_REDRAW, self.OnHoverRedraw)

    # # Size of the canvas grid
    @property
    def gridsize(self):
        return self.__gridsize

    # # Snap sensitivity range (+/- each gridline)
    @property
    def range(self):
        return self.__snap_capture_delta

    @property
    def scrollrate(self):
        return self.__SCROLL_RATE

    @property
    def context(self):
        return self.__context

    # # Returns bitmap of Mongoose logo
    # Currently hardcoded
    def GetMongooseLogo(self):
        this_script_filename = os.path.join(os.getcwd(), __file__)
        mongoose_logo_filename = os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/mongoose_small.png"

        if os.path.exists(mongoose_logo_filename):
            return wx.Bitmap(mongoose_logo_filename, wx.BITMAP_TYPE_PNG)
        else:
            return wx.EmptyBitmapRGBA(10, 10)

    # # Returns the parent frame which owns this Canvas
    def GetFrame(self):
        return self.__parent

    # # Returns a tuple showing the area of the canvas visible to the screen
    #  @return (x,y,w,h)
    def GetVisibleArea(self):
        x, y = self.GetViewStart()
        w, h = self.GetClientSize()
        return (x, y, w, h)

    # # Updates color for a transaction
    def UpdateTransactionColor(self, el, annotation):
        if el.HasProperty('auto_color_basis') \
          and el.HasProperty('color_basis_type'):
            auto_color_basis = el.GetProperty('auto_color_basis')
            color_basis_type = el.GetProperty('color_basis_type')
            content_type = el.GetProperty('Content')
            record = self.GetTransactionColor(annotation, content_type, color_basis_type, auto_color_basis)
            if record:
                string_to_display, brush, _, _ = record
            else:
                string_to_display, brush, _ = self.AddColoredTransaction(annotation,
                                                                         content_type,
                                                                         color_basis_type,
                                                                         auto_color_basis)
        else:
            string_to_display = self.__hover_preview.annotation
            brush = wx.Brush((200, 200, 200))

        return string_to_display, brush

    # # Draw hover text onto specified DC.
    # Function called by either OnPaint or OnHoverRedraw
    def DoHoverTextDraw(self, dc):

        string_to_display, brush = self.UpdateTransactionColor(self.__hover_preview.element, \
                                                               self.__hover_preview.annotation)

        # set brush and pen
        dc.SetBrush(brush)
        BORDER_LIGHTNESS = 0.7
        dc.SetPen(wx.Pen((int(brush.Colour[0] * BORDER_LIGHTNESS),
                          int(brush.Colour[1] * BORDER_LIGHTNESS),
                          int(brush.Colour[2] * BORDER_LIGHTNESS)), 1)) # Darker border around brush
        # draw text and box
        text = self.__hover_preview.GetText()

        if text == '':
            return # Do not draw an empty rectangle here. It causes dirt to be
                   # left behind when the 1px-wide buffer is restored

        tr = self.__hover_preview.position

        br = dc.GetMultiLineTextExtent(text)

        rect_mop = [tr[0], tr[1], br[0] + 1, br[1] + 1]

        visible_area = self.GetVisibleArea()
        box_right = rect_mop[0] + rect_mop[2]
        box_bottom = rect_mop[1] + rect_mop[3]
        if box_right > visible_area[2]: # off the right edge
            # shift left
            rect_mop[0] -= (box_right - visible_area[2])
        if box_bottom > visible_area[3]:
            rect_mop[1] -= (box_bottom - visible_area[3])

        if rect_mop[0] < 0 or rect_mop[1] < 0:
            return # screen too small
        # capture old swatch and position so we can patch it later.
        self.__clean_buffer = self.__buffer.GetSubBitmap(rect_mop)
        self.__dirty_position = rect_mop[0], rect_mop[1]

        dc.DrawRectangle(*rect_mop)
        dc.DrawLabel(text, rect = rect_mop)

    # # Here the canvas does all it's drawing
    def DoDraw(self, dc):
        logging.debug('xxx: Started C drawing')

        # Reapply font
        dc.SetFont(self.__fnt_layout)
        # Draw grid
        # This has effectively 0 cost
        if self.__draw_grid:
            brush = dc.GetBackground()
            brush.SetColour('black')
            dc.SetBackground(brush)
            dc.Clear()
            dc.SetPen(wx.Pen((220, 220, 220), 1))
            # dc.SetLogicalFunction(wx.INVERT)
            xoff, yoff = self.GetRenderOffsets()
            for x, y, x1, y1 in self.__gridlines:
                dc.DrawLine(x - xoff, y - yoff, x1 - xoff, y1 - yoff)

        t_start = time.monotonic()
        self.__renderer.drawElements(dc, self, self.__context.GetHC())

        logging.debug('{0}s: C drawing'.format(time.monotonic() - t_start))

        # #t_start = time.monotonic()
        # #
        # ## Draw elements in draw-order
        # #for pair in self.__context.GetElements():
        # #    e = pair.GetElement()
        # #    string_to_display = pair.GetVal()
        # #    (x,y),(w,h) = e.GetProperty('position'),e.GetProperty('dimensions')
        # #    (x,y) = (x-xoff, y-yoff)
        # #
        # #    pen  = wx.Pen(e.GetProperty('color'), 1)
        # #    dc.SetPen(pen)
        # #
        # #    uop_id = string_to_display[:1]
        # #    if uop_id in self.__background_brushes \
        # #       and e.GetProperty('Content') == 'auto_color_annotation':
        # #        dc.SetBrush(self.__background_brushes[uop_id])
        # #    else:
        # #        dc.SetBrush(dc.GetBackground())
        # #
        # #    dc.DrawRectangle(x,y,w,h)
        # #
        # #    # Draw text clipped to this element
        # #    dc.SetClippingRegion(x,y,w,h)
        # #
        # #    if e.GetProperty('Content') == 'image':
        # #        dc.DrawBitmap(self.__mongoose_image, x, y)
        # #    else:
        # #        dc.DrawText(string_to_display,x+2,y+2)
        # #
        # #    dc.DestroyClippingRegion()
        # #
        # #logging.debug('Py drawing took {0}s'.format(time.monotonic() - t_start))

    # # Flip the front and back buffers
    def __flip(self):
        self.__buffer, self.__backbuffer = self.__backbuffer, self.__buffer
        self.Update()
        self.Refresh()

    def Scrollin(self, evt):
        wx.ScrolledWindow.Refresh(self)

        bounds = self.__GetScrollBounds()
        x_bound = bounds[0] - self.scrollrate
        y_bound = bounds[1] - self.scrollrate

        w_pix, h_pix = self.GetClientSize()
        percent_bar_x = w_pix / (1.0 * self.__WIDTH * self.__canvas_scale)
        percent_bar_y = h_pix / (1.0 * self.__HEIGHT * self.__canvas_scale)

        self.__scroll_ratios = self.GetScrollPos(wx.HORIZONTAL) / (x_bound * 1.0) + percent_bar_x / 4.0, \
                              self.GetScrollPos(wx.VERTICAL) / (y_bound * 1.0) + percent_bar_y / 4.0

        # don't let old clean buffer be written in wrong spot
        self.__clean_buffer = None

        # If we're in edit mode, update the cursor location in the toolbar
        if self.__user_handler.GetEditMode():
            (x, y) = self.GetMousePosition()
            offset = self.CalcScrollInc(evt)
            scroll_units = self.GetScrollPixelsPerUnit()

            if evt.GetOrientation() == wx.HORIZONTAL:
                x += offset * scroll_units[0]
            elif evt.GetOrientation() == wx.VERTICAL:
                y += offset * scroll_units[1]

            self.__parent.UpdateMouseLocation(x, y)

        evt.Skip()

    # # Returns the position of the mouse within the canvas
    def GetMousePosition(self):
        return self.CalcUnscrolledPosition(self.ScreenToClient(wx.GetMousePosition()))

    # # Execute all drawing logic, flip buffers
    def FullUpdate(self):

        # update quad tree (if needed)
        self.__context.MicroUpdate()
        self.Refresh()
        # Tell the Element Prop's Dlg window to update itself
        self.__dlg.Refresh()

    # # Execute all drawing logic, flip the buffers, blit the front buffer to
    #  the screen
    def OnPaint(self, event):
        dc = wx.MemoryDC()
        dc.SelectObject(self.__backbuffer)
        dc.Clear()
        dc.SetUserScale(self.__canvas_scale, self.__canvas_scale)
        self.DoDraw(dc)
        self.__selection.Draw(dc)

        self.__buffer, self.__backbuffer = self.__backbuffer, self.__buffer
        dc = wx.BufferedPaintDC(self, self.__buffer)

        # update the hover
        if self.__hover_preview.IsEnabled():
            mousePos = wx.GetMousePosition()
            screenPos = self.GetScreenPosition()
            localMousePos = (mousePos[0] - screenPos[0], mousePos[1] - screenPos[1])
            self.__hover_preview.HandleMouseMove(localMousePos, self)

    # # Perform limited redraw.
    # Only updates the hover text that appears when object is moused over.
    def OnHoverRedraw(self, event):
        dc = wx.MemoryDC()
        dc.SelectObject(self.__buffer)
        if self.__clean_buffer:
            dc.DrawBitmap(self.__clean_buffer, self.__dirty_position[0], self.__dirty_position[1])
        if self.__hover_preview.show:
            dc.SetFont(self.__fnt_layout)
            self.DoHoverTextDraw(dc)
        client_dc = wx.ClientDC(self)
        wx.BufferedDC(client_dc, self.__buffer)

    # # Returns Hover Preview
    def GetHoverPreview(self):
        return self.__hover_preview

    # # Turns on or off the drawing of the grid
    def ToggleGrid(self):
        self.__draw_grid = not self.__draw_grid
        self.Refresh()

    # # Refresh. Recalc size and forward to superclass
    def Refresh(self):
        # Recalculate canvas size
        if self.__CalcCanvasSize():
            self.__UpdateScrollbars()
            self.__UpdateGrid(self.__gridsize)

        super(self.__class__, self).Refresh()

    # # Gets called when the window is resized, and when the canvas is initialized
    def OnSize(self, event):
        # Here we need to create a new off-screen buffer to hold
        # the in-progress drawings on.
        width, height = self.GetClientSize()
        if width == 0:
            width = 1
        if height == 0:
            height = 1
        self.__buffer = wx.Bitmap(width, height)
        self.__backbuffer = wx.Bitmap(width, height)
        # Now update the screen
        self.FullUpdate()

    # # Gets the selection manager for this canvas
    def GetSelectionManager(self):
        return self.__selection

    # # Gets the input decoder for this canvas
    def GetInputDecoder(self):
        return self.__user_handler

    # # Forward on the draw-orderd list of all Elements in this
    #  frame/canvas/layout...
    def GetElements(self):
        return self.__context.GetElements()

    # # Return Element/Value pairs
    def GetElementPairs(self):
        return self.__context.GetElementPairs()

    # # Compute and returns display bounds
    def GetBounds(self):
        posx, posy, width, height = self.GetVisibleArea()
        mins = self.CalcUnscrolledPosition((0, 0))
        bounds = (mins[0],
                  mins[1],
                  mins[0] + width / self.__canvas_scale,
                  mins[1] + height / self.__canvas_scale)
        return bounds

    # # Returns element pairs suitable for drawing
    def GetDrawPairs(self):
        bounds = self.GetBounds()
        return self.__context.GetDrawPairs(bounds)

    def GetVisibilityTick(self):
        return self.__context.GetVisibilityTick()

    # # Return stored image
    def GetMongooseImage(self):
        return self.__mongoose_image

    # # Set the brushes in the renderer to the current brush set
    def SetRendererBrushes(self):
        self.__renderer.setBrushes(autocoloring.REASON_BRUSHES.as_dict(), autocoloring.BACKGROUND_BRUSHES.as_dict())

    # # Resets the renderer brushes and purges the brush cache - needed whenever the palette or color shuffle mode changes
    def RefreshBrushes(self):
        self.SetRendererBrushes()
        self.PurgeBrushCache()

    # # Returns brushes needed to draw elements
    def GetBrushes(self):
        return autocoloring.BACKGROUND_BRUSHES, autocoloring.REASON_BRUSHES

    # #Returns offsets needed to render
    def GetRenderOffsets(self):
        return self.CalcUnscrolledPosition((0, 0))

    # # Returns the Cython renderer
    def GetRenderer(self):
        return self.__renderer

    # # Returns element settings dialog
    def GetDialog(self):
        return self.__dlg

    # # Returns global setting for schedule line drawing
    def GetScheduleLineStyle(self):
        return self.__schedule_line_draw_style

    def GetScale(self):
        return self.__canvas_scale

    def GetScheduleScale(self):
        return self.__schedule_scale

    # # Updates the highlighting state of all cached transactions
    def UpdateTransactionHighlighting(self):
        for key, val in self.__colored_transactions.items():
            if self.__context.IsUopUidHighlighted(val[3]):
                self.__colored_transactions[key] = val[:2] + (True,) + val[3:]
            else:
                self.__colored_transactions[key] = val[:2] + (False,) + val[3:]

    def GetTransactionColor(self, annotation, content_type, color_basis_type, auto_color_basis):
        return self.__colored_transactions.get('%s:%s:%s:%s' % (annotation, color_basis_type, auto_color_basis, hash(content_type)))

    # # Track the tagging of one transaction
    #  @note OThis can be called mutiple times to override existing colors
    def AddColoredTransaction(self, annotation, content_type, color_basis_type, auto_color_basis):
        if len(self.__colored_transactions) > 10000:
            self.__colored_transactions.clear()

        string_to_display, brush = self.__renderer.parseAnnotationAndGetColor(annotation,
                                                                              content_type,
                                                                              color_basis_type,
                                                                              auto_color_basis)
        key = '%s:%s:%s:%s' % (annotation, color_basis_type, auto_color_basis, hash(content_type))
        uop_uid = highlighting_utils.GetUopUid(annotation)
        if self.__context.IsUopUidHighlighted(uop_uid):
            highlighted = True
        else:
            highlighted = False
        self.__colored_transactions[key] = (string_to_display, brush, highlighted, uop_uid)
        return string_to_display, brush, highlighted

    # # Gets old-style coloring (no basis configured)
    def GetAutocolorColor(self, annotation):
        string_to_display, brush = self.__renderer.parseAnnotationAndGetColor(annotation,
                                                        'auto_color_annotation')
        return brush.GetColour()

    # # remove all entries from the dictionary of colored transactions
    # will auto regenerate next render frame
    def PurgeBrushCache(self):
        self.__colored_transactions.clear()
        self.__context.FullRedraw()
        self.Refresh()

    # # Set the global style for schedule lines
    def SetScheduleLineStyle(self, style):
        if style != self.__schedule_line_draw_style:
            self.__schedule_line_draw_style = style
            self.__context.FullUpdate()
            self.Refresh()

    def SetScale(self, scale):
        # cap at 2x zoom in
        if scale > 2:
            self.__canvas_scale = 2
        else:
            self.__canvas_scale = scale
        # purge outdated buffer
        self.__clean_buffer = None
        self.__CalcCanvasSize()
        self.__UpdateScrollbars()
        self.__UpdateGrid(self.__gridsize)

        super(self.__class__, self).Refresh()

    def SetScheduleScale(self, scale):
        self.__schedule_scale = scale
        # purge outdated buffer
        self.__clean_buffer = None
        self.__context.FullUpdate()
        self.Refresh()

    # # Returns a list of all Elements beneath the given point
    def DetectCollision(self, pt):
        return self.__context.DetectCollision(pt)

    # # override to handle full-canvas scale
    def CalcUnscrolledPosition(self, position):
        x0, y0 = self.GetViewStart()[0] / self.__canvas_scale * self.scrollrate, \
                        self.GetViewStart()[1] / self.__canvas_scale * self.scrollrate
        return x0 + position[0] / self.__canvas_scale, y0 + position[1] / self.__canvas_scale

    # # Calculate the canvas size based on all elements
    #  @return True if size changed, False if not
    def __CalcCanvasSize(self):

        l, t, r, b = self.__context.GetElementExtents()

        width = max(self.MIN_WIDTH, r + self.__AUTO_CANVAS_SIZE_MARGIN)
        height = max(self.MIN_HEIGHT, b + self.__AUTO_CANVAS_SIZE_MARGIN)

        if self.__WIDTH == width and self.__HEIGHT == height:
            return False # No change

        self.__WIDTH = width
        self.__HEIGHT = height
        return True

    def __GetScrollBounds(self):
        sr = float(self.scrollrate)
        return self.__WIDTH * self.__canvas_scale / sr, self.__HEIGHT * self.__canvas_scale / sr

    # # Update the scrollbars based on a new canvas size
    #  Restores prior scroll offsets
    #  Uses instance attributes __SCROLL_RATE, __WIDTH, __HEIGHT
    def __UpdateScrollbars(self):
        sr = self.scrollrate

        w_pix, h_pix = self.GetClientSize()
        x_bound, y_bound = self.__GetScrollBounds()
        x, y = self.__scroll_ratios
        percent_bar_x = w_pix / (1.0 * self.__WIDTH * self.__canvas_scale)
        percent_bar_y = h_pix / (1.0 * self.__HEIGHT * self.__canvas_scale)
        if percent_bar_x > 1:
            percent_bar_x = 1
        if percent_bar_y > 1:
            percent_bar_y = 1

        self.SetScrollbars(sr,
                           sr,
                           x_bound,
                           y_bound,
                           x * x_bound * (1 - percent_bar_x),
                           y * y_bound * (1 - percent_bar_y),
                           True)

    # # Regenerates the gridlines based on __WIDTH and __HEIGHT
    #  @param gridsize Space between each gridline
    def __UpdateGrid(self, gridsize):
        self.__gridlines = []
        for x in range(round(self.__WIDTH / gridsize)):
            self.__gridlines.append((x * gridsize, 0, x * gridsize, self.__HEIGHT))
        for y in range(round(self.__HEIGHT / gridsize)):
            self.__gridlines.append((0, y * gridsize, self.__WIDTH, y * gridsize))
        assert gridsize % 2 == 0
        self.__snap_capture_delta = 7
        assert self.__snap_capture_delta <= gridsize / 2
