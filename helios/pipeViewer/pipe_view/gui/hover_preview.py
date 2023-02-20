from __future__ import annotations
from logging import warn, error
import textwrap

import wx
import wx.lib.newevent

from .dialogs.watchlist_dialog import WatchListDlg
from .font_utils import GetMonospaceFont

from typing import Any, List, Optional, Tuple, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from ..element import Element
    from ..element_value import Element_Value
    from ..layout_context import Layout_Context
    from .argos_menu import Argos_Menu
    from .layout_canvas import Layout_Canvas

# @brief This new event triggers the canvas to just redraw the mouse-over text.
# No update of the underlying view is executed on this event.
HoverRedrawEvent, EVT_HOVER_REDRAW = wx.lib.newevent.NewEvent()

ID_HOVER_POPUP_COPY = wx.NewId()
ID_HOVER_POPUP_WATCH_RELATIVE = wx.NewId()
ID_HOVER_POPUP_WATCH_ABSOLUTE = wx.NewId()
ID_HOVER_POPUP_SEARCH = wx.NewId()
ID_PREV_ANNO = wx.NewId()
ID_NEXT_ANNO = wx.NewId()
ID_HIGHLIGHT_UOP_TOGGLE = wx.NewId()


class HoverPreview:
    '''
    @brief Stores information needed for mouse-over preview
    '''

    LINE_LENGTH = 50

    # The hover will be rendered in a separate window so we don't have to deal
    # with manually repainting the layout canvas
    class HoverPreviewWindow(wx.PopupWindow):
        def __init__(self,
                     canvas: Layout_Canvas,
                     handler: HoverPreview) -> None:
            super().__init__(canvas.GetParent())
            self.Show(False)
            self.__canvas = canvas
            self.__handler = handler
            self.Bind(wx.EVT_ENTER_WINDOW, self.OnMouse)
            sizer = wx.BoxSizer(wx.HORIZONTAL)
            panel_sizer = wx.BoxSizer(wx.HORIZONTAL)
            self.__panel = wx.Panel(self)
            self.__panel.SetFont(
                GetMonospaceFont(self.__canvas.GetSettings().hover_font_size)
            )
            self.__panel.SetForegroundColour(wx.BLACK)
            self.__text_ctrl = wx.StaticText(self.__panel)
            panel_sizer.Add(self.__text_ctrl)
            self.__panel.SetSizer(panel_sizer)
            sizer.Add(
                self.__panel,
                flag=wx.TOP | wx.BOTTOM | wx.LEFT | wx.RIGHT | wx.ALIGN_CENTER,
                border=1
            )
            self.SetSizer(sizer)

        def UpdateInfo(self,
                       element: Element,
                       annotation: str,
                       text: str,
                       position: wx.Point) -> None:
            BORDER_LIGHTNESS = 70

            _, brush = self.__canvas.UpdateTransactionColor(element,
                                                            annotation)
            color = brush.GetColour()
            self.__panel.SetBackgroundColour(color)
            self.SetBackgroundColour(color.ChangeLightness(BORDER_LIGHTNESS))
            self.__text_ctrl.SetLabel(text)
            width, height = self.GetBestSize()
            visible_area = self.__canvas.GetVisibleArea()
            x, y = position
            box_right = x + width
            box_bottom = y + height
            if box_right > visible_area[2]:  # off the right edge
                # shift left
                x -= (box_right - visible_area[2])
            if box_bottom > visible_area[3]:
                y -= (box_bottom - visible_area[3])

            if x < 0 or y < 0:
                return  # screen too small

            x, y = self.__canvas.ClientToScreen((x, y))
            old_rect = self.GetRect()
            self.SetRect((x, y, width, height))
            self.__UpdateCanvas(old_rect)

        def __UpdateCanvas(self, old_rect: wx.Rect) -> None:
            old_rect.SetPosition(
                self.__canvas.CalcUnscrolledPosition(old_rect.GetTopLeft())
            )
            old_rect.Inflate(10, 10)
            self.__canvas.RefreshRect(old_rect)

        def AcceptsFocus(self) -> bool:
            return False

        # Handle the corner case where the user manages to mouse over the
        # window
        def OnMouse(self, event: wx.MouseEvent) -> None:
            self.__handler.DestroyWindow()

        def Destroy(self) -> None:
            old_rect = self.GetRect()
            self.__UpdateCanvas(old_rect)
            self.Disable()
            self.Show(False)
            self.DestroyLater()

    def __init__(self, canvas: Layout_Canvas, context: Layout_Context) -> None:
        self.__canvas = canvas
        self.__context = context
        self.__value = ""
        self.annotation = ""
        self.element: Optional[Element] = None
        self.__enabled = False
        self.show = False
        self.position = wx.Point(0, 0)
        self.__last_move_tick: Optional[int] = None
        self.__window: Optional[HoverPreview.HoverPreviewWindow] = None

        self.__fields = ['annotation']

    def Enable(self, is_enable: bool) -> None:
        '''
        Setter for enable bool
        '''
        self.__enabled = is_enable

    # Getter for enable bool
    def IsEnabled(self) -> bool:
        return self.__enabled

    def SetFields(self, fields: List[str]) -> None:
        self.__fields = fields

    def GetFields(self) -> List[str]:
        return self.__fields

    def GetElement(self) -> Optional[Element]:
        '''
        Gets the element currently referenced by this hover preview
        @return None if there is none
        '''
        return self.element

    def IsDifferent(self, element: Optional[Element]) -> bool:
        '''
        Determine if a given element is different enough from the current
        element (self.element) to warrant updating the tooltip.
        '''

        if self.element is None or element is None:
            return True

        def prop_num(prop_name: str) -> int:
            assert self.element is not None
            assert element is not None
            return int(self.element.HasProperty(prop_name)) + \
                int(element.HasProperty(prop_name))

        def prop_diff(prop_name: str) -> bool:
            assert self.element is not None
            assert element is not None
            return self.element.GetProperty(prop_name) != \
                element.GetProperty(prop_name)

        if prop_diff('t_offset'):
            return True
        else:
            num_have_locstr = prop_num('LocationString')
            if num_have_locstr == 1 or \
               (num_have_locstr == 2 and prop_diff('LocationString')):
                return True

            else:
                num_have_toolip = prop_num('tooltip')
                if num_have_toolip == 1 or \
                   (num_have_toolip == 2 and prop_diff('tooltip')):
                    return True
                else:
                    return False

    def SetValue(self, string: str) -> None:
        '''
        Set what is supposed to be drawn.
        '''
        self.__value = textwrap.fill(string,
                                     self.LINE_LENGTH,
                                     replace_whitespace=False)

    def GetText(self) -> str:
        '''
        Get what is suppsed to be drawn.
        '''
        return self.__value

    def GetWindow(self) -> HoverPreview.HoverPreviewWindow:
        if self.__window is None:
            self.__window = HoverPreview.HoverPreviewWindow(self.__canvas,
                                                            self)
        return self.__window

    def HandleMenuClick(self, position: wx.Point) -> None:
        '''
        if applicable, pops up menu to copy and paste from hover
        '''
        if self.show:
            menu = wx.Menu()
            copy = menu.Append(ID_HOVER_POPUP_COPY, "Copy Text")
            watch_rel = menu.Append(ID_HOVER_POPUP_WATCH_RELATIVE,
                                    "Add Relative Position to Watch List")
            watch_abs = menu.Append(ID_HOVER_POPUP_WATCH_ABSOLUTE,
                                    "Add Absolute Position to Watch List")
            menu.AppendSeparator()
            search_loc = menu.Append(ID_HOVER_POPUP_SEARCH,
                                     "Search from this Location")
            menu.AppendSeparator()
            next_anno = menu.Append(ID_NEXT_ANNO,
                                    "Next change in annotation\tN")
            prev_anno = menu.Append(ID_PREV_ANNO,
                                    "Previous change in annotation\tShift+N")
            if self.__context.IsUopHighlighted(self.__value):
                self.__is_highlighted = True
                highlight_uop_toggle = menu.Append(ID_HIGHLIGHT_UOP_TOGGLE,
                                                   "Unhighlight Uop")
            else:
                self.__is_highlighted = False
                highlight_uop_toggle = menu.Append(ID_HIGHLIGHT_UOP_TOGGLE,
                                                   "Highlight Uop")
            self.__canvas.Bind(wx.EVT_MENU, self.__OnCopyText, copy)
            self.__canvas.Bind(wx.EVT_MENU,
                               self.__OnAddWatchRelative,
                               watch_rel)
            self.__canvas.Bind(wx.EVT_MENU,
                               self.__OnAddWatchAbsolute,
                               watch_abs)
            if self.element is not None and \
               self.element.HasProperty('LocationString') and \
               cast(str, self.element.GetProperty('LocationString')) != '':
                self.__canvas.Bind(wx.EVT_MENU,
                                   self.__OnSearchLocation,
                                   search_loc)
                self.__canvas.Bind(wx.EVT_MENU, self.__OnNextAnno, next_anno)
                self.__canvas.Bind(wx.EVT_MENU, self.__OnPrevAnno, prev_anno)
                self.__canvas.Bind(wx.EVT_MENU,
                                   self.__HighlightUopToggle,
                                   highlight_uop_toggle)
            else:
                search_loc.Enable(False)
                highlight_uop_toggle.Enable(False)
            self.__canvas.PopupMenu(menu, position)

    def GotoNextChange(self) -> None:
        '''
        Goto the next annotation change
        '''
        pair = self.__canvas.GetSelectionManager().GetPlaybackSelected()
        if pair:

            self.__GotoNextAnno(pair.GetElement())

    def GotoPrevChange(self) -> None:
        '''
        Goto the previous annotation change
        '''
        pair = self.__canvas.GetSelectionManager().GetPlaybackSelected()
        if pair is None:
            return
        el = pair.GetElement()

        self.__GotoPrevAnno(el)

    # @profile
    def __HighlightUopToggle(self, event: wx.MenuEvent) -> None:
        '''
        Handle click on "Highlight Uop"
        '''
        if self.__is_highlighted:
            self.__context.UnhighlightUop(self.__value)
            self.__is_highlighted = False
        else:
            self.__context.HighlightUop(self.__value)
        self.__canvas.UpdateTransactionHighlighting()
        self.__context.RedrawHighlightedElements()
        self.__canvas.FullUpdate()

    def __OnCopyText(self, event: wx.MenuEvent) -> None:
        '''
        Called when copy text entry in popup menu is pressed.
        '''
        clipboard_data = wx.TextDataObject()
        clipboard_data.SetText(self.__value)
        if not wx.TheClipboard.IsOpened():
            wx.TheClipboard.Open()
            wx.TheClipboard.SetData(clipboard_data)
            wx.TheClipboard.Close()
        else:
            warn("Cannot copy. Clipboard already open.")

    def __OnAddWatchRelative(self, evt: wx.MenuEvent) -> None:
        # frame could be None. Probably not in this case though.
        # going to let it throw an exception if it is None
        self.__AddWatch(relative=True)

    def __OnAddWatchAbsolute(self, evt: wx.MenuEvent) -> None:
        self.__AddWatch(relative=False)

    # Handle click on "Search From this Location"
    def __OnSearchLocation(self, evt: wx.MenuEvent) -> None:
        assert self.element is not None
        self.__canvas.GetFrame().ShowSearch(
            location=self.element.GetProperty('LocationString')
        )

    # Handle click on the "Next Change in Annotation"
    def __OnNextAnno(self, evt: Optional[wx.MenuEvent] = None) -> None:
        if self.element is None:
            return

        self.__GotoNextAnno(self.element)

    def __GotoNextAnno(self, el: Element) -> None:
        if not el.HasProperty('LocationString'):
            return

        location_str = cast(str, el.GetProperty('LocationString'))
        loc_mgr = self.__context.dbhandle.database.location_manager
        loc_results = loc_mgr.getLocationInfoNoVars(location_str)
        if loc_results == loc_mgr.LOC_NOT_FOUND:
            # @todo Prevent this command from being called without a valid
            # location
            return
        location_id, _, clock = loc_results

        # @todo Current tick should be based on this element's t_offset.
        cur_tick = self.__context.GetHC()

        fields = self.__context.GetTransactionFields(cur_tick,
                                                     location_str,
                                                     ['annotation'])
        cur_annotation = fields.get('annotation')
        if cur_annotation is None:
            cur_annotation = ''

        def progress_cb(percent: int,
                        num_results: int,
                        info: str) -> Tuple[bool, bool]:
            cont = True
            skip = False
            if num_results > 0:
                cont = False  # Found some results. No need to keep searching
            return (cont, skip)

        wx.BeginBusyCursor()
        try:
            # @todo Include location ID in searches
            results = self.__context.searchhandle.Search(
                'string',
                cur_annotation,
                cur_tick + 1,  # start tick
                -1,  # end tick
                [location_id],
                progress_cb,
                invert=True
            )
        except Exception as ex:
            error('Error searching: %s', ex)
            return  # Failed to search
        finally:
            wx.EndBusyCursor()

        closest = None
        for start, end, loc, annotation in results:
            if loc == location_id:
                if annotation != cur_annotation:
                    if closest is None or \
                       (start > cur_tick and start < closest):
                        closest = start

        if closest is not None:
            self.__context.GoToHC(closest)

        if wx.IsBusy():
            wx.EndBusyCursor()

    # Handle click on the "Previous Change in Annotation"
    def __OnPrevAnno(self, evt: Optional[wx.MenuEvent] = None) -> None:
        if self.element is None:
            return

        self.__GotoPrevAnno(self.element)

    def __GotoPrevAnno(self, el: Element) -> None:
        if not el.HasProperty('LocationString'):
            return

        location_str = cast(str, el.GetProperty('LocationString'))
        loc_mgr = self.__context.dbhandle.database.location_manager
        loc_results = loc_mgr.getLocationInfoNoVars(location_str)
        if loc_results == loc_mgr.LOC_NOT_FOUND:
            # \todo Prevent this command from being called without a valid
            # location
            return
        location_id, _, clock = loc_results

        # @todo Current tick should be based on this element's t_offset.
        cur_tick = self.__context.GetHC()

        fields = self.__context.GetTransactionFields(cur_tick,
                                                     location_str,
                                                     ['annotation'])
        cur_annotation = fields.get('annotation')
        if cur_annotation is None:
            cur_annotation = ''

        def progress_cb(percent: int, *args: Any) -> Tuple[bool, bool]:
            cont = True
            skip = False
            return (cont, skip)

        wx.BeginBusyCursor()
        try:
            results = self.__context.searchhandle.Search(
                'string',
                cur_annotation,
                0,  # start tick
                cur_tick - 1,  # end tick
                [location_id],
                progress_cb,
                invert=True
            )
            # TODO Support some kind of reverse search that doesn't require
            # starting from the beginning
        except Exception as ex:
            error('Error searching: %s', ex)
            return  # Failed to search
        finally:
            wx.EndBusyCursor()

        closest = None
        for start, end, loc, annotation in results:
            if loc == location_id:
                if annotation != cur_annotation:
                    # <= cur_tick because end is exclusive
                    if closest is None or (end <= cur_tick and end > closest):
                        closest = end

        if closest is not None:
            self.__context.GoToHC(max(0, closest - 1))

        if wx.IsBusy():
            wx.EndBusyCursor()

    def __AddWatch(self, relative: bool) -> None:
        frame = self.__context.GetFrame()
        assert frame is not None
        watch = cast(WatchListDlg,
                     frame.ShowDialog('watchlist', WatchListDlg))
        assert self.element is not None
        t_offset = cast(int, self.element.GetProperty('t_offset'))
        loc = cast(str, self.element.GetProperty('LocationString'))
        watch.Add(loc, t_offset, relative=relative)

    def HandleMouseMove(self,
                        position: wx.Point,
                        canvas: Layout_Canvas,
                        redraw: bool = True) -> None:
        '''
        Called when mouse move event happens in correct circumstances.
        hover needs to be enabled and mode needs to not be edit
        '''
        # On a different tick for the same element, force an upate
        force_update = self.__context.GetHC() != self.__last_move_tick
        self.__last_move_tick = self.__context.GetHC()
        (x, y) = canvas.CalcUnscrolledPosition(position)
        hits = self.__context.DetectCollision((x, y), include_subelements=True)
        old_show_state = self.show
        self.show = False
        for hit in hits:
            e = hit.GetElement()
            if e is not None:
                self.show = True
                # As long as we're showing the preview eventually,
                # capture the position.
                # offset our box slightly so pointer doesn't obscure the text
                self.position = wx.Point(position[0] + 10, position[1] + 10)
                if force_update or self.IsDifferent(e):
                    self.__SetElement(hit)

        is_dirty = old_show_state or self.show
        if is_dirty:
            assert self.element is not None
            self.GetWindow().UpdateInfo(self.element,
                                        self.annotation,
                                        self.__value,
                                        self.position)
            if self.__value:
                self.GetWindow().Show(True)
            else:
                self.DestroyWindow()
        elif not self.show:
            self.DestroyWindow()

    def __SetElement(self, pair: Element_Value) -> None:
        '''
        Sets current element
        @param pair Element_Value pair
        Internal method to implement HandleMouseMove and SetElement
        '''
        annotation = pair.GetVal()
        e = pair.GetElement()
        self.element = e
        has_loc_str = e.HasProperty('LocationString')
        if has_loc_str:
            loc_str = cast(str, e.GetProperty('LocationString'))
            has_loc_str = has_loc_str and loc_str != ''
        else:
            loc_str = None

        # If element currently has an annotation to display
        if annotation and has_loc_str:
            t_offset = cast(int, e.GetProperty('t_offset'))
            self.annotation = cast(str, annotation)
            tick_time = \
                int(t_offset * pair.GetClockPeriod() + self.__context.GetHC())
            if tick_time >= 0:
                # make query

                results = self.__context.GetTransactionFields(
                    tick_time,
                    cast(str, self.element.GetProperty('LocationString')),
                    self.__fields
                )
                intermediate = ''
                for field in list(results.keys()):
                    intermediate += '%s: %s\n' % (field, results[field])
                self.annotation = str(results.get('annotation'))
                self.SetValue(intermediate[:-1])  # remove extra newline
            else:
                self.SetValue(self.annotation)
        else:
            if e.HasProperty('tooltip') and e.GetProperty('tooltip'):
                self.annotation = cast(str, e.GetProperty('tooltip'))
                self.SetValue(self.annotation)
            elif has_loc_str:
                self.annotation = f'<{loc_str}>'
                self.SetValue(self.annotation)
            elif e.HasProperty('data'):  # For node_element
                self.annotation = f'<{e.GetProperty("data")}>'
                self.SetValue(self.annotation)
            elif e.HasProperty('annotation_basis'):  # For rpc_element
                self.annotation = cast(str, annotation)
                self.SetValue(self.annotation)
            else:
                # No idea what to print for whatever type of element this is
                self.annotation = repr(e)
                self.SetValue(self.annotation)

    def DestroyWindow(self) -> None:
        if self.__window:
            self.__window.Destroy()
            self.__window = None


class HoverPreviewOptionsDialog(wx.Dialog):
    '''
    Hover Options dialog display and collection code. No really good place to
    put this.
    '''

    def __init__(self,
                 parent: Argos_Menu,
                 hover_preview: HoverPreview) -> None:
        wx.Dialog.__init__(self,
                           parent,
                           wx.NewId(),
                           'Hover Preview Options')

        self.hover_preview = hover_preview
        self.checkOptions = {
            'start': wx.CheckBox(self, wx.NewId(), 'Start'),
            'end': wx.CheckBox(self, wx.NewId(), 'End'),
            'start_cycle': wx.CheckBox(self, wx.NewId(), 'Start Cycle'),
            'end_cycle': wx.CheckBox(self, wx.NewId(), 'End Cycle'),
            'transaction': wx.CheckBox(self, wx.NewId(), 'Transaction ID'),
            'loc': wx.CheckBox(self, wx.NewId(), 'Location'),
            'parent': wx.CheckBox(self, wx.NewId(), 'Parent Transaction'),
            'opcode': wx.CheckBox(self, wx.NewId(), 'OP Code'),
            'vaddr': wx.CheckBox(self, wx.NewId(), 'Virtual Address'),
            'paddr': wx.CheckBox(self, wx.NewId(), 'Physical Address'),
            'clock': wx.CheckBox(self, wx.NewId(), 'Clock'),
            'annotation': wx.CheckBox(self, wx.NewId(), 'Annotation'),
            'time': wx.CheckBox(self, wx.NewId(), 'Time')
        }

        sizer = wx.BoxSizer(wx.VERTICAL)
        for value in list(self.checkOptions.values()):
            sizer.Add(value, proportion=1, flag=wx.LEFT, border=5)

        done = wx.Button(self, wx.NewId(), 'Done')

        sizerbuttons = wx.BoxSizer(wx.HORIZONTAL)
        sizerbuttons.Add(done, proportion=2, flag=wx.ALL, border=5)
        self.__select = wx.Button(self, wx.NewId(), 'Select All')
        sizerbuttons.Add(self.__select, proportion=2, flag=wx.ALL, border=5)
        sizer.Add(sizerbuttons)
        self.SetSizer(sizer)
        self.Bind(wx.EVT_BUTTON, self.OnSelect, self.__select)
        self.Bind(wx.EVT_BUTTON, self.OnDone, done)
        self.is_all_selected = True

        # set current settings
        self.SetOptions()
        self.Fit()

    # Goes through check box elements and appends the checked keys to a list.
    def GetOptions(self) -> List[str]:
        checked_options = []
        for key, val in self.checkOptions.items():
            if val.GetValue():
                checked_options.append(key)
        return checked_options

    def SetOptions(self) -> None:
        fields = self.hover_preview.GetFields()
        for key, val in self.checkOptions.items():
            if key in fields:
                val.SetValue(True)

    def OnSelect(self, evt: wx.CommandEvent) -> None:
        if self.is_all_selected:
            self.__select.SetLabel('Deselect')
        else:
            self.__select.SetLabel('Select All')
        for val in self.checkOptions.values():
            val.SetValue(self.is_all_selected)
        self.is_all_selected = not self.is_all_selected

    def OnDone(self, evt: wx.CommandEvent) -> None:
        self.EndModal(wx.ID_OK)
        self.hover_preview.SetFields(self.GetOptions())
