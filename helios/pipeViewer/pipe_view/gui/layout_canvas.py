from __future__ import annotations
import wx
import wx.lib.dragscroller as drgscrlr
import math
import sys
import time
import logging
from typing import Any, Dict, List, Optional, Tuple, Union, cast, TYPE_CHECKING

from .selection_manager import Selection_Mgr
from .input_decoder import Input_Decoder
from .hover_preview import HoverPreview
from functools import partial
from . import autocoloring
from ..model import highlighting_utils
from .font_utils import GetMonospaceFont, GetDefaultFont

if TYPE_CHECKING:
    from .dialogs.element_propsdlg import Element_PropsDlg
    from .layout_frame import Layout_Frame
    from ..model.element import Element
    from ..model.element_value import Element_Value
    from ..model.layout_context import Layout_Context
    from ..model.settings import ArgosSettings

try:
    from .. import core
except ImportError as e:
    print('Argos failed to import module: "core". Argos requires Make',
          file=sys.stderr)
    print(f'Exception: {e}', file=sys.stderr)
    sys.exit(1)


ColoredTransactionTuple = \
    Tuple[str, wx.Brush, bool, bool, Optional[int], Optional[int]]


class Layout_Canvas(wx.ScrolledWindow):

    __AUTO_CANVAS_SIZE_MARGIN = 40  # Pixels

    MIN_WIDTH = 500
    MIN_HEIGHT = 500
    MIN_ZOOM = 0.1
    MAX_ZOOM = 2

    # Construct a Layout_Canvas
    #  @param parent Parent Layout_Frame
    #  @param context Associated Layout_Context
    #  @param dialog Element Properties Dialog for this canvas/frame
    #  @param ID Window ID
    def __init__(self,
                 parent: Layout_Frame,
                 context: Layout_Context,
                 dialog: Element_PropsDlg,
                 ID: int = -1,
                 pos: wx.Point = wx.DefaultPosition,
                 size: wx.Size = wx.DefaultSize,
                 style: int = wx.NO_FULL_REPAINT_ON_RESIZE):

        wx.ScrolledWindow.__init__(self, parent)
        self.SetBackgroundStyle(wx.BG_STYLE_PAINT)
        self.SetBackgroundColour(wx.WHITE)
        self.__renderer = core.Renderer()
        assert autocoloring.REASON_BRUSHES and autocoloring.BACKGROUND_BRUSHES
        self.SetRendererBrushes()
        self.__renderer.setExtensions(context.GetExtensionManager())
        self.__dragscrl = drgscrlr.DragScroller(self,
                                                rate=300,
                                                sensitivity=.01)
        self.__WIDTH = 2000
        self.__HEIGHT = 2000

        # full canvas scale
        self.__canvas_scale = 1.0
        self.__font_scale = (1.0, 1.0)

        self.__SCROLL_RATE = 20
        self.__scroll_ratios = (0.0, 0.0)
        self.__UpdateScrollbars()
        self.__parent = parent
        self.__context = context
        self.__layout = context.GetLayout()
        self.__dlg = dialog
        self.__draw_grid = False
        # TODO, make neat ####################################
        self.__gridsize = 14
        self.__gridlines: List[Tuple[int, int, int, int]] = []
        self.__UpdateGrid(self.__gridsize)
        # End TODO ###########################################
        self.__selection = Selection_Mgr(self, dialog)
        self.__user_handler = Input_Decoder(self)

        self.__set_renderer_font = False
        self.__schedule_line_draw_style = 4  # classic

        # used for color highlighting of transactions
        self.__colored_transactions: Dict[str, ColoredTransactionTuple] = {}

        self.__hover_preview = HoverPreview(self, context)
        # Load images

        self.__fnt_layout = GetMonospaceFont(
            self.GetSettings().layout_font_size
        )
        self.__UpdateFontScaling()

        # Disable background erasing
        def disable_event(*pargs: Any, **kwargs: Any) -> None:
            pass

        self.Bind(wx.EVT_ERASE_BACKGROUND, disable_event)
        self.Bind(wx.EVT_PAINT, self.OnPaint)
        self.Bind(wx.EVT_SIZE, self.OnSize)

        # Bindings for mouse events
        self.Bind(wx.EVT_LEFT_DOWN,
                  partial(self.__user_handler.LeftDown,
                          canvas=self,
                          selection=self.__selection,
                          dialog=self.__dlg))
        self.Bind(wx.EVT_MOTION,
                  partial(self.__user_handler.MouseMove,
                          canvas=self,
                          selection=self.__selection,
                          context=self.__context,
                          mouse_over_preview=self.__hover_preview))
        self.Bind(wx.EVT_LEFT_DCLICK,
                  partial(self.__user_handler.LeftDouble,
                          canvas=self,
                          selection=self.__selection,
                          dialog=self.__dlg))
        self.Bind(wx.EVT_LEFT_UP,
                  partial(self.__user_handler.LeftUp,
                          canvas=self,
                          selection=self.__selection,
                          dialog=self.__dlg))
        self.Bind(wx.EVT_RIGHT_DOWN,
                  partial(self.__user_handler.RightDown,
                          canvas=self,
                          drgscrl=self.__dragscrl))
        self.Bind(wx.EVT_RIGHT_UP,
                  partial(self.__user_handler.RightUp,
                          canvas=self,
                          drgscrl=self.__dragscrl))
        # Bindings for key events
        self.Bind(wx.EVT_KEY_DOWN,
                  partial(self.__user_handler.KeyDown,
                          canvas=self,
                          selection=self.__selection,
                          dialog=self.__dlg,
                          context=self.__context))
        self.Bind(wx.EVT_KEY_UP,
                  partial(self.__user_handler.KeyUp,
                          canvas=self,
                          selection=self.__selection,
                          dialog=self.__dlg,
                          context=self.__context))

        self.Bind(wx.EVT_MOUSEWHEEL,
                  partial(self.__user_handler.MouseWheel, canvas=self))

        # required to actually receive key events
        self.SetFocus()

        self.Bind(wx.EVT_SCROLLWIN, self.ScrollWin)

    # Size of the canvas grid
    @property
    def gridsize(self) -> int:
        return self.__gridsize

    # Snap sensitivity range (+/- each gridline)
    @property
    def range(self) -> int:
        return self.__snap_capture_delta

    @property
    def scrollrate(self) -> int:
        return self.__SCROLL_RATE

    @property
    def context(self) -> Layout_Context:
        return self.__context

    # Returns the parent frame which owns this Canvas
    def GetFrame(self) -> Layout_Frame:
        return self.__parent

    # Returns a tuple showing the area of the canvas visible to the screen
    #  @return (x,y,w,h)
    def GetVisibleArea(self) -> Tuple[int, int, int, int]:
        x, y = self.GetViewStart()
        w, h = self.GetClientSize()
        return (x, y, w, h)

    # Updates color for a transaction
    def UpdateTransactionColor(self,
                               el: Element,
                               annotation: str) -> Tuple[str, wx.Brush]:
        if el.HasProperty('auto_color_basis') \
          and el.HasProperty('color_basis_type'):
            auto_color_basis = cast(str, el.GetProperty('auto_color_basis'))
            color_basis_type = cast(str, el.GetProperty('color_basis_type'))
            content_type = cast(str, el.GetProperty('Content'))
            record = self.GetTransactionColor(annotation,
                                              content_type,
                                              color_basis_type,
                                              auto_color_basis)
            if record:
                string_to_display, brush, _, _, _, _ = record
            else:
                string_to_display, brush, _, _ = self.AddColoredTransaction(
                    annotation,
                    content_type,
                    color_basis_type,
                    auto_color_basis,
                    0,
                    el
                )
        else:
            string_to_display = self.__hover_preview.annotation
            brush = wx.Brush((200, 200, 200))

        return string_to_display, brush

    # Here the canvas does all its drawing
    def DoDraw(self, dc: wx.DC) -> None:
        logging.debug('xxx: Started C drawing')

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
                dc.DrawLine(int(x - xoff),
                            int(y - yoff),
                            int(x1 - xoff),
                            int(y1 - yoff))

        t_start = time.monotonic()
        self.__renderer.drawElements(dc, self, self.__context.GetHC())

        logging.debug('%ss: C drawing', time.monotonic() - t_start)

    def ScrollWin(self, evt: wx.ScrollWinEvent) -> None:
        super().Refresh()

        bounds = self.__GetScrollBounds()
        x_bound = bounds[0] - self.scrollrate
        y_bound = bounds[1] - self.scrollrate

        w_pix, h_pix = self.GetClientSize()
        percent_bar_x = w_pix / self.__WIDTH
        percent_bar_y = h_pix / self.__HEIGHT

        self.__scroll_ratios = (
            self.GetScrollPos(wx.HORIZONTAL) / x_bound + percent_bar_x / 4.0,
            self.GetScrollPos(wx.VERTICAL) / y_bound + percent_bar_y / 4.0
        )

        # If we're in edit mode, update the cursor location in the toolbar
        if self.__user_handler.GetEditMode():
            (x, y) = self.GetMousePosition()
            offset = self.CalcScrollInc(evt)
            scroll_units = self.GetScrollPixelsPerUnit()

            if evt.GetOrientation() == wx.HORIZONTAL:
                x += offset * scroll_units[0]
            elif evt.GetOrientation() == wx.VERTICAL:
                y += offset * scroll_units[1]

            self.__parent.UpdateMouseLocation((x, y))

        mousePos = wx.GetMousePosition()
        self.__hover_preview.HandleMouseMove(mousePos, self)
        evt.Skip()

    # Returns the position of the mouse within the canvas
    def GetMousePosition(self) -> Tuple[int, int]:
        return self.CalcUnscrolledPosition(
            self.ScreenToClient(wx.GetMousePosition())
        )

    # Execute all drawing logic
    def FullUpdate(self) -> None:

        # update quad tree (if needed)
        self.__context.MicroUpdate()
        self.Refresh()
        # Tell the Element Prop's Dlg window to update itself
        self.__dlg.Refresh()

    # Execute all drawing logic
    def OnPaint(self, event: wx.PaintEvent) -> None:
        paint_dc = wx.AutoBufferedPaintDC(self)
        paint_dc.Clear()
        context = wx.GraphicsContext.Create(paint_dc)
        dc = wx.GCDC(context)
        dc.SetLogicalScale(self.__canvas_scale, self.__canvas_scale)
        dc.SetFont(self.__fnt_layout)
        if not self.__set_renderer_font:
            self.__renderer.setFontFromDC(dc)
            self.__set_renderer_font = True
        self.DoDraw(dc)
        self.__selection.Draw(dc)

    # Returns Hover Preview
    def GetHoverPreview(self) -> HoverPreview:
        return self.__hover_preview

    # Turns on or off the drawing of the grid
    def ToggleGrid(self) -> None:
        self.__draw_grid = not self.__draw_grid
        self.Refresh()

    # Refresh. Recalc size and forward to superclass
    def Refresh(
        self,
        eraseBackground: bool = True,
        rect: Optional[Union[Tuple[int, int, int, int], wx.Rect]] = None
    ) -> None:
        # Recalculate canvas size
        if self.__CalcCanvasSize():
            self.__UpdateScrollbars()
            self.__UpdateGrid(self.__gridsize)

        super().Refresh()

    # Gets called when the window is resized and when the canvas is initialized
    def OnSize(self, event: wx.SizeEvent) -> None:
        # update the screen
        self.FullUpdate()

    # Gets the selection manager for this canvas
    def GetSelectionManager(self) -> Selection_Mgr:
        return self.__selection

    # Gets the input decoder for this canvas
    def GetInputDecoder(self) -> Input_Decoder:
        return self.__user_handler

    # Forward on the draw-orderd list of all Elements in this
    #  frame/canvas/layout...
    def GetElements(self) -> List[Element]:
        return self.__context.GetElements()

    # Return Element/Value pairs
    def GetElementPairs(self) -> List[Element_Value]:
        return self.__context.GetElementPairs()

    # Compute and returns display bounds
    def GetBounds(self) -> Tuple[int, int, int, int]:
        posx, posy, width, height = self.GetVisibleArea()
        mins = self.CalcUnscrolledPosition((0, 0))
        bounds = (mins[0],
                  mins[1],
                  int(mins[0] + width / self.__canvas_scale),
                  int(mins[1] + height / self.__canvas_scale))
        return bounds

    # Gets the update region relative to the current visible area
    # Coordinates are scaled to account for current zoom factor
    # Used by schedule elements to determine where to blit
    def GetScaledUpdateRegion(self) -> wx.Rect:
        box = self.GetUpdateRegion().GetBox()
        box = wx.Rect(int(math.floor(box[0] / self.__canvas_scale)),
                      int(math.floor(box[1] / self.__canvas_scale)),
                      int(math.ceil(box[2] / self.__canvas_scale)),
                      int(math.ceil(box[3] / self.__canvas_scale)))

        box.Inflate(10, 10)  # Fudge factor to ensure no dirt is left behind
        return box

    # Gets the update region relative to the origin of the scrolled area
    # Region is scaled to account for current zoom factor
    # Used to determine which elements need to be updated
    def GetScrolledUpdateRegion(self) -> wx.Rect:
        box = self.GetUpdateRegion().GetBox()
        box.SetPosition(self.CalcUnscrolledPosition(box.GetTopLeft()))
        box.SetHeight(int(math.ceil(box.GetHeight() / self.__canvas_scale)))
        box.SetWidth(int(math.ceil(box.GetWidth() / self.__canvas_scale)))

        box.Inflate(10, 10)  # Fudge factor to ensure no dirt is left behind
        return box

    # Returns element pairs suitable for drawing
    def GetDrawPairs(self) -> List[Element_Value]:
        box = self.GetScrolledUpdateRegion()
        top_left = box.GetTopLeft()
        bottom_right = box.GetBottomRight()
        bounds = (top_left[0], top_left[1], bottom_right[0], bottom_right[1])
        return self.__context.GetDrawPairs(bounds)

    def GetVisibilityTick(self) -> int:
        return self.__context.GetVisibilityTick()

    # Set the brushes in the renderer to the current brush set
    def SetRendererBrushes(self) -> None:
        self.__renderer.setBrushes(autocoloring.REASON_BRUSHES.as_dict(),
                                   autocoloring.BACKGROUND_BRUSHES.as_dict())

    # Resets the renderer brushes and purges the brush cache
    # Needed whenever the palette or color shuffle mode changes
    def RefreshBrushes(self) -> None:
        self.SetRendererBrushes()
        self.PurgeBrushCache()

    # Returns brushes needed to draw elements
    def GetBrushes(self) -> Tuple[autocoloring.BrushRepository,
                                  autocoloring.BrushRepository]:
        return autocoloring.BACKGROUND_BRUSHES, autocoloring.REASON_BRUSHES

    # Returns offsets needed to render
    def GetRenderOffsets(self) -> Tuple[int, int]:
        return self.CalcUnscrolledPosition((0, 0))

    # Returns the Cython renderer
    def GetRenderer(self) -> core.Renderer:
        return self.__renderer

    # Returns element settings dialog
    def GetDialog(self) -> Element_PropsDlg:
        return self.__dlg

    # Returns global setting for schedule line drawing
    def GetScheduleLineStyle(self) -> int:
        return self.__schedule_line_draw_style

    def GetScale(self) -> float:
        return self.__canvas_scale

    # Updates the highlighting state of all cached transactions
    def UpdateTransactionHighlighting(self) -> None:
        for key, val in self.__colored_transactions.items():
            self.__colored_transactions[key] = \
                val[:2] + \
                (self.__context.IsUopUidHighlighted(val[4]),) + \
                (self.__context.IsSearchResult(val[5]),) + \
                val[4:]

    @staticmethod
    def __GetTransactionKey(
        annotation: str,
        content_type: str,
        color_basis_type: str,
        auto_color_basis: str
    ) -> str:
        return f'{annotation}:{color_basis_type}:{auto_color_basis}:{hash(content_type)}'  # noqa: E501

    def GetTransactionColor(
        self,
        annotation: str,
        content_type: str,
        color_basis_type: str,
        auto_color_basis: str
    ) -> Optional[ColoredTransactionTuple]:
        return self.__colored_transactions.get(
            self.__GetTransactionKey(annotation,
                                     content_type,
                                     color_basis_type,
                                     auto_color_basis)
        )

    # Track the tagging of one transaction
    #  @note OThis can be called mutiple times to override existing colors
    def AddColoredTransaction(
        self,
        annotation: str,
        content_type: str,
        color_basis_type: str,
        auto_color_basis: str,
        start_tick: int,
        element: Element
    ) -> Tuple[str, wx.Brush, bool, bool]:
        if len(self.__colored_transactions) > 10000:
            self.__colored_transactions.clear()

        string_to_display, brush = self.__renderer.parseAnnotationAndGetColor(
            annotation,
            content_type,
            color_basis_type,
            auto_color_basis
        )

        key = self.__GetTransactionKey(annotation,
                                       content_type,
                                       color_basis_type,
                                       auto_color_basis)

        uop_uid = highlighting_utils.GetUopUid(annotation)
        highlighted = self.__context.IsUopUidHighlighted(uop_uid)

        if element.HasProperty('LocationString'):
            search_result_hash = self.__context.SearchResultHash(
                start_tick,
                self.__context.GetLocationId(element)
            )
        else:
            search_result_hash = None

        search_result = self.__context.IsSearchResult(search_result_hash)
        self.__colored_transactions[key] = (string_to_display,
                                            brush,
                                            highlighted,
                                            search_result,
                                            uop_uid,
                                            search_result_hash)
        return string_to_display, brush, highlighted, search_result

    # Gets old-style coloring (no basis configured)
    def GetAutocolorColor(self, annotation: str) -> wx.Colour:
        string_to_display, brush = self.__renderer.parseAnnotationAndGetColor(
            annotation,
            'auto_color_annotation'
        )
        return brush.GetColour()

    # remove all entries from the dictionary of colored transactions
    # will auto regenerate next render frame
    def PurgeBrushCache(self) -> None:
        self.__colored_transactions.clear()
        self.__context.FullRedraw()
        self.Refresh()

    # Set the global style for schedule lines
    def SetScheduleLineStyle(self, style: int) -> None:
        if style != self.__schedule_line_draw_style:
            self.__schedule_line_draw_style = style
            self.__context.FullUpdate()
            self.Refresh()

    def SetScale(self, scale: float) -> None:
        # cap at 2x zoom in
        self.__canvas_scale = min(max(self.MIN_ZOOM, scale), self.MAX_ZOOM)

        self.__CalcCanvasSize()
        self.__UpdateScrollbars()
        self.__UpdateGrid(self.__gridsize)

        super().Refresh()

    # Returns a list of all Elements beneath the given point
    def DetectCollision(
        self,
        pt: Union[Tuple[int, int], wx.Point]
    ) -> List[Element_Value]:
        return self.__context.DetectCollision(pt)

    # override to handle full-canvas scale
    def CalcUnscrolledPosition(
        self,
        position: Union[wx.Point, Tuple[int, int]]
    ) -> Tuple[int, int]:
        view_start = self.GetViewStart()
        x0 = view_start[0] * self.scrollrate
        y0 = view_start[1] * self.scrollrate
        return (int((x0 + position[0]) / self.__canvas_scale),
                int((y0 + position[1]) / self.__canvas_scale))

    # Calculate the canvas size based on all elements
    #  @return True if size changed, False if not
    def __CalcCanvasSize(self) -> bool:

        l, t, r, b = self.__context.GetElementExtents()
        r = int(r * self.__canvas_scale)
        b = int(b * self.__canvas_scale)

        width = max(self.MIN_WIDTH, r + self.__AUTO_CANVAS_SIZE_MARGIN)
        height = max(self.MIN_HEIGHT, b + self.__AUTO_CANVAS_SIZE_MARGIN)

        if self.__WIDTH == width and self.__HEIGHT == height:
            return False  # No change

        self.__WIDTH = width
        self.__HEIGHT = height
        return True

    def __GetScrollBounds(self) -> Tuple[float, float]:
        sr = float(self.scrollrate)
        return self.__WIDTH / sr, self.__HEIGHT / sr

    # Update the scrollbars based on a new canvas size
    #  Restores prior scroll offsets
    #  Uses instance attributes __SCROLL_RATE, __WIDTH, __HEIGHT
    def __UpdateScrollbars(self,
                           force_x: Optional[int] = None,
                           force_y: Optional[int] = None) -> None:
        sr = self.scrollrate

        w_pix, h_pix = self.GetClientSize()
        x_bound, y_bound = self.__GetScrollBounds()
        x, y = self.__scroll_ratios
        percent_bar_x = w_pix / self.__WIDTH
        percent_bar_y = h_pix / self.__HEIGHT
        if percent_bar_x > 1:
            percent_bar_x = 1
        if percent_bar_y > 1:
            percent_bar_y = 1

        if force_x is not None:
            x_scroll = force_x
        else:
            x_scroll = int(x * x_bound * (1 - percent_bar_x))

        if force_y is not None:
            y_scroll = force_y
        else:
            y_scroll = int(y * y_bound * (1 - percent_bar_y))

        self.SetScrollbars(sr,
                           sr,
                           int(x_bound),
                           int(y_bound),
                           x_scroll,
                           y_scroll,
                           True)

    # Regenerates the gridlines based on __WIDTH and __HEIGHT
    #  @param gridsize Space between each gridline
    def __UpdateGrid(self, gridsize: int) -> None:
        assert gridsize % 2 == 0
        self.__snap_capture_delta = 7
        assert self.__snap_capture_delta <= gridsize / 2
        self.__gridlines = []
        for x in range(round(self.__WIDTH / gridsize)):
            grid_x = x * gridsize
            self.__gridlines.append((grid_x, 0, grid_x, self.__HEIGHT))
        for y in range(round(self.__HEIGHT / gridsize)):
            grid_y = y * gridsize
            self.__gridlines.append((0, grid_y, self.__WIDTH, grid_y))

    def GetSettings(self) -> ArgosSettings:
        return self.__parent.GetSettings()

    def __UpdateFontScaling(self) -> None:
        default_font_w, default_font_h = GetDefaultFont().GetPixelSize()
        cur_font_w, cur_font_h = self.__fnt_layout.GetPixelSize()
        self.__font_scale = (cur_font_w / default_font_w,
                             cur_font_h / default_font_h)
        for e in self.__context.GetElementPairs():
            e.GetElement().SetProperty('scale_factor', self.__font_scale)
        self.__context.UpdateElementExtents()

    def UpdateFontSize(self) -> None:
        old_font = self.__fnt_layout
        self.__fnt_layout = GetMonospaceFont(
            self.GetSettings().layout_font_size
        )
        if old_font.GetPointSize() != self.__fnt_layout.GetPointSize():
            self.__set_renderer_font = False
            self.__UpdateFontScaling()
            self.__CalcCanvasSize()
            self.__UpdateScrollbars(force_x=0, force_y=0)
            self.__context.FullRedraw()
            self.FullUpdate()
