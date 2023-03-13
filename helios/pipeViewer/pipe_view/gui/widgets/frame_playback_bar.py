from __future__ import annotations
import wx
import wx.lib.agw.hyperlink as hl
import logging
import time
from typing import Any, Optional, Tuple, Union, TYPE_CHECKING

from ...model.clock_manager import ClockManager
from ..font_utils import ScaleFont

if TYPE_CHECKING:
    from ..layout_frame import Layout_Frame
    from ...model.settings import ArgosSettings

# @package frame_playback_bar.py
#  @brief Contains FramePlaybackBar which holds all playback controls for a
#  single LayoutFrame


# @brief A wide, stretchable panel containing playback controls for a single
# Argos Frame
class FramePlaybackBar(wx.Panel):

    # Label for start-of-database cycle number
    START_TIME_FMT = 'cyc-start:{0}'

    # Label for end-of-database cycle number
    END_TIME_FMT = 'cyc-end:{0}'

    # Label for tick number
    CUR_TIME_FMT = 'tick:{0}'

    # Label for current-time current cycle number
    CUR_CYCLE_FMT = 'cycle:{0}'

    # Range of time slider values (input resolution)
    TIME_SLIDER_RANGE = 1000000

    # Label for play button
    LABEL_PLAY = 'play'
    LABEL_PAUSE = 'pause'

    # Amount to step when a key is used to press the rewind or fast-forward
    # buttons
    COARSE_KEYPRESS_STEP = 5

    # Default size of the query API's dynamic in-memory data window in terms of
    # ticks
    DB_PRELOAD_WINDOW_SIZE = 1000

    # Rate of playback when the rewind or fast-forward key is held with the
    # mouse (in current clock cycles per second)
    COARSE_MOUSE_RATE = 10

    # Maximum animated playback rate
    MAX_PLAY_RATE = 1000

    # Set up all the menus and embedded sub-menus, with all their
    # bindings/callbacks
    def __init__(self, frame: Layout_Frame) -> None:
        self.__parent = frame
        wx.Panel.__init__(self, self.__parent, wx.ID_ANY)

        context = self.__parent.GetContext()
        self.__db = context.dbhandle.database
        self.__qapi = context.dbhandle.api

        # Vars
        # keep a list of clock instances that are referenced
        self.__referenced_clocks = context.GetVisibleClocks()
        self.__current_clock: Optional[ClockManager.ClockDomain] = None
        self.__is_auto_clock = False
        # Dummy to hold play rate when playing. If None, refers to the play
        # speed spin control. Otherwise indicates an actual play speed.
        # Set when starting and pausing playback
        self.__play_rate: Optional[float] = None
        # Timestamp of last play tick
        self.__last_play_tick: Optional[float] = None
        # Cumulative advance time across multiple cycles
        self.__accum_play_cycle_fraction: Optional[float] = None
        # Is user controlling the slider
        # (True prevents auto-updates to the slider)
        self.__slider_hooked = False

        # Colors

        self.__med_blue = wx.Colour(0, 0, 220)

        # Controls
        # Placeholder. Will be created as needed
        self.__play_timer = wx.Timer(self)

        self.__hl_start = hl.HyperLinkCtrl(self, wx.ID_ANY, label=' ', URL='')
        self.__hl_start.SetToolTip(
            'Jump to first cycle in the transaction database'
        )
        self.__hl_start.AutoBrowse(False)
        self.__hl_end = hl.HyperLinkCtrl(self, wx.ID_ANY, label=' ', URL='')
        self.__hl_end.SetToolTip(
            'Jump to final cycle in the transaction database'
        )
        self.__hl_end.AutoBrowse(False)

        self.__time_slider = wx.Slider(self, wx.ID_ANY)
        self.__time_slider.SetToolTip(
            'Displays the current position in the transaction database. '
            'Slide this bar to quickly move around'
        )
        # self.__time_slider.SetThumbLength(2)
        # self.__time_slider.SetLineSize(1)
        self.__time_slider.SetRange(0, self.TIME_SLIDER_RANGE)
        self.__time_slider.SetForegroundColour(self.__med_blue)

        clocks = self.__db.clock_manager.getClocks()
        clock_choices = ['<any clk edge>']
        self.__ALL_CLOCKS = 0
        for clk in clocks:
            clock_choices.append(clk.name)

        self.__drop_clock = wx.ComboBox(self, choices=clock_choices,
                                        size=(150, -1),
                                        style=wx.CB_DROPDOWN | wx.CB_READONLY)
        self.__drop_clock.SetToolTip(
            'Current clock domain. This dictates the units in which this '
            'frame prints time. Playback and step controls will operate '
            'on this clock domain. Select any clock domain from the list.'
        )
        # self.__drop_clock.SetValue(ClockManager.HYPERCYCLE_CLOCK_NAME)
        self.__drop_clock.SetSelection(0)

        curtime_msg = 'Current cycle in the current clock domain. Also ' \
                      'shows the hyper-tick (common time) tick count of ' \
                      'this frame'

        self.__static_curcycle = wx.StaticText(self, wx.ID_ANY, size=(90, -1))
        self.__static_curcycle.SetToolTip(curtime_msg)
        self.__static_curcycle.SetForegroundColour(self.__med_blue)

        self.__static_curtick = wx.StaticText(self, wx.ID_ANY, size=(90, -1))
        self.__static_curtick.SetToolTip(curtime_msg)
        self.__static_curtick.SetForegroundColour(self.__med_blue)

        PLAYBACK_BUTTON_WIDTH = 40
        self.__btn_rw_hold = ShyButton(self,
                                       wx.ID_ANY,
                                       '<<',
                                       size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_rw_hold.SetToolTip(
            'Rewinds while left-mouse is held. Using space or enter to '
            'activate this control also rewinds but rate is dictated by '
            'keyboard repeat rate'
        )
        self.__btn_back30 = ShyButton(self,
                                      wx.ID_ANY,
                                      '-30',
                                      size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_back30.SetToolTip(
            'Steps back 30 cycles in the current clock domain'
        )
        self.__btn_back10 = ShyButton(self,
                                      wx.ID_ANY,
                                      '-10',
                                      size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_back10.SetToolTip(
            'Steps back 10 cycles in the current clock domain'
        )
        self.__btn_back3 = ShyButton(self,
                                     wx.ID_ANY,
                                     '-3',
                                     size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_back3.SetToolTip(
            'Steps back 3 cycles in the current clock domain'
        )
        self.__btn_back1 = ShyButton(self,
                                     wx.ID_ANY,
                                     '-1',
                                     size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_back1.SetToolTip(
            'Steps back 1 cycle in the current clock domain'
        )
        self.__btn_forward1 = ShyButton(self,
                                        wx.ID_ANY,
                                        '+1',
                                        size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_forward1.SetToolTip(
            'Steps forward 1 cycle in the current clock domain'
        )
        self.__btn_forward3 = ShyButton(self,
                                        wx.ID_ANY,
                                        '+3',
                                        size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_forward3.SetToolTip(
            'Steps forward 3 cycles in the current clock domain'
        )
        self.__btn_forward10 = ShyButton(self,
                                         wx.ID_ANY,
                                         '+10',
                                         size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_forward10.SetToolTip(
            'Steps forward 10 cycles in the current clock domain'
        )
        self.__btn_forward30 = ShyButton(self,
                                         wx.ID_ANY,
                                         '+30',
                                         size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_forward30.SetToolTip(
            'Steps forward 30 cycles in the current clock domain'
        )
        self.__btn_ff_hold = ShyButton(self,
                                       wx.ID_ANY,
                                       '>>',
                                       size=(PLAYBACK_BUTTON_WIDTH, -1))
        self.__btn_ff_hold.SetToolTip(
            'Fast-Forwards while left-mouse is held. Using space or enter to '
            'activate this control also fast-forwards but rate is dictated by '
            'keyboard repeat rate'
        )

        self.__txt_goto = wx.TextCtrl(self,
                                      wx.ID_ANY,
                                      size=(60, -1),
                                      value='+10')
        self.__txt_goto.SetToolTip(
            'Enter an absolute or relative cycle number (in the current clock '
            'domain) to jump to. Decimal, octal (0NNN), hex (0xNNN), or '
            'binary (0bNNN) literals are all acceptable inputs. Prefixing the '
            'number with a + or - sign will result in this value being '
            'interpreted as a relative value from the current cycle. Press '
            'Enter or click "jump" to jump to the specified cycle'
        )
        self.__btn_goto = ShyButton(self, wx.ID_ANY, 'jump', size=(60, -1))
        self.__btn_goto.SetToolTip(
            'Jump to the absolute or relative cycle specified in the jump '
            'text control'
        )

        self.__static_playback_speed_units = wx.StaticText(self,
                                                           wx.ID_ANY,
                                                           'cyc/\nsec')
        self.__spin_playback_speed = wx.SpinCtrl(self, wx.ID_ANY, '1')
        self.__spin_playback_speed.SetToolTip(
            'Set the number of cycles (in current clock domain) to display '
            'per second during playback. This can be positive or negative. '
            'A value of 0 prevents playing'
        )
        self.__spin_playback_speed.SetRange(-self.MAX_PLAY_RATE,
                                            self.MAX_PLAY_RATE)
        self.__spin_playback_speed.SetValue(1)
        self.__btn_playpause = ShyButton(self,
                                         wx.ID_ANY,
                                         self.LABEL_PLAY,
                                         size=(45, -1))
        self.__btn_playpause.SetToolTip(
            'Automatically steps through cycles (in the current clock domain) '
            'at the rate (positive or negative) specified by the "cyc/sec" '
            'spin-control beside this button')

        # Setup Fonts
        self.UpdateFontSize()

        # Event Bindings

        self.Bind(wx.EVT_TIMER, self.__OnPlayTimer, self.__play_timer)
        self.Bind(wx.EVT_CHILD_FOCUS, self.__OnChildFocus)

        self.__hl_start.Bind(hl.EVT_HYPERLINK_LEFT, self.__OnGotoStart)
        self.__hl_end.Bind(hl.EVT_HYPERLINK_LEFT, self.__OnGotoEnd)
        self.__time_slider.Bind(wx.EVT_SLIDER, self.__OnTimeSlider)
        self.__time_slider.Bind(wx.EVT_LEFT_DOWN, self.__OnTimeSliderLeftDown)
        self.__time_slider.Bind(wx.EVT_LEFT_UP, self.__OnTimeSliderLeftUp)
        self.__time_slider.Bind(wx.EVT_CHAR, self.__OnTimeSliderChar)
        self.__drop_clock.Bind(wx.EVT_COMBOBOX, self.__OnClockSelect)

        self.__btn_rw_hold.Bind(wx.EVT_LEFT_DOWN, self.__OnRWButtonDown)
        self.__btn_rw_hold.Bind(wx.EVT_LEFT_UP, self.__OnRWButtonUp)
        self.__btn_rw_hold.Bind(wx.EVT_KEY_DOWN, self.__OnRWKeyDown)
        self.__btn_back30.Bind(wx.EVT_BUTTON, self.__OnBack30)
        self.__btn_back10.Bind(wx.EVT_BUTTON, self.__OnBack10)
        self.__btn_back3.Bind(wx.EVT_BUTTON, self.__OnBack3)
        self.__btn_back1.Bind(wx.EVT_BUTTON, self.__OnBack1)
        self.__btn_back1.Bind(wx.EVT_LEFT_DOWN, self.__OnBack1LeftDown)
        self.__btn_back1.Bind(wx.EVT_LEFT_UP, self.__OnBack1LeftUp)
        self.__btn_forward1.Bind(wx.EVT_BUTTON, self.__OnForward1)
        self.__btn_forward1.Bind(wx.EVT_LEFT_DOWN, self.__OnForward1LeftDown)
        self.__btn_forward1.Bind(wx.EVT_LEFT_UP, self.__OnForward1LeftUp)
        self.__btn_forward3.Bind(wx.EVT_BUTTON, self.__OnForward3)
        self.__btn_forward10.Bind(wx.EVT_BUTTON, self.__OnForward10)
        self.__btn_forward30.Bind(wx.EVT_BUTTON, self.__OnForward30)
        self.__btn_ff_hold.Bind(wx.EVT_LEFT_DOWN, self.__OnFFButtonDown)
        self.__btn_ff_hold.Bind(wx.EVT_LEFT_UP, self.__OnFFButtonUp)
        self.__btn_ff_hold.Bind(wx.EVT_KEY_DOWN, self.__OnFFKeyDown)
        self.__txt_goto.Bind(wx.EVT_TEXT, self.__OnChangeGotoValue)
        self.__txt_goto.Bind(wx.EVT_CHAR, self.__OnGotoChar)
        self.__btn_goto.Bind(wx.EVT_BUTTON, self.__OnGoto)
        self.__spin_playback_speed.Bind(wx.EVT_SPINCTRL,
                                        self.__OnPlaySpinValChange)
        self.__spin_playback_speed.Bind(wx.EVT_CHAR, self.__OnPlaySpinValChar)
        self.__btn_playpause.Bind(wx.EVT_BUTTON, self.__OnPlayPause)

        # Layout

        curticks = wx.BoxSizer(wx.VERTICAL)
        curticks.Add(self.__static_curcycle, 1, wx.EXPAND)
        curticks.Add(self.__static_curtick, 1, wx.EXPAND)

        row1 = wx.BoxSizer(wx.HORIZONTAL)
        row1.Add(self.__hl_start,
                 0,
                 wx.ALIGN_CENTER_VERTICAL | wx.LEFT | wx.RIGHT,
                 2)
        slider_sizer = wx.BoxSizer(wx.HORIZONTAL)
        slider_sizer.Add(self.__time_slider, 1, wx.ALIGN_CENTER_VERTICAL)
        row1.Add(slider_sizer, 1, wx.EXPAND)
        row1.Add(self.__hl_end,
                 0,
                 wx.ALIGN_CENTER_VERTICAL | wx.LEFT | wx.RIGHT,
                 2)

        row2 = wx.FlexGridSizer(cols=6)
        for i in range(5):
            row2.AddGrowableCol(i)
        row2.AddGrowableRow(0)

        clock_sizer = wx.FlexGridSizer(2)
        clock_sizer.AddGrowableRow(0)
        clock_sizer.Add(self.__drop_clock,
                        0,
                        wx.ALIGN_CENTER_VERTICAL | wx.SHAPED)
        clock_sizer.Add(curticks, 0, wx.EXPAND | wx.LEFT, 3)
        row2.Add(clock_sizer, 0, wx.ALIGN_CENTER_VERTICAL | wx.EXPAND)

        nav_sizer = wx.FlexGridSizer(10)
        nav_sizer.AddGrowableRow(0)
        nav_sizer.Add(self.__btn_rw_hold, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_back30, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_back10, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_back3, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_back1, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_forward1, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_forward3, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_forward10, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_forward30, 0, wx.ALIGN_CENTER_VERTICAL)
        nav_sizer.Add(self.__btn_ff_hold, 0, wx.ALIGN_CENTER_VERTICAL)
        row2.Add(nav_sizer, 0, wx.ALIGN_CENTER_VERTICAL | wx.EXPAND)

        line_sizer1 = wx.BoxSizer(wx.HORIZONTAL)
        line_sizer1.Add(wx.StaticLine(self, wx.ID_ANY, style=wx.VERTICAL),
                        0,
                        wx.SHAPED | wx.ALIGN_CENTER_VERTICAL)
        row2.Add(line_sizer1, 0, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)

        goto_sizer = wx.FlexGridSizer(2)
        goto_sizer.AddGrowableRow(0)
        goto_sizer.Add(self.__txt_goto, 0, wx.ALIGN_CENTER_VERTICAL)
        goto_sizer.Add(self.__btn_goto, 0, wx.ALIGN_CENTER_VERTICAL)
        row2.Add(goto_sizer, 0, wx.ALIGN_CENTER_VERTICAL | wx.EXPAND)

        line_sizer2 = wx.BoxSizer(wx.HORIZONTAL)
        line_sizer2.Add(wx.StaticLine(self, wx.ID_ANY, style=wx.VERTICAL),
                        0,
                        wx.SHAPED | wx.ALIGN_CENTER_VERTICAL)
        row2.Add(line_sizer2, 0, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)

        playback_sizer = wx.FlexGridSizer(2)
        playback_sizer.AddGrowableRow(0)
        playback_sizer.Add(self.__btn_playpause, 0, wx.ALIGN_CENTER_VERTICAL)
        spinner_sizer = wx.BoxSizer(wx.HORIZONTAL)
        spinner_sizer.Add(self.__static_playback_speed_units,
                          0,
                          wx.ALIGN_CENTER_VERTICAL | wx.LEFT,
                          2)
        spinner_sizer.Add(self.__spin_playback_speed,
                          0,
                          wx.ALIGN_CENTER_VERTICAL | wx.LEFT,
                          1)
        playback_sizer.Add(spinner_sizer, 0, wx.ALIGN_CENTER_VERTICAL)
        row2.Add(playback_sizer, 0, wx.ALIGN_RIGHT | wx.EXPAND)

        playback_controls = [
            c for c in self.GetChildren()
            if not isinstance(c, wx.StaticText) and
            row2.GetItem(c, recursive=True) is not None
        ]
        min_playback_height = max(
            c.GetBestSize().GetHeight() for c in playback_controls
        )
        for c in playback_controls:
            orig_width, _ = c.GetMinSize()
            c.SetMinSize((orig_width, min_playback_height))

        rows = wx.BoxSizer(wx.VERTICAL)
        rows.Add(row2, 0, wx.EXPAND | wx.TOP, 2)
        rows.Add(wx.StaticLine(self, wx.ID_ANY),
                 0,
                 wx.EXPAND | wx.TOP | wx.BOTTOM,
                 4)
        rows.Add(row1, 0, wx.EXPAND)

        self.SetSizer(rows)

        self.Fit()
        self.SetAutoLayout(True)

        # Initialization

        self.__OnChangeGotoValue()  # Validate initial GOTO value
        self.__GetSelectedClock()
        self.__Update()

    # Refreshes the content of this panel
    def Refresh(
        self,
        eraseBackground: bool = True,
        rect: Optional[Union[wx.Rect, Tuple[int, int, int, int]]] = None
    ) -> None:
        self.__Update()

    # Updates values displayed to match current context and selection
    #  @note Does not invoke FullUpdate on the associated layout canvas
    def __Update(self) -> None:
        # Compute cycle to display
        assert self.__current_clock is not None
        start_cycle = self.__current_clock.HypercycleToLocal(
            self.__qapi.getFileStart()
        )
        inc_end_cycle = self.__current_clock.HypercycleToLocal(
            self.__qapi.getFileInclusiveEnd()
        )
        hc = self.__parent.GetContext().GetHC()
        cur_cycle = self.__current_clock.HypercycleToLocal(hc)

        self.__hl_start.SetLabel(self.START_TIME_FMT.format(start_cycle))
        self.__hl_end.SetLabel(self.END_TIME_FMT.format(inc_end_cycle))
        self.__static_curcycle.SetLabel(self.CUR_CYCLE_FMT.format(cur_cycle))
        self.__static_curtick.SetLabel(self.CUR_TIME_FMT.format(hc))

        # Do not update if user is editing slider
        if not self.__slider_hooked:
            cyc_range = max(float(inc_end_cycle - start_cycle + 1), 0.1)
            new_slider_cycle = min(1.0, (cur_cycle - start_cycle) / cyc_range)
            # Do not update the slider unless the actual cycle has changed
            prev_slider_cycle = self.__ComputeSliderCycle(cyc_range)
            if prev_slider_cycle != new_slider_cycle:
                self.__time_slider.SetValue(
                    int(new_slider_cycle * self.TIME_SLIDER_RANGE)
                )

    # Moves to the start cycle of this database
    def __OnGotoStart(self, evt: hl.HyperLinkEvent) -> None:
        hc = self.__qapi.getFileStart()
        self.__parent.GetContext().GoToHC(hc)

    # Moves to the end cycle of this database
    def __OnGotoEnd(self, evt: hl.HyperLinkEvent) -> None:
        hc = self.__qapi.getFileEnd()
        self.__parent.GetContext().GoToHC(hc)

    # Time slider being moved.
    #  Pauses playback
    def __OnTimeSlider(self, evt: wx.ScrollEvent) -> None:
        self.__PausePlaying()

        # Show current cycle
        clk = self.__GetSelectedClock(auto_chosen=False)

        cur_cycle = self.__ComputeSliderLocalCycle(clk)
        self.__static_curcycle.SetLabel(self.CUR_CYCLE_FMT.format(cur_cycle))
        self.__static_curtick.SetLabel(self.CUR_TIME_FMT.format(
            clk.LocalToHypercycle(cur_cycle))
        )

    # Mouse down even on time slider
    def __OnTimeSliderLeftDown(self, evt: wx.MouseEvent) -> None:
        evt.Skip()

    # Moves to a specific cycle of this database
    #  Also pauses playback
    def __OnTimeSliderLeftUp(self, evt: wx.MouseEvent) -> None:
        self.__PausePlaying()
        clk = self.__GetSelectedClock()
        cur_cycle = self.__ComputeSliderLocalCycle(clk)

        self.__slider_hooked = True
        try:
            hc = clk.LocalToHypercycle(cur_cycle)

            # Do not do this. It currently confuses the IntervalWindow
            # Reduce preload window to 1 tick in either direction to maximize
            # load speed
            self.__parent.GetContext().GoToHC(hc)
        except Exception:
            raise
        finally:
            self.__slider_hooked = False
            evt.Skip()

    # Handles keyboard input on the time slider
    def __OnTimeSliderChar(self, evt: wx.KeyEvent) -> None:
        pass  # Do not forward the event

    # Updates information for new clock selection.
    #  Does not actually change context
    def __OnClockSelect(self, evt: wx.CommandEvent) -> None:
        self.__referenced_clocks = \
            self.__parent.GetContext().GetVisibleClocks()
        self.__parent.Refresh()

    def __OnBack30(self, evt: wx.CommandEvent) -> None:
        self.__StepBackward(30)

    def __OnBack10(self, evt: wx.CommandEvent) -> None:
        self.__StepBackward(10)

    def __OnBack3(self, evt: wx.CommandEvent) -> None:
        self.__StepBackward(3)

    def __OnBack1(self, evt: wx.CommandEvent) -> None:
        self.__StepBackward(1)

    def __OnBack1LeftDown(self, evt: wx.MouseEvent) -> None:
        self.__StartPlaying(rate=-1)
        evt.Skip()

    def __OnBack1LeftUp(self, evt: wx.MouseEvent) -> None:
        self.__PausePlaying()
        evt.Skip()

    def __OnForward1(self, evt: wx.CommandEvent) -> None:
        self.__StepForward(1)

    def __OnForward1LeftDown(self, evt: wx.MouseEvent) -> None:
        self.__StartPlaying(rate=1)
        evt.Skip()

    def __OnForward1LeftUp(self, evt: wx.MouseEvent) -> None:
        self.__PausePlaying()
        evt.Skip()

    def __OnForward3(self, evt: wx.CommandEvent) -> None:
        self.__StepForward(3)

    def __OnForward10(self, evt: wx.CommandEvent) -> None:
        self.__StepForward(10)

    def __OnForward30(self, evt: wx.CommandEvent) -> None:
        self.__StepForward(30)

    def __OnRWButtonDown(self, evt: wx.MouseEvent) -> None:
        self.__StartPlaying(rate=-self.COARSE_MOUSE_RATE)
        evt.Skip()

    def __OnRWButtonUp(self, evt: wx.MouseEvent) -> None:
        self.__PausePlaying()
        evt.Skip()

    def __OnRWKeyDown(self, evt: wx.KeyEvent) -> None:
        self.__StepBackward(self.COARSE_KEYPRESS_STEP)
        evt.Skip()

    def __OnFFButtonDown(self, evt: wx.MouseEvent) -> None:
        self.__StartPlaying(rate=self.COARSE_MOUSE_RATE)
        evt.Skip()

    def __OnFFButtonUp(self, evt: wx.MouseEvent) -> None:
        self.__PausePlaying()
        evt.Skip()

    def __OnFFKeyDown(self, evt: wx.KeyEvent) -> None:
        self.__StepForward(self.COARSE_KEYPRESS_STEP)
        evt.Skip()

    def __OnChangeGotoValue(self,
                            evt: Optional[wx.CommandEvent] = None) -> None:
        try:
            _ = self.__GetGotoCycle()
        except ValueError:
            self.__btn_goto.Enable(False)
        else:
            self.__btn_goto.Enable(True)

    def __OnGotoChar(self, evt: wx.KeyEvent) -> None:
        if evt.GetKeyCode() == wx.WXK_RETURN:
            self.__OnGoto()
        else:
            evt.Skip()

    def __OnGoto(self, evt: Optional[wx.CommandEvent] = None) -> None:
        try:
            goto_cycle, relative = self.__GetGotoCycle()
        except Exception:
            wx.MessageBox(
                f'Could not convert jump value "{self.__txt_goto.GetValue()}" '
                'to an integer. This string must be a decimal or hexidecimal '
                '(prefixed with 0x) integer',
                'Could not jump to cycle',
                style=wx.OK,
                parent=self
            )
        else:
            clk = self.__GetSelectedClock()
            cur_cycle = clk.HypercycleToLocal(
                self.__parent.GetContext().GetHC()
            )
            if relative == '+':
                cur_cycle += goto_cycle
            elif relative == '-':
                cur_cycle -= goto_cycle
            else:
                cur_cycle = goto_cycle

            # Jump. Context must constrain
            self.__parent.GetContext().GoToHC(clk.LocalToHypercycle(cur_cycle))
            self.__parent.GetCanvas().SetFocus()  # go back to canvas

    # Changed spin value
    #
    #  Enables or disables the play/pause button depending on current value
    def __OnPlaySpinValChange(self, evt: wx.SpinEvent) -> None:
        spin_val = self.__spin_playback_speed.GetValue()
        self.__btn_playpause.Enable(spin_val != 0)

    # Keystroke on spin value
    #
    # Starts or stops playing if enter is pressed
    def __OnPlaySpinValChar(self, evt: wx.KeyEvent) -> None:
        if evt.GetKeyCode() == wx.WXK_RETURN:
            self.__OnPlayPause()
        else:
            evt.Skip()

    # Handler for clicking the play-pause button
    def __OnPlayPause(self, evt: Optional[wx.CommandEvent] = None) -> None:
        if self.__btn_playpause.GetLabel() == self.LABEL_PLAY:
            self.__StartPlaying()
        elif self.__btn_playpause.GetLabel() == self.LABEL_PAUSE:
            self.__PausePlaying()
        else:
            raise RuntimeError('Play button label was neither play nor pause')

    # If a ShyButton is being focused, focus the canvas instead
    def __OnChildFocus(self, evt: wx.FocusEvent) -> None:
        obj = evt.GetWindow()
        if obj and not obj.AcceptsFocus():
            self.__parent.GetCanvas().SetFocus()

    # Helper methods

    # Steps current clock forward by a number of cycles on the current clock
    #  @param step Number of cycles on local clock to step forward
    def __StepForward(self, step: int = 1) -> None:
        # to avoid rounding errors
        # not a perfect fix since playback rate will not be exact
        step = int(step)

        clk = self.__GetSelectedClock()
        cur_cycle = self.__GetNextCycle(clk, step)
        self.__parent.GetContext().GoToHC(clk.LocalToHypercycle(cur_cycle))

    # Steps current clock backward by a number of cycles on the current clock
    #  @param step Number of cycles on local clock to step backward (should be
    #  positive)
    def __StepBackward(self, step: int = 1) -> None:
        step = int(step)
        clk = self.__GetSelectedClock(forward=False)
        cur_cycle = self.__GetPrevCycle(clk, step)

        # because of floor and since the clock is already behind hc, add one
        # @todo Stepping backward (and forward) should actually choose the
        # closest clock edge-by-edge. It should never take the closest clock
        # and add/subtract a number greater than 1.
        self.__parent.GetContext().GoToHC(clk.LocalToHypercycle(cur_cycle))

    # Gets the value of the goto (jump) text box and converts it to a tuple:
    #  (absolute_value, relative) where absolute_value is the integer value in
    #  the textbox and relative is the sign character extracted. If no sign
    #  character is found at the start of the string, the relative component
    #  of the result tuple will be None.
    #  @throw ValueError if number cannot be converted (see __StrintToInt)
    #  @note Supports all numeric types that __StringToInt does
    def __GetGotoCycle(self) -> Tuple[int, Optional[str]]:
        cyc_text = self.__txt_goto.GetValue()
        cyc_text = cyc_text.strip()
        if cyc_text.find('+') == 0:
            relative = '+'
            cyc_text = cyc_text[1:]
        elif cyc_text.find('-') == 0:
            relative = '-'
            cyc_text = cyc_text[1:]
        else:
            relative = None

        cyc_text = cyc_text.strip()
        return (self.__StringToInt(cyc_text), relative)

    # Computes the current cycle indicated by the slider in terms of the given
    # clock.
    def __ComputeSliderLocalCycle(self, clk: ClockManager.ClockDomain) -> int:
        start_cycle = clk.HypercycleToLocal(self.__qapi.getFileStart())
        inc_end_cycle = clk.HypercycleToLocal(
            self.__qapi.getFileInclusiveEnd()
        )

        cyc_range = max(float(inc_end_cycle - start_cycle), 0.1)
        return int(self.__ComputeSliderCycle(cyc_range))

    # Computes the current cycle based on the slider position interpolated
    #  within the given cycle range tuple representing the range of cycles
    #  addressable by the slider
    def __ComputeSliderCycle(self, cyc_range: float) -> int:
        # Scale into [0,1) range
        portion = self.__time_slider.GetValue() / float(self.TIME_SLIDER_RANGE)
        assert self.__current_clock is not None
        return int(
            (portion * cyc_range) +
            self.__current_clock.HypercycleToLocal(self.__qapi.getFileStart())
        )

    # Gets the current Clock object selected by the dropdown
    def __GetSelectedClock(
        self,
        forward: bool = True,
        auto_chosen: bool = False
    ) -> ClockManager.ClockDomain:
        # Find the current clock
        clocks = self.__db.clock_manager.getClocks()
        hc = self.__parent.GetContext().GetHC()
        clock_selection = self.__drop_clock.GetCurrentSelection()

        if clock_selection == self.__ALL_CLOCKS:
            auto_chosen = True
            clk = self.__db.clock_manager.getClosestClock(
                hc,
                self.__referenced_clocks,
                forward
            )
        else:
            auto_chosen = False
            # <any clk edge> bumps forward
            clk = clocks[self.__drop_clock.GetCurrentSelection() - 1]
        self.__current_clock = clk
        self.__is_auto_clock = auto_chosen
        return clk

    # Sets up timer to start playing with given rate in cycles/sec.
    #  @param rate Rate to attempt to refresh in current-clock-cycles/sec.
    #  If rate==None, uses the rate from self.__spin_playback_speed control
    def __StartPlaying(self,
                       rate: Optional[float] = None,
                       delay: int = 0) -> None:
        self.__play_rate = rate
        self.__last_play_tick = time.time() + delay
        self.__accum_play_cycle_fraction = 0
        self.__btn_playpause.SetLabel(self.LABEL_PAUSE)
        self.__UpdatePlayTimer(delay=delay)

    # Handle clicking on the pause button
    def __PausePlaying(self) -> None:
        self.__play_timer.Stop()
        self.__play_rate = None
        self.__last_play_tick = None
        self.__accum_play_cycle_fraction = None

        # Update label if not already play. Causes flicker if set to same label
        if self.__btn_playpause.GetLabel() != self.LABEL_PLAY:
            self.__btn_playpause.SetLabel(self.LABEL_PLAY)

    # Handler for __play_timer event
    def __OnPlayTimer(self, evt: wx.TimerEvent) -> None:
        # Determine play rate
        if self.__play_rate is not None:
            play_rate = self.__play_rate
        else:
            play_rate = self.__spin_playback_speed.GetValue()

        # Determine step size based on rate
        assert self.__last_play_tick is not None
        cur_time = time.time()
        if cur_time < self.__last_play_tick:
            # Someone moved __last_play_tick ahead. This timer callback could
            # have happened when someone was trying to cancel it, resulting in
            # an extra event (e.g. two steps-forward when arrow-right was
            # pressed but not held). Simply ignore this event.
            msg = 'Playback not advancing'
            logging.getLogger('FramePlayback').debug(msg)
            return

        delta_time = cur_time - self.__last_play_tick
        self.__last_play_tick = cur_time
        # How many ticks should be advanced in this time delta for this
        # play_rate
        advance_raw = play_rate * delta_time

        if play_rate > 0:
            assert self.__accum_play_cycle_fraction is not None
            self.__accum_play_cycle_fraction += advance_raw
            if self.__accum_play_cycle_fraction > 1:  # Greater than threshold
                advance = int(self.__accum_play_cycle_fraction)
                self.__accum_play_cycle_fraction %= 1  # Fractional portion
            else:
                advance = 0
                pass  # Do no update
        elif play_rate < 0:
            assert self.__accum_play_cycle_fraction is not None
            self.__accum_play_cycle_fraction += advance_raw
            if self.__accum_play_cycle_fraction < -1:
                advance = int(self.__accum_play_cycle_fraction)
                self.__accum_play_cycle_fraction %= -1  # Fractional portion
            else:
                advance = 0
                pass  # Do no update
        else:
            assert 0, 'Invalid play rate of 0'

        # Move to next (back or forward) cycles
        context = self.__parent.GetContext()
        if advance < -1:
            self.__StepBackward(-advance)
            if context.GetHC() <= self.__qapi.getFileStart():
                # Pause if we've exceeded the duration of the data.
                self.__PausePlaying()
            else:
                self.__UpdatePlayTimer()
                self.__parent.Update()  # Force redraw
        elif advance >= 1:
            self.__StepForward(advance)
            if context.GetHC() >= self.__qapi.getFileInclusiveEnd():
                # Pause if we've reached the duration of the data.
                self.__PausePlaying()
            else:
                self.__UpdatePlayTimer()
                # print 'UPDATING', self.__parent
                # import pdb; pdb.set_trace()
                # Force redraw NOW. Refresh does not cause redraws
                self.__parent.Update()
        else:
            self.__UpdatePlayTimer()
            # No advance because it was too small.
            # self.__accum_play_cycle_fraction was incremented above
            pass

        logging.getLogger('FramePlayback').debug(
            'Playback cycle advance=%s advance_frac=%s dt=%s cur_t=%s',
            advance,
            self.__accum_play_cycle_fraction,
            delta_time,
            cur_time
        )

    # Updates the play timer value based on the play speed spin ctrl
    #  @pre Do not invoke if spin ctrl value is 0
    def __UpdatePlayTimer(self, delay: int = 0) -> None:
        # Determine play rate
        if self.__play_rate is not None:
            play_rate = self.__play_rate
        else:
            play_rate = self.__spin_playback_speed.GetValue()

        # Clamp play_rate to be safe
        play_rate = max(-self.MAX_PLAY_RATE,
                        min(self.MAX_PLAY_RATE, play_rate))

        # Start up a new timer with current play speed if possible
        try:
            cps = float(play_rate)
        except Exception:
            self.__PausePlaying()
            wx.MessageBox(
                f'Could not convert cycles/second string "{play_rate}" to a '
                'float. This string must be a decimal floating poing or '
                'integer value',
                'Could play',
                style=wx.OK,
                parent=self
            )
        else:
            if cps == 0:
                self.__PausePlaying()
            else:

                # Actually play (time in milliseconds)
                next_timer = abs(1000.0 / cps) + delay * 1000
                logging.getLogger('FramePlayback').debug(
                    'Playback cps=%s delay=%s next_timer=%s',
                    cps,
                    delay,
                    next_timer
                )

                # Delay scheduling of timer and always schedule immediately.
                # This lets us redraw as fast as possible. May need to
                # specially handle case __OnPlayTimer wher the tick advances
                # less than 1 cycle due to quick refresh
                #
                # TODO: Compute proper refresh rate based on observed timer
                # speed while properly considering redraw time cost between the
                # most recent timer event and now.
                def TimerStarter() -> None:
                    # If not paused/stopped
                    if self.__last_play_tick is not None:
                        self.__play_timer.Start(10, oneShot=True)

                wx.CallAfter(TimerStarter)

    # Converts a string to an integer allowing for hex, binary, and octal
    #  prefixes
    #  @note Does NOT support negative or positive prefixes
    #  @throw ValueError if number cannot be converted
    def __StringToInt(self, num_str: str) -> int:
        num_str = num_str.strip()
        if (num_str.find('-') == 0) or (num_str.find('+') == 0):
            raise ValueError('__StringToInt does not expect a -/+ prefix')
        if num_str.find('0x') == 0 or num_str.find('0X') == 0:
            num = int(num_str, 16)
        elif num_str.find('0b') == 0 or num_str.find('0b') == 0:
            num = int(num_str, 2)
        elif num_str.find('0') == 0 or num_str.find('0') == 0:
            num = int(num_str, 8)
        else:
            num = int(num_str, 10)

        return num

    # Returns the current cycle in the selected clock domain
    def __GetCurrentCycle(self, clk: ClockManager.ClockDomain) -> int:
        return clk.HypercycleToLocal(self.__parent.GetContext().GetHC())

    # Determines the next clock cycle within the database file extents.
    #  @param step Size of forward step (should be a positive number)
    #  @return an integer with the next local cycle if valid. This may be
    #  the same as the current value if the current value is at the right
    #  edge of the database transaction range.
    def __GetNextCycle(self,
                       clk: ClockManager.ClockDomain,
                       step: int = 1) -> int:
        cur = self.__GetCurrentCycle(clk)
        next = step + cur
        return next

    # Determines the prev clock cycle within the database file extents.
    #  @param step Size of backward step (should be a positive number)
    #  @return an integer with the next local cycle if valid. This may be
    #  the same as the current value if the current value is at the left
    #  edge of the database transaction range.
    def __GetPrevCycle(self,
                       clk: ClockManager.ClockDomain,
                       step: int = 1) -> int:
        cur = self.__GetCurrentCycle(clk)
        prev = cur - step
        return prev

    # Steps current clock forward by a number of cycles on the current clock
    #  @param step Number of cycles on local clock to step forward
    #  Called by external element
    def StepForward(self, step: int = 1) -> None:
        self.__StepForward(step)

    # Steps current clock backward by a number of cycles on the current clock
    #  @param step Number of cycles on local clock to step backward (should be
    #  positive)
    #  Called by external element
    def StepBackward(self, step: int = 1) -> None:
        self.__StepBackward(step)

    # Used by input decoder when arrow key is held down
    def StartPlaying(self, step: int = 1, delay: int = 0) -> None:
        self.__StartPlaying(step, delay=delay)

    # Used by input decoder when arrow key is released
    # Public interface to pause playing (from Frame)
    #  @note This exists to stop playing when the owning frame is trying to
    #  close using a wx.CallAfter but playback is not allowing the event queue
    #  to completely drain
    def PausePlaying(self) -> None:
        self.__PausePlaying()

    # Attempt to select the given clock by name
    def SetDisplayClock(self,
                        clock_name: str,
                        error_if_not_found: bool = True) -> bool:
        cn = clock_name.lower()
        for idx, item in enumerate(self.__drop_clock.GetItems()):
            if item.lower() == cn:
                self.__drop_clock.SetSelection(idx)
                self.__parent.Refresh()  # Allow update of the cycle printout
                return True

        if error_if_not_found:
            clock_items = ', '.join(x for x in self.__drop_clock.GetItems())
            raise IndexError(
                f'No clock known for frame "{self.__parent.GetTitle()}" '
                f'with (case insensitive) name "{clock_name}". '
                f'Options are: {clock_items}'
            )

        return False

    # To to a specific cyle on the currently displayed clock for this frame
    def GoToCycle(self, cycle: int) -> None:
        assert cycle is not None
        clk = self.__GetSelectedClock()
        assert clk is not None
        self.__parent.GetContext().GoToHC(clk.LocalToHypercycle(cycle))

    # Sets focus to the jump-to-time text entry box
    def FocusJumpBox(self) -> None:
        self.__txt_goto.SetFocus()

    # Gets the global settings object
    def GetSettings(self) -> ArgosSettings:
        return self.__parent.GetSettings()

    # Updates font sizes for all of the controls
    def UpdateFontSize(self) -> None:
        self.__fnt_tiny = wx.Font(
            ScaleFont(self.GetSettings().playback_font_size),
            wx.NORMAL,
            wx.NORMAL,
            wx.NORMAL
        )
        self.__fnt_bold_med = wx.Font(
            ScaleFont(self.GetSettings().playback_font_size),
            wx.NORMAL,
            wx.NORMAL,
            wx.BOLD
        )
        self.__fnt = wx.Font(
            ScaleFont(self.GetSettings().playback_font_size),
            wx.NORMAL,
            wx.NORMAL,
            wx.NORMAL
        )
        self.__fnt_hl = self.__fnt.Underlined()

        self.SetFont(self.__fnt)
        self.__drop_clock.SetFont(self.__fnt)
        self.__static_curcycle.SetFont(self.__fnt_bold_med)
        self.__static_curtick.SetFont(self.__fnt_tiny)
        self.__hl_start.SetFont(self.__fnt_hl)
        self.__hl_end.SetFont(self.__fnt_hl)
        self.__btn_rw_hold.SetFont(self.__fnt)
        self.__btn_back30.SetFont(self.__fnt)
        self.__btn_back10.SetFont(self.__fnt)
        self.__btn_back3.SetFont(self.__fnt)
        self.__btn_back1.SetFont(self.__fnt)
        self.__btn_forward1.SetFont(self.__fnt)
        self.__btn_forward3.SetFont(self.__fnt)
        self.__btn_forward10.SetFont(self.__fnt)
        self.__btn_forward30.SetFont(self.__fnt)
        self.__btn_ff_hold.SetFont(self.__fnt)
        self.__txt_goto.SetFont(self.__fnt)
        self.__btn_goto.SetFont(self.__fnt)
        self.__static_playback_speed_units.SetFont(self.__fnt)
        self.__spin_playback_speed.SetFont(self.__fnt)
        self.__btn_playpause.SetFont(self.__fnt)

        self.Layout()


# A button which does not accept focus
class ShyButton(wx.Button):

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        wx.Button.__init__(self, *args, **kwargs)

    # overriding this method should do the trick, but although it doesn't work
    # we use this method to identify shy button objects and de-focus them if
    # they get focused
    def AcceptsFocus(self) -> bool:
        return False
