from __future__ import annotations
import wx
from . import key_definitions

from typing import Optional, Tuple, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from wx.lib.dragscroller import DragScroller
    from gui.dialogs.element_propsdlg import Element_PropsDlg
    from gui.hover_preview import HoverPreview
    from gui.layout_canvas import Layout_Canvas
    from gui.selection_manager import Selection_Mgr
    from model.layout_context import Layout_Context

## This class provides a central hub of intelligence for responding to
#  user-events, namely Mouse or Keyboard, & issuing commands to the likes of
#  Canvases, Selection Mgrs, Property Dialogs, etc.
#  Should be a singleton class on the view-side
class Input_Decoder:

    __MOVE_SPEED = 5
    __ACCEL_FACTOR = 2.0

    __ZOOM_MULT = 0.75

    __SHIFT_STEP_DISTANCE = 10
    __CTRL_STEP_DISTANCE = 100

    ## TODO: improve later
    def __init__(self, parent: Layout_Canvas) -> None:
        self.__edit_mode = False
        self.__parent = parent
        self.__context = parent.context
        self.__move_speed = self.__MOVE_SPEED
        self.__accel_factor = self.__ACCEL_FACTOR
        #used to prevent insane quantities of rapid fire copied elementts
        #being added to the layout
        self.__copy_completed = False
        self.__is_traveling = False

    ## Handle scenarios for selecting/deslecting Elements based off of Left
    #  Mouse Down events
    def LeftDown(self,
                 event: wx.MouseEvent,
                 canvas: Layout_Canvas,
                 selection: Selection_Mgr,
                 dialog: Element_PropsDlg) -> None:
        #Quickfix for re-enabling canvas key events after using frame
        #playback controls
        canvas.SetFocus()

        canvas.CaptureMouse()
        event_pos = canvas.ScreenToClient(cast(wx.Window, event.GetEventObject()).ClientToScreen(event.GetPosition()))
        (x,y) = canvas.CalcUnscrolledPosition(event_pos)

        # small routine for playback mode
        if not self.__edit_mode:
            hits = canvas.context.DetectCollision((x,y), include_subelements=True)
            for hit in reversed(hits): # Walk top to bottom layered
                if hit.GetElement().IsSelectable():
                    selection.SetPlaybackSelected(hit)
                    break
            return

        # Edit-mode selection
        hits = canvas.context.DetectCollision((x,y),
                                              include_subelements=False,
                                              include_nondrawables=True)

        #Check to see if user clicked a selection handle to initiate a resize
        #event
        #NOTE: initiating a resize event will short-circuit all other
        #LeftDown logic. Fun!
        resize_evt = selection.HitHandle((x,y))
        if resize_evt:
            selection.BeginCheckpoint('resize elements')
            selection.SetHistory(selection.resize)
            return
        #Sorta like shift-selecting multiple elements in a spreadsheet, or grid
        #NOTE: pressing shift will short-circuit all other LeftDown logic. Fun!
        if event.ShiftDown() and not event.ControlDown() and len(hits) > 0:
            selection.SetHistory(selection.collision)
            top_hit = hits[0].GetElement()
            selection.SetDominant(top_hit)
            top_hit_pos = cast(Tuple[int, int], top_hit.GetProperty('position'))
            top_hit_dim = cast(Tuple[int, int], top_hit.GetProperty('dimensions'))
            exes = [top_hit_pos[0]]
            whys = [top_hit_pos[1]]
            exes.append(top_hit_pos[0]+top_hit_dim[0])
            whys.append(top_hit_pos[1]+top_hit_dim[1])
            hits = canvas.DetectCollision(selection.GetPos())
            for val in hits:
                e = val.GetElement()
                if selection.IsSelected(e):
                    (ex,ey) = cast(Tuple[int, int], e.GetProperty('position'))
                    (w,h) = cast(Tuple[int, int], e.GetProperty('dimensions'))
                    exes.append(ex)
                    whys.append(ey)
                    exes.append(ex+w)
                    whys.append(ey+h)
            #tl = top left; br = bottom right
            tlx = min(exes)
            tly = min(whys)
            brx = max(exes)
            bry = max(whys)
            selection.SetPos((tlx,tly))
            add_these = selection.CalcAddable((brx,bry))
            for el in add_these:
                selection.Add(el)
            #necessary to make the selection permanent, and also prevent it
            #from interfering with future rubber-band operations
            selection.FlushTempRubber()
            #corner case(s)
            #If you don't like the way corner cases are handled, try toggling
            #the variable corner_case below. Hint: shift selecting across
            #layered Elements is weird, period.
            corner_case = 0
            if corner_case == 0:
                #Trust me, it is indeed supposed to be Addable(), not Removable()
                remove_these = selection.CalcAddable((x,y))
                for el in remove_these:
                    selection.Remove(el)
                selection.FlushTempRubber()
                selection.SetPos((x,y))
            elif corner_case == 1:
                selection.Add(top_hit)
            #end corner cases
            selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
            #Keep the return statement! Shift-selecting should short circuit
            #the rest of LeftDown() processing
            return

        #Multiple Elements under mouse click, fun times ensue
        if len(hits) > 1:
            selected_hits = []
            not_selected_hits = []
            #Sort the elements beneath the Mouse click into selected or not
            for pair in hits:
                e = pair.GetElement()
                if selection.IsSelected(e):
                    selected_hits.append(e)
                else:
                    not_selected_hits.append(e)
            #Process what to do
            #Add the next Element
            if event.ControlDown() and not event.ShiftDown():
                selection.SetPos((x,y))
                if len(not_selected_hits)>0:
                    selection.Add(not_selected_hits[-1])
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                    selection.SetHistory(selection.collision)
                    selection.SetDominant(not_selected_hits[-1])
            #Remove the top element
            elif event.ControlDown():
                selection.SetHistory(selection.whitespace)
                if len(selected_hits)>0:
                    selection.Remove(selected_hits[-1])

            else:
                #Maybe we are about to clk-n-drag?
                if len(selected_hits)>0:
                    selection.SetPos((x,y))
                    selection.SetHistory(selection.prep_remove)
                    selection.SetDominant(selected_hits[-1])
                #Else Select just one guy
                else:
                    selection.Clear()
                    selection.SetPos((x,y))
                    selection.SetHistory(selection.collision)
                    ele = hits[-1].GetElement()
                    selection.SetDominant(ele)
                    selection.Add(ele)
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
        #Single Element under mouse click
        elif len(hits) == 1:
            e = hits[0].GetElement()
            if selection.IsSelected(e):
                selection.SetPos((x,y))
                if event.ControlDown() and not event.ShiftDown():
                    selection.SetHistory(selection.collision)
                    selection.SetDominant(e)
                elif event.ControlDown():
                    selection.Remove(e)
                    selection.SetHistory(selection.whitespace)
                    #Show the user immediately that it was removed
                    canvas.FullUpdate()
                else:
                    selection.SetHistory(selection.prep_remove)
                    selection.SetDominant(e)
            else:
                if event.ControlDown() and not event.ShiftDown():
                    selection.SetPos((x,y))
                    selection.Add(e)
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                    selection.SetHistory(selection.collision)
                    selection.SetDominant(e)
                elif event.ControlDown():
                    selection.SetHistory(selection.whitespace)
                else:
                    selection.Clear()
                    selection.SetPos((x,y))
                    selection.Add(e)
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                    selection.SetHistory(selection.collision)
                    selection.SetDominant(e)

            # Selection Manager is responsible for updating the properties dialog

        #No Elements under mouse click
        elif len(hits) == 0:
            selection.SetHistory(selection.rubber_band)
            selection.SetPos((x,y))
            if not (event.ControlDown() and event.ShiftDown()):
                selection.ProcessRubber((x,y), selection.add)
            else:
                selection.ProcessRubber((x,y), selection.remove)

            if not event.ControlDown():
                selection.Clear()

    ## Handle scenarios for selecting/deselecting Elements
    def LeftUp(self,
               event: wx.MouseEvent,
               canvas: Layout_Canvas,
               selection: Selection_Mgr,
               dialog: Element_PropsDlg) -> None:

        # Allow Mouse UP action in non-edit mode

        hist = selection.GetHistory()
        #A history of prep_remove could mean a few things, depending on what
        #the user did between the mouse down which set the history, and the
        #mouse up
        if hist == selection.prep_remove:
            (x,y) = canvas.CalcUnscrolledPosition(event.GetPosition())
            hits = canvas.DetectCollision((x,y))
            #A Selection Mgr's history should only be set to prep-remove
            #during a mouse down over one or more elements, and should be
            #changed to something else by movement before a MouseUp
            if len(hits) > 0:
                selection.SetHistory(selection.collision)
                if len(hits) == 1 or selection.IsSelected(hits[0].GetElement()):
                    selection.Clear()
                    selection.Add(hits[-1].GetElement())
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                else:
                    i = 0
                    while not selection.IsSelected(hits[i+1].GetElement()):
                        i = i+1
                    selection.Clear()
                    selection.Add(hits[i].GetElement())
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
            else:
                print("this is some weird case of the clicks that isn't supposed to be possible...")
                print("you should probably make note of what you just did, and alert Josh")
        #Rubber band op finished, clean up the temp memory
        elif hist == selection.rubber_band:
            selection.FlushTempRubber()
            selection.SetHistory(selection.collision)
            canvas.FullUpdate()
        #Resize op finished, clean up the temp memory
        elif hist == selection.resize:
            selection.FlushQueue()
            selection.CommitCheckpoint()
        elif hist == selection.dragged:
            selection.CommitCheckpoint()
        #Allow the user to click elsewhere, ...
        if not event.AltDown():
            while canvas.HasCapture():
                canvas.ReleaseMouse()
        canvas.FullUpdate()

    ## Set & Show the Element Props Dlg, if double clicked on an Element
    def LeftDouble(self,
                   event: wx.MouseEvent,
                   canvas: Layout_Canvas,
                   selection: Selection_Mgr,
                   dialog: Element_PropsDlg) -> None:

        # Perform no action unless in edit mode
        if not self.__edit_mode:
            return

        if not event.AltDown():
            (x,y) = canvas.CalcUnscrolledPosition(event.GetPosition())
            hits = canvas.DetectCollision((x,y))
            if len(hits) == 1:
                # Selection Manager is responsible for updating the properties dialog
                ##dialog.SetElement(hits[0])
                dialog.Show()
                dialog.SetFocus()
                dialog.Raise()

    ## When the Mouse Right button is pressed,
    def RightDown(self,
                  event: wx.MouseEvent,
                  canvas: Layout_Canvas,
                  drgscrl: DragScroller) -> None:

        # Perform no action unless in edit mode
        if not self.__edit_mode:
            return

        drgscrl.Start(canvas.CalcUnscrolledPosition(event.GetPosition()))

    ## When the Mouse Right button is released,
    def RightUp(self,
                event: wx.MouseEvent,
                canvas: Layout_Canvas,
                drgscrl: DragScroller) -> None:
        if self.__edit_mode:
            drgscrl.Stop()
        canvas.GetHoverPreview().HandleMenuClick(event.GetPosition())

    ## Watch the mouse move, re-draw & update Element positions if the user
    #  is click-and-dragging Elements, or rubber-band-boxing stuff
    def MouseMove(self,
                  event: wx.MouseEvent,
                  canvas: Layout_Canvas,
                  selection: Selection_Mgr,
                  context: Layout_Context,
                  mouse_over_preview: HoverPreview) -> None:
        if self.__edit_mode:
            (x,y) = canvas.CalcUnscrolledPosition(event.GetPosition())

            # Update the mouse location in the edit toolbar if applicable
            if self.GetEditMode():
                canvas.GetFrame().UpdateMouseLocation(x, y)
            if event.LeftIsDown():
                hist = selection.GetHistory()
                if hist == selection.rubber_band:
                    canvas.FullUpdate()
                    if not (event.AltDown()):
                        selection.ProcessRubber((x,y))
                    else:
                        selection.ProcessRubber((x,y))
                elif hist == selection.whitespace:
                    return #Do not drag the selection
                elif hist == selection.collision or hist == selection.dragged or hist == selection.prep_remove:
                    if selection.HasOpenCheckpoint() is False:
                        # Should be transitioning to dragged for the first time, but maybe not
                        selection.BeginCheckpoint('drag elements')
                    selection.Move(pos=(x,y))
                    selection.SetHistory(selection.dragged)
                    canvas.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                    canvas.Refresh()
                elif hist == selection.resize:
                    if event.AltDown():
                        selection.Resize((x,y),mode = 'two')
                    else:
                        selection.Resize((x,y))
            #No click-n-dragging, just waving the mouse around
            else:
                if selection.DetectCollision((x,y)):
                    selection.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
                if selection.HitHandle((x,y)):
                    selection.SetCursor()
        elif mouse_over_preview.IsEnabled():
            mouse_over_preview.HandleMouseMove(event.GetPosition(), canvas)

    def MouseWheel(self, event: wx.MouseEvent, canvas: Layout_Canvas) -> None:
        # canvas scale
        if event.GetModifiers() == wx.MOD_CONTROL:
            rotation = event.GetWheelRotation()/event.GetWheelDelta()
            if rotation > 0:
                factor = 1.05
            else:
                factor = 0.95
            new_scale = canvas.GetScale()*factor
            canvas.SetScale(canvas.GetScale()*factor)
        else:
            event.Skip()

    ## Initiate a scrolling operation on the canvas
    def MiddleDown(self, event: wx.MouseEvent, canvas: Layout_Canvas, selection: Selection_Mgr) -> None:

        # Perform no action unless in edit mode
        if not self.__edit_mode:
            return

        return

    ## End the scrolling operation
    def MiddleUp(self, event: wx.MouseEvent, canvas: Layout_Canvas, selection: Selection_Mgr) -> None:
        return

    ## Handle keystrokes as necessary
    def KeyDown(self,
                event: wx.KeyEvent,
                canvas: Layout_Canvas,
                selection: Selection_Mgr,
                dialog: Element_PropsDlg,
                context: Layout_Context) -> None:
        #Note: KeyCodes are all equivalent to the ASCII values for the
        #corresponding capital character
        key = event.GetKeyCode()
        #print key
        key_handled = False
        # Mode-dependent key mappings
        if not self.__edit_mode:
            # Playback Mode
            if key in key_definitions.STEP_FORWARD:
                if not self.__is_traveling:
                    self.__is_traveling = True
                    layout_frame = self.__parent.GetFrame()
                    if event.ShiftDown():
                        layout_frame.GetPlaybackPanel().StepForward(self.__SHIFT_STEP_DISTANCE) #move multiple spaces
                    else:
                        layout_frame.GetPlaybackPanel().StepForward() #move one space
                    layout_frame.GetPlaybackPanel().StartPlaying(9, delay=1)
                key_handled = True

            elif key in key_definitions.STEP_BACKWARD:
                if not self.__is_traveling:
                    self.__is_traveling = True
                    layout_frame = self.__parent.GetFrame()
                    if event.ShiftDown():
                        layout_frame.GetPlaybackPanel().StepBackward(self.__SHIFT_STEP_DISTANCE) #move multiple spaces
                    else:
                        layout_frame.GetPlaybackPanel().StepBackward() #move one space
                    layout_frame.GetPlaybackPanel().StartPlaying(-9, delay=1)
                key_handled = True

            elif key in key_definitions.STEP_FORWARD_10:
                if not self.__is_traveling:
                    self.__is_traveling = True
                    layout_frame = self.__parent.GetFrame()
                    if event.ControlDown():
                        layout_frame.GetPlaybackPanel().StepForward(self.__CTRL_STEP_DISTANCE) #move (more) multiple spaces
                    else:
                        layout_frame.GetPlaybackPanel().StepForward(self.__SHIFT_STEP_DISTANCE) #move multiple spaces
                    layout_frame.GetPlaybackPanel().StartPlaying(9, delay=1)
                key_handled = True

            elif key in key_definitions.STEP_BACKWARD_10:
                if not self.__is_traveling:
                    self.__is_traveling = True
                    layout_frame = self.__parent.GetFrame()
                    if event.ControlDown():
                        layout_frame.GetPlaybackPanel().StepBackward(self.__CTRL_STEP_DISTANCE) #move (more) multiple spaces
                    else:
                        layout_frame.GetPlaybackPanel().StepBackward(self.__SHIFT_STEP_DISTANCE) #move multiple spaces
                    layout_frame.GetPlaybackPanel().StartPlaying(-9, delay=1)
                key_handled = True

        else: # if not self.__edit_mode:

            # Edit mode
            if key_definitions.isNegativeSelection(key, event):
                canvas.CaptureMouse()
                key_handled = True

            #up-arrow Move Selection
            elif key == key_definitions.MOVE_EL_UP:
                mod = 1.0
                if key_definitions.isSlowMove(event):
                    mod = self.__move_speed
                elif key_definitions.isFastMove(event):
                    mod = 1/self.__accel_factor
                selection.BeginCheckpoint('move element up')
                try:
                    selection.Move(delta=(0, int(-self.__move_speed/mod)))
                finally:
                    selection.CommitCheckpoint()
                key_handled = True

            #down-arrow Move Selection
            elif key == key_definitions.MOVE_EL_DOWN:
                mod = 1.0
                if key_definitions.isSlowMove(event):
                    mod = self.__move_speed
                elif key_definitions.isFastMove(event):
                    mod = 1/self.__accel_factor
                selection.BeginCheckpoint('move element down')
                try:
                    selection.Move(delta=(0, int(self.__move_speed/mod)))
                finally:
                    selection.CommitCheckpoint()
                key_handled = True

            # Left-arrow Move Selection
            elif key == key_definitions.MOVE_EL_LEFT:
                mod = 1.0
                if key_definitions.isSlowMove(event):
                    mod = self.__move_speed
                elif key_definitions.isFastMove(event):
                    mod = 1/self.__accel_factor
                selection.BeginCheckpoint('move element left')
                try:
                    selection.Move(delta=(int(-self.__move_speed/mod), 0))
                finally:
                    selection.CommitCheckpoint()
                key_handled = True

            #right-arrow Move Selection
            elif key == key_definitions.MOVE_EL_RIGHT:
                mod = 1.0
                if key_definitions.isSlowMove(event):
                    mod = self.__move_speed
                elif key_definitions.isFastMove(event):
                    mod = 1/self.__accel_factor
                selection.BeginCheckpoint('move element right')
                try:
                    selection.Move(delta=(int(self.__move_speed/mod), 0))
                finally:
                    selection.CommitCheckpoint()
                key_handled = True

            #Snap all edges to the grid if applicable
            elif key_definitions.isSnapToGrid(key, event):
                selection.SnapCorner()
                key_handled = True

            if key_handled:
                canvas.SetFocus()

        # Mode-independent key mappings

        #Advance Hypercycle
        if key == key_definitions.ADVANCE_HYPERCYCLE:
            context.GoToHC(context.GetHC() + 1)

        #Turn background grid on/off
        elif key_definitions.isToggleBackgroundGrid(key, event):
            canvas.ToggleGrid()

        # jump
        elif key == key_definitions.JUMP_KEY:
            self.__parent.GetFrame().FocusJumpBox()

        # zoom in
        elif key_definitions.isZoomInKey(key, event):
            canvas.SetScale(canvas.GetScale()/self.__ZOOM_MULT)

        # zoom out
        elif key_definitions.isZoomOutKey(key, event):
            canvas.SetScale(canvas.GetScale()*self.__ZOOM_MULT)

        # zoom reset
        elif key_definitions.isZoomResetKey(key, event):
            canvas.SetScale(1.0)

        # next change
        elif key_definitions.isNextChange(key, event):
            canvas.GetHoverPreview().GotoNextChange()

        # previous change
        elif key_definitions.isPrevChange(key, event):
            canvas.GetHoverPreview().GotoPrevChange()

        # otherwise, this event should be handled by someone else
        elif not key_handled:
            event.Skip()


    ## Handle key releases as necessary
    def KeyUp(self,
              event: wx.KeyEvent,
              canvas: Layout_Canvas,
              selection: Selection_Mgr,
              dialog: Element_PropsDlg,
              context: Layout_Context) -> None:
        #Note: KeyCodes are all equivalent to the ASCII values for the
        #corresponding capital character
        key = event.GetKeyCode()
        #Allow another copy event on another key down
        if key == ord('D'):
            selection.PrepNextCopy()
        elif (key in key_definitions.STEP_FORWARD) or \
                (key in key_definitions.STEP_BACKWARD) or \
                (key in key_definitions.STEP_FORWARD_10) or \
                (key in key_definitions.STEP_BACKWARD_10):
            if self.__is_traveling:
                layout_frame = self.__parent.GetFrame()
                layout_frame.GetPlaybackPanel().PausePlaying()
                self.__is_traveling = False

    def Undo(self) -> None:
        self.__parent.GetSelectionManager().Undo()

    def Redo(self) -> None:
        self.__parent.GetSelectionManager().Redo()

    ## Used for specifying edit mode
    #  @param menuEditBool Edit Mode on or off
    #  @param selection SelectionManager instance
    def SetEditMode(self, menuEditBool: bool, selection: Selection_Mgr) -> None:
        self.__edit_mode = menuEditBool
        selection.SetEditMode(menuEditBool)

    ## Returns whether this decoder is in edit mode
    def GetEditMode(self) -> bool:
        return self.__edit_mode

