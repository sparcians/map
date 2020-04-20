

## @package workspace.py
#  @brief Contains the workspace class which is central to an argos session

import os
import sys
import weakref
import logging
import wx

from . import group
from .layout import Layout
from model.layout_context import Layout_Context
from gui.layout_frame import Layout_Frame
import gui.autocoloring

## Responsible for managing Databases, LayoutContexts & and
#  Groups, the building blocks for the model-side of the application.
class Workspace:

    WINDOW_CASCADE_STEP_X = 20
    WINDOW_CASCADE_STEP_Y = 20

    ## Constructor
    def __init__(self):
        ##self.__layouts = [] # Managed layouts
        ##self.__laycons = [] # Managed layout contexts
        ##self.__dbs = [] # Managed databases
        self.__groups = [group.Group()] # Synchronization groups
        self.__frames = [] # Weak references to frames
        self.__color_palette = 'default'
        self.__color_shuffle_state = 'default'

        # Figure out a starting window pos
        pos = [0,0]
        self.__primary_window_size = None
        for i in range(wx.Display.GetCount()):
            d = wx.Display(i)
            if d.IsPrimary():
                self.__primary_window_size = d.Geometry.Size[:2]
                pos = d.Geometry.Position[:2]
                break

        self.__primary_window_coords = tuple(pos)
        self.__last_window_pos = None

        # User resource dirs
        self.__user_resource_dirs = []

        # Builtin resource dir
        this_script_dir = os.path.dirname(os.path.join(os.getcwd(), __file__))
        self.__builtin_resource_dir = os.path.join(os.path.dirname(this_script_dir), 'resources')

    def __str__(self):
        return '<Argos Workspace>'

    def __repr__(self):
        return self.__str__()

    ## Opens a new layout frame
    #  @param lf Layout filename. If '' or None, opens blank layout
    #  @param db Database to link this layout frame to through a layout context
    #  @param start_tick Startup tick. If None, default is used
    #  @param loc_vars Location variable values. May be None or a dictionary
    #  @param norefresh (kwarg only, default False) If True, do not show or
    #  refresh the frame/layout. This is used if the client wants to resize or
    #  change current time before rendering the layoutframe
    #  @return frame created
    def OpenLayoutFrame(self, lf, db, start_tick, update_enabled, title_prefix, title_override, title_suffix, loc_vars, **kwargs):
        norefresh = False
        for k,v in kwargs.items():
            if k == 'norefresh':
                norefresh = True
            else:
                raise KeyError('Unknown kwargs {}'.format(k))

        if lf is None:
            lf = ''
        layout = Layout(lf, workspace=self) # If '', no layout will be loaded (as the user intended)

        lc = Layout_Context(layout, db, start_tick, loc_vars) # Jump to startup tick on construction

        frame = Layout_Frame(None, self, -1, lc, update_enabled, title_prefix, title_override, title_suffix)

        if not norefresh:
            frame.Show(True)
            lc.RefreshFrame() # Updates meta-data so that everything is good on the first rendering

        return frame

    def AddFrame(self, frame):
        self.__frames.append(weakref.ref(frame, self.__RemoveFrame))

    # Set the palette for this workspace
    def SetPalette(self, palette):
        self.__color_palette = palette
        # The autocoloring module is global, so we can set the mode once and then update all of the canvas brushes
        gui.autocoloring.SetPalettes(palette)
        for frame in self.__frames:
            frame().GetCanvas().RefreshBrushes()

    # Set the color shuffle state for this workspace
    def SetColorShuffleState(self, state):
        self.__color_shuffle_state = state
        # The autocoloring module is global, so we can set the mode once and then update all of the canvas brushes
        gui.autocoloring.SetShuffleModes(state)
        for frame in self.__frames:
            frame().GetCanvas().RefreshBrushes()

    # Get the palette for this workspace
    def GetPalette(self):
        return self.__color_palette

    # Get the color shuffle state for this workspace
    def GetColorShuffleState(self):
        return self.__color_shuffle_state

    def AddGroup(self):
        # not in first cut
        return

    def GetGroup(self):
        return

    def GetGroups(self):
        return self.__groups[:]

    def GetDefaultGroup(self):
        assert len(self.__groups) > 0, \
               'Workspace should never have fewer than 1 sync group'
        return self.__groups[0]

    def NewLayoutContext(self):
        # doit
        return

    def OpenDatabase(self):
        return

    def CloseDatabase(self):
        return

    def AddUserResourceDir(self, dirname):
        self.__user_resource_dirs.append(os.path.join(os.getcwd(), os.path.expanduser(dirname)))

    def GetUserResourceDirs(self):
        return self.__user_resource_dirs[:]

    def GetBuiltinResourceDirs(self):
        return [self.__builtin_resource_dir]

    ## Get all resources in the proper resolution order
    def GetResourceResolutionList(self):
        return self.GetBuiltinResourceDirs() + self.GetUserResourceDirs()

    ## Resolve a resource by its filename in all known resource directories in
    #  the proper resolution order
    #  @param filename Filename to find in resource dirs. If absolute path, does
    #  not check resource dirs
    #  @param ignore Single string or a sequence of stringsd. Should some
    #  resolved resource paths be skipped? If this was invoked and the file path
    #  it returns was not a valid file or could not be opened, then this method
    #  can be re-invoked with that path (or a sequence of paths) in order to
    #  ignore them. This is done by comparing canonical paths
    #  @return Absolute path of a resource specified by filename in some
    #  resource dir to exist. A string will be returned only if the filed it
    #  points to exists (it maybe not be the correct resource though). Otherwise
    #  returns None to indicate a failed resolution.
    def LocateResource(self, filename, ignore=[], try_first=None):
        if isinstance(ignore, str):
            ignore = [ignore]

        if os.path.isabs(filename):
            for igp in ignore:
                if os.path.realpath(igp) == os.path.filename:
                    return None # Caller ignored their own input absolute path.
            if os.path.exists(filename):
                return filename
            return None

        rds = []
        if isinstance(try_first, (list, tuple)):
            rds.extend(try_first)
        elif try_first is not None:
            rds.append(try_first)
        rds.extend(self.GetResourceResolutionList())

        for d in rds:
            fullpath = os.path.join(d, filename)

            # Ignore it?
            skip = False
            for igp in ignore:
                if os.path.realpath(igp) == os.path.filename:
                    skip = True # Caller ignored this path
            if skip:
                continue # Skip this iteration

            if os.path.exists(fullpath):
                return fullpath

        return None

    # Exit all of argos by closing all frames
    def Exit(self):
        for idx,frame in enumerate(self.__frames):
            frame = frame() # weakref deref
            if frame is not None:
                try:
                    if not frame._PromptBeforeClose():
                        return # About exit
                except wx.PyDeadObjectError as ex:
                    print('Failed to access workspace frame idx {}: {}. It was already destroyed'.format(idx, frame), file=sys.stderr)

        # Now that all layouts had a chance to save, discard, or cancel, force
        # All close to avoid prompting again
        while len(self.__frames) > 0:
            framewr = self.__frames[0]
            frame = framewr() # weakref deref
            if frame:
                try:
                    frame._HandleClose(force=True)
                except wx.PyDeadObjectError as ex:
                    print('Failed to access {}. It was already destroyed'.format(frame), file=sys.stderr)

            # Should have been removed by _HandleClose. If it wasn't (due to exception or other bug)
            # remove it anyway
            if framewr in self.__frames:
                self.__frames.remove(framewr)

    ## Returns a 2-tuple window position based on the existing windows
    def GetNextNewFramePosition(self, size):
        lg = logging.getLogger('Workspace')
        if self.__last_window_pos is not None:
            # Selet position based on previous position, with an x and y offset
            lg.debug('Getting New Frame Position based on last pos')
            pos = self.__last_window_pos
            pos = (pos[0] + self.WINDOW_CASCADE_STEP_X, \
                   pos[1] + self.WINDOW_CASCADE_STEP_Y)
            self.__last_window_pos = pos
            return pos
        elif self.__primary_window_size is not None:
            # Determine window pos so it is centered on screen
            lg.debug('Getting New Frame Position based on screen geometry')
            pos = (self.__primary_window_coords[0] + (self.__primary_window_size[0]/2 - size[0]/2), \
                   self.__primary_window_coords[1] + (self.__primary_window_size[1]/2 - size[1]/2))
            ## \todo Ensure that this is actually ON the screen. Should also adjust if the window is
            #  largely off the screen
            self.__last_window_pos = pos
            return pos
        else:
            # Just pick something since we don't have geometry
            lg.debug('Getting New Frame Position using wx.DefaultPosition')
            return wx.DefaultPosition

    ## Remove a frame given its weak pointer
    def __RemoveFrame(self, frame):
        assert isinstance(frame, weakref.ref)
        assert frame in self.__frames
        self.__frames.remove(frame)

    ## Remove a frame given its reference. This is meant to be called by Frames
    #  themselves, but could be used for inter-workspace frame management
    #  @return True if the frame was removed, False if not
    def RemoveFrame(self, frame):
        for fwr in self.__frames:
            if fwr() == frame:
                self.__RemoveFrame(fwr)
                return True

        return False
