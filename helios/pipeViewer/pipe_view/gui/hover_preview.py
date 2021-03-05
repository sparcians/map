

from logging import warn, debug, error
import string
import logging
import sys
import textwrap

import wx
import wx.lib.newevent

from gui.dialogs.watchlist_dialog import WatchListDlg
from gui.font_utils import GetMonospaceFont

# # @brief This new event triggers the canvas to just redraw the mouse-over text.
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

    # The hover will be rendered in a separate window so we don't have to deal with manually repainting the layout canvas
    class HoverPreviewWindow(wx.PopupWindow):
        def __init__(self, canvas, handler):
            super(self.__class__, self).__init__(canvas.GetParent())
            self.Show(False)
            self.__canvas = canvas
            self.__handler = handler
            self.Bind(wx.EVT_ENTER_WINDOW, self.OnMouse)
            sizer = wx.BoxSizer(wx.HORIZONTAL)
            panel_sizer = wx.BoxSizer(wx.HORIZONTAL)
            self.__panel = wx.Panel(self)
            self.__panel.SetFont(GetMonospaceFont(11))
            self.__panel.SetForegroundColour(wx.BLACK)
            self.__text_ctrl = wx.StaticText(self.__panel)
            panel_sizer.Add(self.__text_ctrl)
            self.__panel.SetSizer(panel_sizer)
            sizer.Add(self.__panel,
                      flag=wx.TOP | wx.BOTTOM | wx.LEFT | wx.RIGHT | wx.ALIGN_CENTER,
                      border=1)
            self.SetSizer(sizer)

        def UpdateInfo(self, element, annotation, text, position):
            BORDER_LIGHTNESS = 70

            _, brush = self.__canvas.UpdateTransactionColor(element, annotation)
            color = brush.GetColour()
            self.__panel.SetBackgroundColour(color)
            self.SetBackgroundColour(color.ChangeLightness(BORDER_LIGHTNESS))
            self.__text_ctrl.SetLabel(text)
            width, height = self.GetBestSize()
            visible_area = self.__canvas.GetVisibleArea()
            x, y = position
            box_right = x + width
            box_bottom = y + height
            if box_right > visible_area[2]: # off the right edge
                # shift left
                x -= (box_right - visible_area[2])
            if box_bottom > visible_area[3]:
                y -= (box_bottom - visible_area[3])

            if x < 0 or y < 0:
                return # screen too small

            x, y = self.__canvas.ClientToScreen((x, y))
            self.SetRect((x, y, width, height))

        def AcceptsFocus(self):
            return False

        # Handle the corner case where the user manages to mouse over the window
        def OnMouse(self, event):
            self.__handler.DestroyWindow()


    def __init__(self, canvas, context):
        self.__canvas = canvas
        self.__context = context
        self.__value = ""
        self.annotation = ""
        self.element = None
        self.__enabled = False
        self.show = False
        self.position = (0, 0)
        self.__last_move_tick = None
        self.__window = None

        self.__fields = ['annotation']

    def Enable(self, is_enable):
        '''
        Setter for enable bool
        '''
        self.__enabled = is_enable

    # Getter for enable bool
    def IsEnabled(self):
        return self.__enabled

    def SetFields(self, fields):
        self.__fields = fields

    def GetFields(self):
        return self.__fields

    def GetElement(self):
        '''
        Gets the element currently referenced by this hover preview
        @return None if there is none
        '''
        return self.element

    def IsDifferent(self, element):
        '''
        Determine if a given element is different enough from the current element
        (self.element) to warrant updating the tooltip.
        '''
        if not (self.element and element):
            return True
        elif self.element.GetProperty('t_offset') != element.GetProperty('t_offset'):
            return True
        else:
            num_have_locstr = int(self.element.HasProperty('LocationString')) + int(element.HasProperty('LocationString'))
            if num_have_locstr == 1 \
               or (num_have_locstr == 2 and self.element.GetProperty('LocationString') != element.GetProperty('LocationString')):
                return True

            else:
                num_have_toolip = int(self.element.HasProperty('tooltip')) + int(element.HasProperty('tooltip'))
                if num_have_toolip == 1 \
                   or (num_have_toolip == 2 and self.element.GetProperty('tooltip') != element.GetProperty('tooltip')):
                    return True
                else:
                    return False

    def SetValue(self, string):
        '''
        Set what is supposed to be drawn.
        '''
        self.__value = textwrap.fill(string,
                            self.LINE_LENGTH,
                            replace_whitespace = False)

    def GetText(self):
        '''
        Get what is suppsed to be drawn.
        '''
        return self.__value

    def GetWindow(self):
        if not self.__window:
            self.__window = HoverPreview.HoverPreviewWindow(self.__canvas, self)
        return self.__window

    def HandleMenuClick(self, position):
        '''
        if applicable, pops up menu to copy and paste from hover
        '''
        if self.show:
            menu = wx.Menu()
            copy = menu.Append(ID_HOVER_POPUP_COPY, "Copy Text")
            watch_rel = menu.Append(ID_HOVER_POPUP_WATCH_RELATIVE, "Add Relative Position to Watch List")
            watch_abs = menu.Append(ID_HOVER_POPUP_WATCH_ABSOLUTE, "Add Absolute Position to Watch List")
            menu.AppendSeparator()
            search_loc = menu.Append(ID_HOVER_POPUP_SEARCH, "Search from this Location")
            menu.AppendSeparator()
            next_anno = menu.Append(ID_NEXT_ANNO, "Next change in annotation\tN")
            prev_anno = menu.Append(ID_PREV_ANNO, "Previous change in annotation\tShift+N")
            if self.__context.IsUopHighlighted(self.__value):
                self.__is_highlighted = True
                highlight_uop_toggle = menu.Append(ID_HIGHLIGHT_UOP_TOGGLE, "Unhighlight Uop")
            else:
                self.__is_highlighted = False
                highlight_uop_toggle = menu.Append(ID_HIGHLIGHT_UOP_TOGGLE, "Highlight Uop")
            self.__canvas.Bind(wx.EVT_MENU, self.__OnCopyText, copy)
            self.__canvas.Bind(wx.EVT_MENU, self.__OnAddWatchRelative, watch_rel)
            self.__canvas.Bind(wx.EVT_MENU, self.__OnAddWatchAbsolute, watch_abs)
            if self.element is not None \
              and self.element.HasProperty('LocationString') is True \
              and self.element.GetProperty('LocationString') != '':
                self.__canvas.Bind(wx.EVT_MENU, self.__OnSearchLocation, search_loc)
                self.__canvas.Bind(wx.EVT_MENU, self.__OnNextAnno, next_anno)
                self.__canvas.Bind(wx.EVT_MENU, self.__OnPrevAnno, prev_anno)
                self.__canvas.Bind(wx.EVT_MENU, self.__HighlightUopToggle, highlight_uop_toggle)
            else:
                search_loc.Enable(False)
                highlight_uop_toggle.Enable(False)
            self.__canvas.PopupMenu(menu, position)

    def GotoNextChange(self):
        '''
        Goto the next annotation change
        '''
        pair = self.__canvas.GetSelectionManager().GetPlaybackSelected()
        if pair:

            self.__GotoNextAnno(pair.GetElement())

    def GotoPrevChange(self):
        '''
        Goto the previous annotation change
        '''
        pair = self.__canvas.GetSelectionManager().GetPlaybackSelected()
        if pair is None:
            return
        el = pair.GetElement()

        self.__GotoPrevAnno(el)

    # @profile
    def __HighlightUopToggle(self, event):
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

    def __OnCopyText(self, event):
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

    def __OnAddWatchRelative(self, evt):
        # frame could be None. Probably not in this case though.
        # going to let it throw an exception if it is None
        self.__AddWatch(relative = True)

    def __OnAddWatchAbsolute(self, evt):
        self.__AddWatch(relative = False)

    # # Handle click on "Search From this Location"
    def __OnSearchLocation(self, evt):
        self.__canvas.GetFrame().ShowSearch(location = self.element.GetProperty('LocationString'))

    # # Handle click on the "Next Change in Annotation"
    def __OnNextAnno(self, evt = None):
        if self.element is None:
            return

        self.__GotoNextAnno(self.element)

    def __GotoNextAnno(self, el):
        if not el.HasProperty('LocationString'):
            return

        location_str = el.GetProperty('LocationString')
        results = self.__context.dbhandle.database.location_manager.getLocationInfo(location_str, {})
        if results == self.__context.dbhandle.database.location_manager.LOC_NOT_FOUND:
            return # # @todo Prevent this command from being called without a valid location
        location_id, _, clock = results

        # # @todo Current tick should be based on this element's t_offset.
        cur_tick = self.__context.GetHC()

        results = self.__context.GetTransactionFields(cur_tick,
                                                      location_str,
                                                      ['annotation'])
        cur_annotation = results.get('annotation')
        if cur_annotation is None:
            cur_annotation = ''

        def progress_cb(percent, num_results, info):
            cont = True
            skip = False
            if num_results > 0:
                cont = False # Found some results. No need to keep searching
            return (cont, skip)

        wx.BeginBusyCursor()
        try:
            # # @todo Include location ID in searches
            results = self.__context.searchhandle.Search('string',
                                                         cur_annotation,
                                                         cur_tick + 1, # start tick
                                                         -1, # end tick
                                                         [location_id],
                                                         progress_cb,
                                                         invert = True)
        except Exception as ex:
            error('Error searching: ', ex, file = sys.stderr)
            return # Failed to search
        finally:
            wx.EndBusyCursor()

        closest = None
        for start, end, loc, annotation in results:
            if loc == location_id:
                if annotation != cur_annotation:
                    diff = start - cur_tick
                    if closest is None or (start > cur_tick and start < closest):
                        closest = start

        if closest is not None:
            self.__context.GoToHC(closest)

        if wx.IsBusy():
            wx.EndBusyCursor()

    # # Handle click on the "Previous Change in Annotation"
    def __OnPrevAnno(self, evt = None):
        if self.element is None:
            return

        self.__GotoPrevAnno(self.element)

    def __GotoPrevAnno(self, el):
        if not el.HasProperty('LocationString'):
            return

        location_str = el.GetProperty('LocationString')
        results = self.__context.dbhandle.database.location_manager.getLocationInfo(location_str, {})
        if results == self.__context.dbhandle.database.location_manager.LOC_NOT_FOUND:
            return # # \todo Prevent this command from being called without a valid location
        location_id, _, clock = results

        # # @todo Current tick should be based on this element's t_offset.
        cur_tick = self.__context.GetHC()

        results = self.__context.GetTransactionFields(cur_tick,
                                                      location_str,
                                                      ['annotation'])
        cur_annotation = results.get('annotation')
        if cur_annotation is None:
            cur_annotation = ''

        def progress_cb(self, percent, *args):
            cont = True
            skip = False
            return (cont, skip)

        wx.BeginBusyCursor()
        try:
            results = self.__context.searchhandle.Search('string',
                                                         cur_annotation,
                                                         0, # start tick
                                                         cur_tick - 1, # end tick
                                                         [location_id],
                                                         progress_cb,
                                                         invert = True)
            # TODO Support some kind of reverse search that doesn't require starting from the beginning
        except Exception as ex:
            error(f'Error searching: {ex}')
            return # Failed to search
        finally:
            wx.EndBusyCursor()

        closest = None
        for start, end, loc, annotation in results:
            if loc == location_id:
                if annotation != cur_annotation:
                    if closest is None or (end <= cur_tick and end > closest): # <= cur_tick because end is exclusive
                        closest = end

        if closest is not None:
            self.__context.GoToHC(max(0, closest - 1))

        if wx.IsBusy():
            wx.EndBusyCursor()

    def __AddWatch(self, relative):
        frame = self.__context.GetFrame()
        watch = frame.ShowDialog('watchlist', WatchListDlg)
        t_offset = int(self.element.GetProperty('t_offset'))
        loc = self.element.GetProperty('LocationString')
        watch.Add(loc, t_offset, relative = relative)

    def HandleMouseMove(self, position, canvas, redraw = True):
        '''
        Called when mouse move event happens in correct circumstances.
        hover needs to be enabled and mode needs to not be edit
        '''
        # On a different tick for the same element, force an upate
        force_update = self.__context.GetHC() != self.__last_move_tick
        self.__last_move_tick = self.__context.GetHC()
        (x, y) = canvas.CalcUnscrolledPosition(position)
        hits = self.__context.DetectCollision((x, y), include_subelements = True)
        old_show_state = self.show
        self.show = False
        for hit in hits:
            e = hit.GetElement()
            if e is not None:
                self.show = True
                # As long as we're showing the preview eventually, capture the position.
                # offset our box slightly so pointer doesn't obscure the text
                self.position = (position[0] + 10, position[1] + 10)
                if force_update or self.IsDifferent(e):
                    self.__SetElement(hit)

        is_dirty = old_show_state or self.show
        if is_dirty:
            self.GetWindow().UpdateInfo(self.element, self.annotation, self.__value, self.position)
            if self.__value:
                self.GetWindow().Show(True)
            else:
                self.DestroyWindow()
        elif not self.show:
            self.DestroyWindow()

    def __SetElement(self, pair):
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
            loc_str = e.GetProperty('LocationString')
            has_loc_str = has_loc_str and loc_str != ''
        else:
            loc_str = None

        if annotation and has_loc_str: # If element currently has an annotation to display
            t_offset = e.GetProperty('t_offset')
            self.annotation = annotation
            tick_time = int(t_offset * pair.GetClockPeriod() + self.__context.GetHC())
            if tick_time >= 0:
                # make query

                results = self.__context.GetTransactionFields(tick_time,
                                                              self.element.GetProperty('LocationString'),
                                                              self.__fields)
                intermediate = ''
                for field in list(results.keys()):
                    intermediate += '%s: %s\n' % (field, results[field])
                self.annotation = str(results.get('annotation'))
                # TODO HACK, this hint shouldn't be in the annotation at all, but it is and this is how it gets removed
                # to avoid being shown on screen
                #if self.annotation and all(char in string.hexdigits for char in self.annotation) and self.annotation[3] == ' ':
                #    self.annotation = self.annotation[4:]
                self.SetValue(intermediate[:-1]) # remove extra newline
            else:
                self.SetValue(self.annotation)
        else:
            if e.HasProperty('tooltip') and e.GetProperty('tooltip'):
                self.annotation = e.GetProperty('tooltip')
                self.SetValue(self.annotation)
            elif has_loc_str:
                self.annotation = '<{}>'.format(loc_str)
                self.SetValue(self.annotation)
            elif e.HasProperty('data'): # For node_element
                self.annotation = '<{}>'.format(e.GetProperty('data'))
                self.SetValue(self.annotation)
            elif e.HasProperty('annotation_basis'): # For rpc_element
                self.annotation = annotation
                self.SetValue(self.annotation)
            else:
                # No idea what to print for whatever type of element this is
                self.annotation = repr(e)
                self.SetValue(self.annotation)

    def DestroyWindow(self):
        if self.__window:
            self.__window.Disable()
            self.__window.Show(False)
            self.__window.DestroyLater()
            self.__window = None


class HoverPreviewOptionsDialog(wx.Dialog):
    '''
    Hover Options dialog display and collection code. No really good place to put this.
    '''

    def __init__(self, parent, hover_preview):
        wx.Dialog.__init__(self, parent, wx.NewId(), 'Hover Preview Options', size = (200, 300))

        self.hover_preview = hover_preview
        self.checkOptions = {'start'      : wx.CheckBox(self, wx.NewId(), 'Start'),
                             'end'        : wx.CheckBox(self, wx.NewId(), 'End'),
                             'start_cycle': wx.CheckBox(self, wx.NewId(), 'Start Cycle'),
                             'end_cycle'  : wx.CheckBox(self, wx.NewId(), 'End Cycle'),
                             'transaction': wx.CheckBox(self, wx.NewId(), 'Transaction ID'),
                             'loc'        : wx.CheckBox(self, wx.NewId(), 'Location'),
                             'parent'     : wx.CheckBox(self, wx.NewId(), 'Parent Transaction'),
                             'opcode'     : wx.CheckBox(self, wx.NewId(), 'OP Code'),
                             'vaddr'      : wx.CheckBox(self, wx.NewId(), 'Virtual Address'),
                             'paddr'      : wx.CheckBox(self, wx.NewId(), 'Physical Address'),
                             'clock'      : wx.CheckBox(self, wx.NewId(), 'Clock'),
                             'annotation' : wx.CheckBox(self, wx.NewId(), 'Annotation'),
                             'time'       : wx.CheckBox(self, wx.NewId(), 'Time')}

        sizer = wx.BoxSizer(wx.VERTICAL)
        for value in list(self.checkOptions.values()):
            sizer.Add(value, 1, 0, 0)

        done = wx.Button(self, wx.NewId(), 'Done')

        sizerbuttons = wx.BoxSizer(wx.HORIZONTAL)
        sizerbuttons.Add(done, 2, 0, 0)
        self.__select = wx.Button(self, wx.NewId(), 'Select All')
        sizerbuttons.Add(self.__select, 2, 0, 0)
        sizer.Add(sizerbuttons)
        self.SetSizer(sizer)
        self.Bind(wx.EVT_BUTTON, self.OnSelect, self.__select)
        self.Bind(wx.EVT_BUTTON, self.OnDone, done)
        self.is_all_selected = True

        # set current settings
        self.SetOptions()

    # # Goes through check box elements and appends the checked keys to a list.
    def GetOptions(self):
        checked_options = []
        for key, val in list(self.checkOptions.items()):
            if val.GetValue():
                checked_options.append(key)
        return checked_options

    def SetOptions(self):
        fields = self.hover_preview.GetFields()
        for key, val in list(self.checkOptions.items()):
            if key in fields:
                val.SetValue(True)

    def OnSelect(self, evt):
        if self.is_all_selected:
            self.__select.SetLabel('Deselect')
        else:
            self.__select.SetLabel('Select All')
        for val in list(self.checkOptions.values()):
            val.SetValue(self.is_all_selected)
        self.is_all_selected = not self.is_all_selected

    def OnDone(self, evt):
        self.EndModal(wx.ID_OK)
        self.hover_preview.SetFields(self.GetOptions())
