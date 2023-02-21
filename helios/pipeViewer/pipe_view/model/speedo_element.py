from __future__ import annotations
from .widget_element import WidgetElement
from . import element_propsvalid as valid

import wx
import math

import wx.lib.agw.speedmeter as SM

from typing import (Any,
                    Callable,
                    List,
                    Optional,
                    Tuple,
                    Union,
                    TYPE_CHECKING,
                    cast)

if TYPE_CHECKING:
    from .element import (PropertyValue,
                          ValidatedPropertyDict)
    from .element_value import Element_Value
    from ..gui.layout_canvas import Layout_Canvas


class SpeedoWidget(wx.Control):
    _DEFAULT_TEXT_FORMAT = '{f}'

    def __init__(self, parent: wx.Panel, *args: Any, **kwargs: Any) -> None:
        wx.Control.__init__(self, parent, *args, **kwargs)
        self.__parent = parent
        self.__panel = wx.Panel(self.__parent)
        self.__speedo_panel = wx.Panel(self.__panel)
        self.__speedo_panel.SetPosition((0, 0))
        self.__speedo = SM.SpeedMeter(self.__speedo_panel,
                                      agwStyle=(SM.SM_ROTATE_TEXT |
                                                SM.SM_DRAW_HAND |
                                                SM.SM_DRAW_MIDDLE_TEXT |
                                                SM.SM_DRAW_SECONDARY_TICKS))
        self.__speed_text_panel = wx.Panel(self.__panel)
        self.__speed_text = wx.StaticText(self.__speed_text_panel,
                                          style=wx.ALIGN_CENTER)
        self.__speed_text_format = SpeedoWidget._DEFAULT_TEXT_FORMAT
        self.__speed_text.Raise()

    def Bind(self,  # type: ignore[override]
             event: wx.PyEventBinder,
             func: Callable) -> None:
        wx.Control.Bind(self, event, func)
        self.__speedo.Bind(event, func)
        self.__speed_text.Bind(event, func)

    def SetPosition(self, pos: Union[Tuple[int, int], wx.Point]) -> None:
        self.__panel.SetPosition(pos)

    def SetSize(  # type: ignore[override]
        self,
        size: Union[Tuple[int, int], wx.Size]
    ) -> None:
        self.__panel.SetSize(size)
        self.__speedo_panel.SetSize(size)
        self.__speedo.SetSize(size)

    def SetBackgroundColor(
        self,
        color: Union[Tuple[int, int, int], wx.Colour]
    ) -> None:
        self.__panel.SetBackgroundColour(color)
        self.__speedo_panel.SetBackgroundColour(color)
        self.__speed_text_panel.SetBackgroundColour(color)
        self.__speed_text.SetBackgroundColour(color)
        self.__speedo.SetSpeedBackground(color)

    def Update(self) -> None:
        self.__speedo_panel.Fit()
        self.__speed_text_panel.Fit()
        self.__panel.Fit()
        self.__panel.Update()

    def SetValue(self, val: float) -> None:
        self.__speed_text.SetLabel(self.__speed_text_format.format(val))
        self.__speedo.SetSpeedValue(val)

    def SetAngleRange(self, start: float, end: float) -> None:
        self.__speedo.SetAngleRange(start, end)

    def SetIntervals(self, intervals: List[int]) -> None:
        self.__speedo.SetIntervals(intervals)

    def SetTicks(self, ticks: List[str]) -> None:
        self.__speedo.SetTicks(ticks)

    def SetTicksColor(self,
                      color: Union[Tuple[int, int, int], wx.Colour]) -> None:
        self.__speedo.SetTicksColour(color)

    def SetNumberOfSecondaryTicks(self, num: int) -> None:
        self.__speedo.SetNumberOfSecondaryTicks(num)

    def SetTicksFont(self, font: wx.Font) -> None:
        self.__speedo.SetTicksFont(font)

    def SetMiddleText(self, text: str) -> None:
        self.__speedo.SetMiddleText(text)

    def SetMiddleTextColor(
        self,
        color: Union[Tuple[int, int, int], wx.Colour]
    ) -> None:
        self.__speedo.SetMiddleTextColour(color)

    def SetMiddleTextFont(self, font: wx.Font) -> None:
        self.__speedo.SetMiddleTextFont(font)

    def SetHandColor(self,
                     color: Union[Tuple[int, int, int], wx.Colour]) -> None:
        self.__speedo.SetHandColour(color)

    def DrawExternalArc(self, draw: bool) -> None:
        self.__speedo.DrawExternalArc(draw)

    def ShowTextValue(self, show: bool) -> None:
        self.__speed_text_panel.Show(show)

    def SetTextValueFormat(self, fmt: str) -> None:
        self.__speed_text_format = fmt

    def SetTextValueFont(self, font: wx.Font) -> None:
        self.__speed_text.SetFont(font)

    def SetTextValueColor(
        self,
        color: Union[Tuple[int, int, int], wx.Colour]
    ) -> None:
        self.__speed_text.SetForegroundColour(color)

    def SetTextValuePosition(self,
                             pos: Union[Tuple[int, int], wx.Point]) -> None:
        self.__speed_text_panel.SetPosition(pos)


class SpeedoElement(WidgetElement):
    _SPEEDO_PROPERTIES: ValidatedPropertyDict = {
        'min_val': (0, valid.validateOffset),
        'max_val': (100, valid.validateOffset),
        'num_intervals': (20, valid.validateOffset),
        'num_secondary_ticks': (5, valid.validateOffset),
        'center_text': ('', valid.validateString),
        'start_angle': (-math.pi/6, valid.validateTimeScale),
        'end_angle': (7*math.pi/6, valid.validateTimeScale),
        'tick_color': ((0, 0, 0), valid.validateColor),
        'bg_color': ((255, 255, 255), valid.validateColor),
        'error_color': ((255, 0, 0), valid.validateColor),
        'text_color': ((0, 0, 0), valid.validateColor),
        'hand_color': ((255, 0, 0), valid.validateColor),
        'tick_font_size': (12, valid.validateOffset),
        'center_font_size': (12, valid.validateOffset),
        'draw_external_arc': (False, valid.validateBool),
        'speedo_delay': (0, valid.validateOffset),
        'show_text_value': (False, valid.validateBool),
        'text_value_format': ('{:03.2f}', valid.validateString),
        'text_value_font_size': (12, valid.validateOffset),
        'text_value_color': ((0, 0, 0), valid.validateColor),
        'text_value_position': ((50, 90), valid.validatePos),
    }

    _ALL_PROPERTIES = WidgetElement._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_SPEEDO_PROPERTIES)

    _DEFAULT_VALUE = 0
    _DEFAULT_DIMENSIONS = (100, 100)

    @staticmethod
    def GetType() -> str:
        return 'speedo'

    @staticmethod
    def GetDrawRoutine() -> Callable:
        return SpeedoElement.DrawRoutine

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        WidgetElement.__init__(self, self.__InitSpeedo, *args, **kwargs)
        self.__start_tick: Optional[int] = None
        self.__tick_delay: Optional[int] = None

    def __GetWidget(self) -> SpeedoWidget:
        widget = self._GetWidget()
        assert widget is not None
        return cast(SpeedoWidget, widget)

    def __SetSpeedoBackgroundColor(self, color: Tuple[int, int, int]) -> None:
        widget = self.__GetWidget()
        widget.SetBackgroundColor(color)

    def __UpdateSpeedoBackgroundColor(self) -> None:
        self.__SetSpeedoBackgroundColor(
            cast(Tuple[int, int, int], self.GetProperty('bg_color'))
        )

    def __SetSpeedoError(self, error: bool) -> None:
        if error:
            self.__SetSpeedoBackgroundColor(
                cast(Tuple[int, int, int], self.GetProperty('error_color'))
            )
        else:
            self.__UpdateSpeedoBackgroundColor()

    def DrawRoutine(self,
                    pair: Element_Value,
                    dc: wx.DC,
                    canvas: Layout_Canvas,
                    tick: int) -> None:
        WidgetElement.DrawRoutine(self, pair, dc, canvas, tick)

        if self.__start_tick is None:
            self.__start_tick = canvas.context.dbhandle.api.getFileStart()
        if self.__tick_delay is None:
            self.__tick_delay = cast(int, self.GetProperty('speedo_delay'))
            self.__tick_delay *= pair.GetClockPeriod()

        offset_tick = tick - self.__start_tick
        if offset_tick < self.__tick_delay:
            value_scale = float(offset_tick) / self.__tick_delay
        else:
            value_scale = 1.0

        val = None
        try:
            self.__SetSpeedoError(False)
            val = float(pair.GetVal())
        except ValueError:
            self.__SetSpeedoError(True)
            val = SpeedoElement._DEFAULT_VALUE
        val = value_scale * val
        widget = self.__GetWidget()
        widget.SetValue(val)
        widget.Update()

    def SetProperty(self, key: str, val: PropertyValue) -> None:
        WidgetElement.SetProperty(self, key, val)
        if key == 'speedo_delay':
            self.__tick_delay = None
        if self._GetWidget() is not None:
            self.__UpdateSpeedoProperties()

    def __UpdateSpeedoProperties(self) -> None:
        widget = self.__GetWidget()
        widget.SetAngleRange(cast(float, self.GetProperty('start_angle')),
                             cast(float, self.GetProperty('end_angle')))

        # Create the speedometer intervals
        min_val = cast(int, self.GetProperty('min_val'))
        max_val = cast(int, self.GetProperty('max_val'))
        num_intervals = cast(int, self.GetProperty('num_intervals'))
        interval_size = (max_val - min_val) // num_intervals
        intervals = list(range(min_val,
                               max_val + interval_size,
                               interval_size))
        widget.SetIntervals(intervals)

        self.__UpdateSpeedoBackgroundColor()

        # Create ticks
        ticks = [f'{interval}' for interval in intervals]
        widget.SetTicks(ticks)
        widget.SetTicksColor(
            cast(Tuple[int, int, int], self.GetProperty('tick_color'))
        )

        # Secondary ticks
        widget.SetNumberOfSecondaryTicks(
            cast(int, self.GetProperty('num_secondary_ticks'))
        )

        # Tick font
        widget.SetTicksFont(
            wx.Font(cast(int, self.GetProperty('tick_font_size')),
                    wx.SWISS,
                    wx.NORMAL,
                    wx.NORMAL)
        )

        # Center text
        widget.SetMiddleText(cast(str, self.GetProperty('center_text')))
        widget.SetMiddleTextColor(
            cast(Tuple[int, int, int], self.GetProperty('text_color'))
        )
        widget.SetMiddleTextFont(
            wx.Font(cast(int, self.GetProperty('center_font_size')),
                    wx.SWISS,
                    wx.NORMAL,
                    wx.BOLD)
        )

        # Hand color
        widget.SetHandColor(
            cast(Tuple[int, int, int], self.GetProperty('hand_color'))
        )

        # External arc
        widget.DrawExternalArc(
            cast(bool, self.GetProperty("draw_external_arc"))
        )

        widget.ShowTextValue(cast(bool, self.GetProperty("show_text_value")))
        widget.SetTextValueFormat(
            cast(str, self.GetProperty("text_value_format"))
        )
        widget.SetTextValueColor(
            cast(Tuple[int, int, int], self.GetProperty('text_value_color'))
        )
        widget.SetTextValuePosition(
            cast(Tuple[int, int], self.GetProperty('text_value_position'))
        )
        widget.SetTextValueFont(
            wx.Font(cast(int, self.GetProperty('text_value_font_size')),
                    wx.SWISS,
                    wx.NORMAL,
                    wx.BOLD)
        )

        # Set value
        widget.SetValue(SpeedoElement._DEFAULT_VALUE)

        canvas = self._GetCanvas()
        assert canvas is not None
        size = canvas.GetSize()
        widget.SetSize(size)
        widget.SetPosition((0, 0))

    def __InitSpeedo(self) -> None:
        canvas = self._GetCanvas()
        assert canvas is not None
        self._SetWidget(SpeedoWidget(canvas))
        self.__UpdateSpeedoProperties()


SpeedoElement._ALL_PROPERTIES['dimensions'] = \
    (SpeedoElement._DEFAULT_DIMENSIONS, valid.validateDim)
SpeedoElement._ALL_PROPERTIES['type'] = (SpeedoElement.GetType(),
                                         valid.validateString)
