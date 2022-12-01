from __future__ import annotations
from .element import LocationallyKeyedElement
from . import element_propsvalid as valid

import wx
from typing import Any, Callable, Optional, Tuple, Union, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from model.element_value import Element_Value
    from model.element import PropertyValue
    from gui.layout_canvas import Layout_Canvas


class WidgetElement(LocationallyKeyedElement):
    _ALL_PROPERTIES = LocationallyKeyedElement._ALL_PROPERTIES.copy()

    @staticmethod
    def GetType() -> str:
        return 'widget'

    @staticmethod
    def IsDrawable() -> bool:
        return True

    @staticmethod
    def GetDrawRoutine() -> Callable:
        return WidgetElement.DrawRoutine

    def __init__(self,
                 widget_init: Callable,
                 *args: Any,
                 **kwargs: Any) -> None:
        self.__canvas: Optional[wx.Panel] = None
        self.__parent: Optional[Layout_Canvas] = None
        self.__widget: Optional[wx.Window] = None
        self.__widget_init = widget_init
        LocationallyKeyedElement.__init__(self, *args, **kwargs)

    def SetProperty(self, key: str, val: PropertyValue) -> None:
        LocationallyKeyedElement.SetProperty(self, key, val)
        if self.__canvas is None:
            return
        val = self.GetProperty(key)
        if key == 'position':
            val = cast(Tuple[int, int], val)
            self.__SetCanvasPosition(val)
            self.__canvas.Update()
        elif key == 'dimensions':
            val = cast(Tuple[int, int], val)
            self.__SetCanvasSize(val)
            self.__canvas.Update()

    def __SetCanvasPosition(self,
                            val: Union[Tuple[int, int], wx.Point]) -> None:
        if self.__canvas is not None:
            assert self.__parent is not None
            self.__canvas.SetPosition(self.__parent.CalcScrolledPosition(val))

    def __SetCanvasSize(self, val: Union[Tuple[int, int], wx.Size]) -> None:
        if self.__canvas is not None:
            self.__canvas.SetClientSize(val)
            self.__canvas.SetSize(val)

    def _GetCanvas(self) -> Optional[wx.Panel]:
        return self.__canvas

    def __ShowCanvas(self) -> None:
        assert self.__canvas is not None
        self.__canvas.Show(True)

    def __HideCanvas(self) -> None:
        assert self.__canvas is not None
        self.__canvas.Show(False)

    def DrawRoutine(self,
                    pair: Element_Value,
                    dc: wx.DC,
                    canvas: Layout_Canvas,
                    tick: int) -> None:
        if self.__canvas is None:
            self.__CreateCanvas(canvas)
        if self.__widget is None:
            self.__InitializeWidget()
        assert self.__canvas is not None
        self.__canvas.Update()

    def EnableDraw(self, enable: bool) -> None:
        LocationallyKeyedElement.EnableDraw(self, enable)
        if not self.ShouldDraw():
            self.__HideCanvas()
            return
        else:
            self.__ShowCanvas()

    def __SkipEvent(self, event: wx.Event) -> None:
        assert self.__parent is not None
        wx.PostEvent(self.__parent, event)
        event.Skip()

    def __OnFocus(self, event: wx.FocusEvent) -> None:
        assert self.__parent is not None
        self.__parent.SetFocus()
        event.Skip()

    def _SetWidget(self, widget: wx.Window) -> None:
        self.__widget = widget

    def _GetWidget(self) -> Optional[wx.Window]:
        return self.__widget

    def __InitializeWidget(self) -> None:
        assert(self.__widget_init is not None)
        self.__widget_init()
        assert(self.__widget is not None)
        self.__widget.Bind(wx.EVT_LEFT_DOWN, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_KEY_DOWN, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_KEY_UP, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_SET_FOCUS, self.__OnFocus)

    def __CreateCanvas(self, parent: Layout_Canvas) -> None:
        assert(self.__canvas is None)
        self.__parent = parent
        self.__canvas = wx.Panel(parent)
        self.__SetCanvasSize(wx.Size(self.GetXDim(), self.GetYDim()))
        self.__SetCanvasPosition(wx.Point(self.GetXPos(), self.GetYPos()))


WidgetElement._ALL_PROPERTIES['type'] = \
    (WidgetElement.GetType(), valid.validateString)
