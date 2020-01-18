from .element import LocationallyKeyedElement
from . import element_propsvalid as valid

import wx
#import math
#import re
#import sys

class WidgetElement(LocationallyKeyedElement):
    _ALL_PROPERTIES = LocationallyKeyedElement._ALL_PROPERTIES.copy()
    
    @staticmethod
    def GetType():
        return 'widget'

    @staticmethod
    def IsDrawable():
        return True

    @staticmethod
    def GetDrawRoutine():
        return WidgetElement.DrawRoutine

    def __init__(self, widget_init, *args, **kwargs):
        self.__canvas = None
        self.__parent = None
        self.__widget = None
        self.__widget_init = widget_init
        LocationallyKeyedElement.__init__(self, *args, **kwargs)

    def SetProperty(self, key, val):
        LocationallyKeyedElement.SetProperty(self, key, val)
        if self.__canvas is None:
            return
        val = self.GetProperty(key)
        if key == 'position':
            self.__SetCanvasPosition(val)
            self.__canvas.Update()
        elif key == 'dimensions':
            self.__SetCanvasSize(val)
            self.__canvas.Update()

    def __SetCanvasPosition(self, val):
        if self.__canvas is not None:
            self.__canvas.SetPosition(self.__parent.CalcScrolledPosition(val))

    def __SetCanvasSize(self, val):
        if self.__canvas is not None:
            self.__canvas.SetClientSize(val)
            self.__canvas.SetSize(val)

    def _GetCanvas(self):
        return self.__canvas

    def __ShowCanvas(self):
        self.__canvas.Show(True)

    def __HideCanvas(self):
        self.__canvas.Show(False)

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    tick):
        if self.__canvas is None:
            self.__CreateCanvas(canvas)
        if self.__widget is None:
            self.__InitializeWidget()
        self.__canvas.Update()

    def EnableDraw(self, enable):
        LocationallyKeyedElement.EnableDraw(self, enable)
        if not self.ShouldDraw():
            self.__HideCanvas()
            return
        else:
            self.__ShowCanvas()

    def __SkipEvent(self, event):
        wx.PostEvent(self.__parent, event)
        event.Skip()

    def __OnFocus(self, event):
        self.__parent.SetFocus()
        event.Skip()

    def _SetWidget(self, widget):
        self.__widget = widget

    def _GetWidget(self):
        return self.__widget

    def __InitializeWidget(self):
        assert(self.__widget_init is not None)
        self.__widget_init()
        assert(self.__widget is not None)
        self.__widget.Bind(wx.EVT_LEFT_DOWN, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_KEY_DOWN, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_KEY_UP, self.__SkipEvent)
        self.__widget.Bind(wx.EVT_SET_FOCUS, self.__OnFocus)

    def __CreateCanvas(self, parent):
        assert(self.__canvas is None)
        self.__parent = parent
        self.__canvas = wx.Panel(parent)
        self.__SetCanvasSize(wx.Size(self.GetXDim(), self.GetYDim()))
        self.__SetCanvasPosition(wx.Point(self.GetXPos(), self.GetYPos()))

WidgetElement._ALL_PROPERTIES['type'] = (WidgetElement.GetType(), valid.validateString)

