from .widget_element import WidgetElement
from . import element_propsvalid as valid

import wx
import math
import re
import sys

import wx.lib.agw.speedmeter as SM

class SpeedoWidget(wx.Control):
    _DEFAULT_TEXT_FORMAT = '{f}'

    def __init__(self, parent, *args, **kwargs):
        wx.Control.__init__(self, parent, *args, **kwargs)
        self.__parent = parent
        self.__panel = wx.Panel(self.__parent)
        self.__speedo_panel = wx.Panel(self.__panel)
        self.__speedo_panel.SetPosition((0, 0))
        self.__speedo = SM.SpeedMeter(self.__speedo_panel, agwStyle=SM.SM_ROTATE_TEXT|SM.SM_DRAW_HAND|SM.SM_DRAW_MIDDLE_TEXT|SM.SM_DRAW_SECONDARY_TICKS)
        self.__speed_text_panel = wx.Panel(self.__panel)
        self.__speed_text = wx.StaticText(self.__speed_text_panel, style=wx.ALIGN_CENTER)
        self.__speed_text_format = SpeedoWidget._DEFAULT_TEXT_FORMAT
        self.__speed_text.Raise()
        #self.__speedo.Hide()

    def Bind(self, event, func):
        wx.Control.Bind(self, event, func)
        self.__speedo.Bind(event, func)
        self.__speed_text.Bind(event, func)

    def SetPosition(self, pos):
        self.__panel.SetPosition(pos)

    def SetSize(self, size):
        self.__panel.SetSize(size)
        self.__speedo_panel.SetSize(size)
        self.__speedo.dim = size
        self.__speedo.SetSize(size)

    def SetBackgroundColor(self, color):
        self.__panel.SetBackgroundColour(color)
        self.__speedo_panel.SetBackgroundColour(color)
        self.__speed_text_panel.SetBackgroundColour(color)
        self.__speed_text.SetBackgroundColour(color)
        self.__speedo.SetSpeedBackground(color)

    def Update(self):
        self.__speedo_panel.Fit()
        self.__speed_text_panel.Fit()
        self.__panel.Fit()
        self.__panel.Update()
    #    self.__speedo.OnPaint(None)
    #    self.__speed_text.Update()
    #    self.__speed_text.Raise()

    def SetValue(self, val):
        self.__speed_text.SetLabel(self.__speed_text_format.format(val))
        self.__speedo.SetSpeedValue(val)

    def SetAngleRange(self, start, end):
        self.__speedo.SetAngleRange(start, end)

    def SetIntervals(self, intervals):
        self.__speedo.SetIntervals(intervals)

    def SetTicks(self, ticks):
        self.__speedo.SetTicks(ticks)

    def SetTicksColor(self, color):
        self.__speedo.SetTicksColour(color)

    def SetNumberOfSecondaryTicks(self, num):
        self.__speedo.SetNumberOfSecondaryTicks(num)

    def SetTicksFont(self, font):
        self.__speedo.SetTicksFont(font)

    def SetMiddleText(self, text):
        self.__speedo.SetMiddleText(text)

    def SetMiddleTextColor(self, color):
        self.__speedo.SetMiddleTextColour(color)

    def SetMiddleTextFont(self, font):
        self.__speedo.SetMiddleTextFont(font)

    def SetHandColor(self, color):
        self.__speedo.SetHandColour(color)

    def DrawExternalArc(self, draw):
        self.__speedo.DrawExternalArc(draw)

    def ShowTextValue(self, show):
        self.__speed_text_panel.Show(show)

    def SetTextValueFormat(self, fmt):
        self.__speed_text_format = fmt

    def SetTextValueFont(self, font):
        self.__speed_text.SetFont(font)

    def SetTextValueColor(self, color):
        self.__speed_text.SetForegroundColour(color)

    def SetTextValuePosition(self, pos):
        self.__speed_text_panel.SetPosition(pos)

class SpeedoElement(WidgetElement):
    _SPEEDO_PROPERTIES = {
                            'min_val' : (0 , valid.validateOffset),
                            'max_val' : (100 , valid.validateOffset),
                            'num_intervals' : (20, valid.validateOffset),
                            'num_secondary_ticks' : (5, valid.validateOffset),
                            'center_text' : ('', valid.validateString),
                            'start_angle' : (-math.pi/6, valid.validateTimeScale),
                            'end_angle' : (7*math.pi/6, valid.validateTimeScale),
                            'tick_color' : ((0, 0, 0), valid.validateColor),
                            'bg_color' : ((255, 255, 255), valid.validateColor),
                            'error_color' : ((255, 0, 0), valid.validateColor),
                            'text_color' : ((0, 0, 0), valid.validateColor),
                            'hand_color' : ((255, 0, 0), valid.validateColor),
                            'tick_font_size' : (12, valid.validateOffset),
                            'center_font_size' : (12, valid.validateOffset),
                            'draw_external_arc' : (False, valid.validateBool),
                            'speedo_delay' : (0, valid.validateOffset),
                            'show_text_value' : (False, valid.validateBool),
                            'text_value_format' : ('{:03.2f}', valid.validateString),
                            'text_value_font_size' : (12, valid.validateOffset),
                            'text_value_color' : ((0, 0, 0), valid.validateColor),
                            'text_value_position' : ((50, 90), valid.validatePos),
                         }

    _ALL_PROPERTIES = WidgetElement._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_SPEEDO_PROPERTIES)

    _DEFAULT_VALUE = 0
    _DEFAULT_DIMENSIONS = (100, 100)

    @staticmethod
    def GetType():
        return 'speedo'

    @staticmethod
    def GetDrawRoutine():
        return SpeedoElement.DrawRoutine

    def __init__(self, *args, **kwargs):
        WidgetElement.__init__(self, self.__InitSpeedo, *args, **kwargs)
        self.__start_tick = None
        self.__tick_delay = None

    def __SetSpeedoBackgroundColor(self, color):
        # Assign The Same Colours To All Sectors (We Simulate A Car Control For Speed)
        # Usually This Is Black
        self._GetWidget().SetBackgroundColor(color)

    def __UpdateSpeedoBackgroundColor(self):
        self.__SetSpeedoBackgroundColor(self.GetProperty('bg_color'))

    def __SetSpeedoError(self, error):
        if error:
            self.__SetSpeedoBackgroundColor(self.GetProperty('error_color'))
        else:
            self.__UpdateSpeedoBackgroundColor()

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    tick):
        WidgetElement.DrawRoutine(self, pair, dc, canvas, tick)

        if self.__start_tick is None:
            self.__start_tick = canvas.context.dbhandle.api.getFileStart()
        if self.__tick_delay is None:
            self.__tick_delay = self.GetProperty('speedo_delay') * pair.GetClockPeriod()

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
        self._GetWidget().SetValue(val)
        self._GetWidget().Update()
        #self._GetWidget().OnPaint(None)

    def SetProperty(self, key, val):
        WidgetElement.SetProperty(self, key, val)
        if key == 'speedo_delay':
            self.__tick_delay = None
        if self._GetWidget() is not None:
            self.__UpdateSpeedoProperties()

    def __UpdateSpeedoProperties(self):
        # Set The Region Of Existence Of SpeedMeter (Always In Radians!!!!)
        self._GetWidget().SetAngleRange(self.GetProperty('start_angle'), self.GetProperty('end_angle'))

        # Create The Intervals That Will Divide Our SpeedMeter In Sectors
        min_val = self.GetProperty('min_val')
        max_val = self.GetProperty('max_val')
        num_intervals = self.GetProperty('num_intervals')
        interval_size = (max_val - min_val)/num_intervals
        intervals = list(range(min_val, max_val + interval_size, interval_size))
        self._GetWidget().SetIntervals(intervals)

        self.__UpdateSpeedoBackgroundColor()

        # Assign The Ticks: Here They Are Simply The String Equivalent Of The Intervals
        ticks = ['{}'.format(interval) for interval in intervals]
        self._GetWidget().SetTicks(ticks)
        # Set The Ticks/Tick Markers Colour
        self._GetWidget().SetTicksColor(self.GetProperty('tick_color'))
        # We Want To Draw 5 Secondary Ticks Between The Principal Ticks
        self._GetWidget().SetNumberOfSecondaryTicks(self.GetProperty('num_secondary_ticks'))

        # Set The Font For The Ticks Markers
        self._GetWidget().SetTicksFont(wx.Font(self.GetProperty('tick_font_size'), wx.SWISS, wx.NORMAL, wx.NORMAL))

        # Set The Text In The Center Of SpeedMeter
        self._GetWidget().SetMiddleText(self.GetProperty('center_text'))
        # Assign The Colour To The Center Text
        self._GetWidget().SetMiddleTextColor(self.GetProperty('text_color'))
        # Assign A Font To The Center Text
        self._GetWidget().SetMiddleTextFont(wx.Font(self.GetProperty('center_font_size'), wx.SWISS, wx.NORMAL, wx.BOLD))

        # Set The Colour For The Hand Indicator
        self._GetWidget().SetHandColor(self.GetProperty('hand_color'))

        # Do Not Draw The External (Container) Arc. Drawing The External Arc May
        # Sometimes Create Uglier Controls. Try To Comment This Line And See It
        # For Yourself!
        self._GetWidget().DrawExternalArc(self.GetProperty("draw_external_arc"))

        self._GetWidget().ShowTextValue(self.GetProperty("show_text_value"))
        self._GetWidget().SetTextValueFormat(self.GetProperty("text_value_format"))
        self._GetWidget().SetTextValueColor(self.GetProperty('text_value_color'))
        self._GetWidget().SetTextValuePosition(self.GetProperty('text_value_position'))
        self._GetWidget().SetTextValueFont(wx.Font(self.GetProperty('text_value_font_size'), wx.SWISS, wx.NORMAL, wx.BOLD))

        # Set The Current Value For The SpeedMeter
        self._GetWidget().SetValue(SpeedoElement._DEFAULT_VALUE)

        size = self._GetCanvas().GetSize()
        self._GetWidget().dim = size
        self._GetWidget().SetSize(size)
        self._GetWidget().SetPosition((0,0))

    def __InitSpeedo(self):
        self._SetWidget(SpeedoWidget(self._GetCanvas()))
        self.__UpdateSpeedoProperties()

SpeedoElement._ALL_PROPERTIES['dimensions'] = (SpeedoElement._DEFAULT_DIMENSIONS, valid.validateDim)
SpeedoElement._ALL_PROPERTIES['type'] = (SpeedoElement.GetType(), valid.validateString)
