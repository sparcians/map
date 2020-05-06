

import logging
import os
import wx
import wx.lib.dragscroller as drgscrl
from .layout_canvas import Layout_Canvas
from model.layout import Layout
from .argos_menu import Argos_Menu
from .dialogs.search_dlg import SearchDialog
from .dialogs.find_element_dlg import FindElementDialog
from .dialogs.element_propsdlg import Element_PropsDlg
from .dialogs.location_window import LocationWindow
from .dialogs.layout_exit_dialog import LayoutExitDialog
from .widgets.frame_playback_bar import FramePlaybackBar


# # The GUI-side top-level 'window' which will house a Layout Canvas, MenuBar
#  and Playback Controls. One-to-one mapping with a layout display, and has
#  minimal functionality itself. Primarily provides the hosting frame/window
#  for the Layout Canvas
class Layout_Frame(wx.Frame):
    _DB_UPDATE_DELAY_MS = 10000

    # # Gets the wx frame created, and ties together a Layout Canvas and a
    #  Element Properties Dialog
    def  __init__(self, parent, ws, id, context, update_enabled, title_prefix, title_override, title_suffix):
        self.__workspace = ws
        self.__context = context
        self.__layout = context.GetLayout()
        self.__dialogs = {} # stores currently active dialogs, keyed by name

        size = (800, 600)
        if ws is not None:
            pos = ws.GetNextNewFramePosition(size)
        else:
            pos = wx.DefaultPosition

        self.__title_prefix = title_prefix
        self.__title_override = title_override
        self.__title_suffix = title_suffix

        if title_prefix is not None:
            title = title_prefix
        else:
            title = ''

        if title_override is not None:
            title += title_override
        else:
            title += self.ComputeTitle()

        if title_suffix is not None:
            title += title_suffix

        wx.Frame.__init__(self, parent, id, title, pos = pos, size = size)
        lg = logging.getLogger('Layout_Frame')
        lg.debug('Creating Layout_Frame {} at pos {}, size {}'.format(self, pos, size))

        self.__title = title
        self.__playback_panel = FramePlaybackBar(self, self.__layout)

        # Element properties dialog. It is important never to destroy this until the Frame dies
        self.__dlg = Element_PropsDlg(self, -1, title)
        self.__canvas = Layout_Canvas(self, context, self.__dlg)
        self.__context.Update()

        self.__dlg.SetElements([], self.__canvas.GetSelectionManager())
        if len(self.__layout.GetElements()) > 0:
           self.__dlg.Show(False)

        self.__menu = Argos_Menu(self, self.__layout, update_enabled)
        self.SetMenuBar(self.__menu)
        self.SetToolBar(self.__menu.GetEditToolbar())

        # Enter edit mode if this is a new layout (no associated file on disk)
        if self.__layout.GetFilename() is None:
            self.SetEditMode(True)

        # Layout

        vert_sz = wx.BoxSizer(wx.VERTICAL)
        vert_sz.Add(self.__canvas, 1, wx.EXPAND)
        vert_sz.Add(self.__playback_panel, 0, wx.EXPAND)
        self.SetSizer(vert_sz)

        # Binding

        self.Bind(wx.EVT_CLOSE, self.Close)
        self.Bind(wx.EVT_SIZE, self.__OnResize)

        # Register self with Workspace

        self.__workspace.AddFrame(self)

        # Associated context with this frame AND a group

        self.__context.SetFrame(self)
        self.__context.SetGroup(self.__workspace.GetDefaultGroup())

        # DB update timer
        self.__update_timer = wx.Timer(self, wx.NewId())
        self.Bind(wx.EVT_TIMER, self.CheckDBUpdate)

        if update_enabled:
            self.__EnableDBUpdates()

    def __del__(self):
        for dlgs in list(self.__dialogs.values()):
            for dlg in dlgs:
                dlg.__loc_win.Close()

    def CheckDBUpdate(self, event):
        if self.__context.dbhandle.api.isUpdateReady():
            self.OnDBUpdate()
            self.__context.dbhandle.api.ackUpdate()

    def OnDBUpdate(self, show_wait_cursor = True):
        if show_wait_cursor:
            wait = wx.BusyCursor()
        self.__playback_panel.Refresh()
        self.__context.DBUpdate()
        self.__canvas.FullUpdate()
        # update dialog
        watchdlgs = self.__dialogs.get('watchlist', [])
        for wdlg in watchdlgs:
            if wdlg.IsShown():
                wdlg.Update(self.__context.hc)

        wx.Frame.Refresh(self)
        if show_wait_cursor:
            del wait

    def __EnableDBUpdates(self):
        self.__update_timer.Start(self._DB_UPDATE_DELAY_MS)

    def SetPollMode(self, mode):
        if mode:
            if not self.__update_timer.IsRunning():
                self.Bind(wx.EVT_TIMER, self.CheckDBUpdate)
            self.__EnableDBUpdates()
            self.__context.dbhandle.api.enableUpdate()
        else:
            if self.__update_timer.IsRunning():
                self.__update_timer.Stop()
            self.__context.dbhandle.api.disableUpdate()

    def ForceDBUpdate(self):
        wait = wx.BusyCursor()
        self.__context.dbhandle.api.forceUpdate()
        self.OnDBUpdate(False)
        del wait

    # # Performs a full redraw
    def Refresh(self):
        self.__canvas.FullUpdate()
        self.__playback_panel.Refresh()

        # update dialog
        watchdlgs = self.__dialogs.get('watchlist', [])
        for wdlg in watchdlgs:
            if wdlg.IsShown():
                wdlg.Update(self.__context.hc)

        wx.Frame.Refresh(self)

    # # Returns the workspace owned by this frame
    def GetWorkspace(self):
        return self.__workspace

    # # Returns the Canvas owned by this Layout Frame
    def GetCanvas(self):
        return self.__canvas

    def GetTitlePrefix(self):
        return self.__title_prefix

    def GetTitleOverride(self):
        return self.__title_override

    def GetTitleSuffix(self):
        return self.__title_suffix

    # # Returns the context contained by this Layout Frame
    def GetContext(self):
        return self.__context

    # # Returns the window title
    def GetTitle(self):
        return self.__title

    # # Returns the Playback Panel owned by this Layout Frame
    def GetPlaybackPanel(self):
        return self.__playback_panel

    # # Show a dialog. Shows existing dialog unless create_new=True.
    # # Forwards **kwargs to dialog_class
    def ShowDialog(self, name, dialog_class, create_new = False, **kwargs):
        windows = self.__dialogs.setdefault(name, [])
        if len(windows) == 0 or create_new is True:
            dlg = dialog_class(self, **kwargs)
            self.__dialogs[name].append(dlg)
        else:
            dlg = windows[-1]
        dlg.Show()
        dlg.SetFocus()
        return dlg

    # # Shows the location list dialog
    #  @note Location list does not disappear when close. It is just hidden
    def ShowLocationsList(self):
        self.ShowDialog('locations', LocationWindow, False, elpropsdlg = self.__dlg)

    # # Shows the search dialog
    #  @note Search does not disappear when close. It is just hidden
    #  @param kwargs Interperts and does not foward certain kwargs:
    #  \li location="starting search location"
    def ShowSearch(self, *args, **kwargs):
        if 'location' in kwargs:
            loc = kwargs['location']
            del kwargs['location']
        else:
            loc = None
        dlg = self.ShowDialog('search', SearchDialog, True, *args, **kwargs)
        if loc is not None:
            dlg.SetSearchLocation(loc)

    # # Shows the find-element dialog
    #  @note Search does not disappear when close. It is just hidden
    def ShowFindElement(self, *args, **kwargs):
        dlg = self.ShowDialog('find element', FindElementDialog, False, *args, **kwargs)

    # # Show or hide the navigation controls
    def ShowNavigationControls(self, show = True):
        self.__playback_panel.Show(show)
        # #self.__menu.Show(show) # Menu bar space cannot be reclaimed when hidden, so this is useless. Also might disable hotkeys
        self.Layout()

    # # Attempt to select the given clock by name
    def SetDisplayClock(self, clock_name, error_if_not_found = True):
        return self.__playback_panel.SetDisplayClock(clock_name, error_if_not_found)

    # # To to a specific cyle on the currently displayed clock for this frame
    def GoToCycle(self, cycle):
        assert cycle is not None
        return self.__playback_panel.GoToCycle(cycle)

    # # Handles shutting down both this window and the Element Properties Dialog
    def Close(self, event = None):
        self._HandleClose()

    # # 'Saving' means saving the Layout to file, nothing else is currently
    #  preserved about a session (no user preferences, current selection, HC)
    #  @return True if saved, False if cancelled (because it deferred to SaveAs)
    def Save(self):
        logging.info('Saving')
        if not self.__layout.GetFilename():
            return self.SaveAs()

        if self.__layout.CanSaveToFile():
            self._SaveToFileWithErrorDlg()
            return True

        message = 'The file "{0}" has been modified by another process (or a different ' \
                  'layout instance) since being last written by this Layout. Do you want '\
                  'to overwrite these changes?' \
                  .format(self.__layout.GetFilename())
        dlg = wx.MessageDialog(self, message, "Save Layout - Overwrite Changed File?", wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)
        dlg.ShowModal()
        ret = dlg.GetReturnCode()
        dlg.Destroy()
        if ret == wx.ID_NO:
            return self.SaveAs()

        os.remove(self.__layout.GetFilename())
        self._SaveToFileWithErrorDlg()

        return True

    # # Handle saving the file to a selected path
    #  @return True if saved, False if cancelled
    def SaveAs(self):
        fp = self.__layout.GetFilename()
        logging.info('Saving As. Current="{0}"'.format(fp))
        if fp is not None:
            initial_file = os.path.dirname(os.path.abspath(fp)) + '/' # directory
        else:
            initial_file = os.getcwd() + '/'

        # Loop until user saves or cancels
        dlg = wx.FileDialog(self,
                            "Save As Argos layout file as",
                            defaultFile = initial_file,
                            wildcard = Layout.LAYOUT_FILE_WILDCARD,
                            style = wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT | wx.FD_CHANGE_DIR)
        dlg.ShowModal()
        ret = dlg.GetReturnCode()
        fp = dlg.GetPath()
        dlg.Destroy()

        if ret == wx.ID_CANCEL:
            return False # No save

        # NOTE: File guaranteed not to exisxt by FileDialog
        self._SaveToFileWithErrorDlg(fp)

        self.UpdateTitle() # Title is based on layout filename

        return True

    def _SaveToFileWithErrorDlg(self, filename = None):
        try:
            if filename is not None:
                self.__layout.SaveToFile(filename)
            else:
                filename = self.__layout.GetFilename()
                self.__layout.SaveToFile()
        except Exception as ex:
            print(('Failed to save layout to\n"{}"\n\n{}' \
                      .format(os.path.abspath(filename), ex)))

            dlg = wx.MessageDialog(self,
                                   'Failed to save layout to\n"{}"\n\n{}' \
                                   .format(os.path.abspath(filename), ex),
                                   'Save To File',
                                   wx.OK)
            dlg.ShowModal()
            dlg.Destroy()

    # # Determine if the user frame can be closed and gives the user a chance to
    #  save of discard or cancel if the layout was modified. This does not
    #  close the frame, but must be followed by a _HandleClose(force=True) call
    #  @note This exists so that all layouts can be tested for changes before
    #  closing any of them
    #  @return True if closing is allowed, False if not
    def _PromptBeforeClose(self):
        if not self.__layout.HasChanged():
            return True # Closing is OK

        while self.__canvas.HasCapture():
            self.__canvas.ReleaseMouse()
        message = 'This layout has changes. Are you sure you want to close this frame without saving?'
        # dlg = wx.MessageDialog(self, message, "Close this frame without saving", wx.YES_NO | wx.NO_DEFAULT | wx.ICON_ASTERISK)
        dlg = LayoutExitDialog(self)
        ret = dlg.ShowModal()
        dlg.Destroy()
        if ret == wx.ID_CANCEL:
            return False # Do not exit
        if ret == wx.ID_SAVE:
            # Attempt to save. Do not quit if user failed to save
            return self.Save()
        elif ret == wx.ID_DELETE:
            return True # discard changes
        else:
            raise Exception('Unkonwn result from LayoutExitDialog: {}'.format(ret))

    # # Handles a closing request from this frame or the menu bar.
    #  @param force (kwargs only, default False) Force closed without prompting.
    #  This is dangerous and should only be done when proceeded by a call to
    #  _PromptBeforeClose().
    #
    #  Prompts the user about actually quitting if the layout has changed.
    # # If successful, destroys
    def _HandleClose(self, **kwargs):
        force = False
        for k, v in list(kwargs.items()):
            if k == 'force':
                force = v
            else:
                raise KeyError('Unknown kwargs {}'.format(k))

        # Only prompt about closing if force is False
        if force is False:
            if not self._PromptBeforeClose():
                return

        self.__playback_panel.PausePlaying() # Stop emitting messages which delay the wx.CallAfter
        while len(self.__dialogs) > 0:
            name, dlgs = self.__dialogs.popitem()
            for dlg in dlgs:
                dlg.Destroy()

        if self.__dlg is not None:
            self.__dlg.Destroy()
            self.__dlg = None

        self.__update_timer.Stop()
        self.__context.LeaveGroup()
        wx.CallAfter(self.Destroy) # Delay destruction to ensure that this handler does not refer to this window
        self.__workspace.RemoveFrame(self)

    # # Handle resize events on this frame
    def __OnResize(self, evt):
        # Resize pauses playing because when timer events are too close together there is
        # no chance to refresh the whole frame as is needed.
        self.__playback_panel.PausePlaying()

        evt.Skip()

    # # Computes a good window title containing the database and layout file information
    def ComputeTitle(self):
        title = os.path.split(self.__context.dbhandle.database.filename)[1]
        title += ':'
        lf = self.__context.GetLayout().GetFilename()
        if lf is not None:
            title += os.path.split(lf)[1]
        else:
            title += "<no layout file>";
        return title

    # # Updates the current title based on ComputeTitle
    def UpdateTitle(self):
        self.SetTitle(self.ComputeTitle())

    # # Used for specifying edit mode
    def SetEditMode(self, menuEditBool):
        self.GetCanvas().GetInputDecoder().SetEditMode(menuEditBool, \
                                                       self.__canvas.GetSelectionManager())
        self.__menu.SetEditModeSettings(menuEditBool)
        self.__menu.ShowEditToolbar(menuEditBool)
        # We have to send a resize event to get the toolbar drawn correctly in v2.x
        self.SendSizeEvent()
        # Update the mouse location in the edit bar
        if menuEditBool:
            (x, y) = self.__canvas.GetMousePosition()
            self.UpdateMouseLocation(x, y)

    def SetHoverPreview(self, isHoverPreview):
        self.GetCanvas().GetHoverPreview().Enable(isHoverPreview)

    def SetHoverPreviewFields(self, fields):
        self.GetCanvas().GetHoverPreview().SetFields(fields)

    # # Sets cursor to busy if True
    def SetBusy(self, busy):
        if busy:
            wx.BeginBusyCursor()
        else:
            wx.EndBusyCursor()

    # # focuses the jump-to-time box in the playback panel
    def FocusJumpBox(self):
       self.__playback_panel.FocusJumpBox()

    # # Updates the mouse location in the edit toolbar
    def UpdateMouseLocation(self, x, y):
        self.__menu.UpdateMouseLocation(x, y)

