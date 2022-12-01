

# Selection Mgrs are in charge of drawing to the canvas a demonstration of
# the current selection, hence importing wx
from __future__ import annotations
import os
import traceback
from typing import Any, Callable, Dict, List, Optional, Set, Sequence, Tuple, Union, cast, TYPE_CHECKING

import wx

from model.element import Element
from model.layout_delta import Checkpoint

if TYPE_CHECKING:
    from model.element_value import Element_Value
    from model.layout import Layout
    from gui.dialogs.element_propsdlg import Element_PropsDlg
    from gui.layout_canvas import Layout_Canvas


# This class is responsible for keeping track of which Elements have been
#  'selected' by the user, within a given Layout, as viewed through a Layout
#  Context.
class Selection_Mgr:

    # width of the selection handle boxes
    __c_box_wid = 5

    # Used for determining how far the mouse moved during a resize op.
    __prev_x: Optional[int] = None
    __prev_y: Optional[int] = None

    # For storing the proposed changes to the properties of elements in the
    # selection. 'Queue' as in, 'this information is queued up, and it will
    # all be accounted for at once'
    __queue: Dict[Element, Tuple[int, int, int, int]] = {}

    # Max depth of the undo/redo stack
    #  This is really intended to prevent runaway allocations and should not be
    #  a burden during normal use.
    MAX_UNDO_STACK_SIZE = 200

    SNAP_ON_RESIZE = True

    # Possible sides of a selection/element. Used primarily for alignment
    # states and tracking resize operations. Also used to describe the
    # direction in which to operate, for actions like Indent()
    RIGHT = 4
    BOTTOM = 3
    LEFT = 2
    TOP = 1
    NONE = 0

    # Calculates the actual resize operation, relative to the data in the
    #  queue,  and saves results to the queue
    #  @param pt The current mouse location
    #  @param base The element who owns the selection handle being dragged
    def __TopResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        _, by, _, bh = self.__queue[base]
        assert self.__prev_y is not None
        delta = pt[1] - self.__prev_y
        # Check that the mouse is moving in a direction at a location worth
        # resizing, since due to snapping, the element may already be where
        # the mouse is headed
        if (delta < 0 and by > pt[1]) or (delta > 0 and by < pt[1]):
            mh = self.__queue[self.__min_ht][3]
            mh = mh - delta
            if mh >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    h = h - delta
                    self.__queue[e] = (x, y + delta, w, h)
        self.__prev_y = pt[1]

    # Calculates the actual resize operation, relative to the data in the
    #  queue,  and saves results to the queue
    def __LeftResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        bx, _, bw, bh = self.__queue[base]
        assert self.__prev_x is not None
        delta = pt[0] - self.__prev_x
        if (delta < 0 and bx > pt[0]) or (delta > 0 and bx < pt[0]):
            mw = self.__queue[self.__min_wid][2]
            mw = mw - delta
            if mw >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    w = w - delta
                    self.__queue[e] = (x + delta, y, w, h)
        self.__prev_x = pt[0]

    # Calculates the actual resize operation, relative to the data in the
    #  queue,  and saves results to the queue
    def __RightResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        bx, _, bw, bh = self.__queue[base]
        assert self.__prev_x is not None
        delta = pt[0] - self.__prev_x
        if (delta < 0 and (bx + bw) > pt[0]) or (delta > 0 and (bx + bw) < pt[0]):
            mw = self.__queue[self.__min_wid][2]
            mw = mw + delta
            if mw >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    w = w + delta
                    self.__queue[e] = (x, y, w, h)
        self.__prev_x = pt[0]

    def __BottomResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        '''
        Calculates the actual resize operation, relative to the data in the
        queue,  and saves results to the queue
        '''
        _, by, bw, bh = self.__queue[base]
        assert self.__prev_y is not None
        delta = pt[1] - self.__prev_y
        if (delta < 0 and (by + bh) > pt[1]) or (delta > 0 and (by + bh) < pt[1]):
            mh = self.__queue[self.__min_ht][3]
            mh = mh + delta
            if mh >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    h = h + delta
                    self.__queue[e] = (x, y, w, h)
        self.__prev_y = pt[1]

    # Forwarder method
    def __BottomMidResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResize(pt, base)
        self.SetProperties([self.BOTTOM])

    # Forwarder method
    def __TopMidResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResize(pt, base)
        self.SetProperties([self.TOP])

    # Forwarder method
    def __LeftMidResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__LeftResize(pt, base)
        self.SetProperties([self.LEFT])

    # Forwarder method
    def __RightMidResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__RightResize(pt, base)
        self.SetProperties([self.RIGHT])

    # Forwarder method
    def __TopLeftResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResize(pt, base)
        self.__LeftResize(pt, base)
        self.SetProperties([self.TOP, self.LEFT])

    # Forwarder method
    def __TopRightResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResize(pt, base)
        self.__RightResize(pt, base)
        self.SetProperties([self.TOP, self.RIGHT])

    # Forwarder method
    def __BottomLeftResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResize(pt, base)
        self.__LeftResize(pt, base)
        self.SetProperties([self.BOTTOM, self.LEFT])

    # Forwarder method
    def __BottomRightResize(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResize(pt, base)
        self.__RightResize(pt, base)
        self.SetProperties([self.BOTTOM, self.RIGHT])

    # Look at all the proposed data for Element properties stored in the
    #  __queue, and actually write them to their respective Elements,
    #  modifying them for snapping if applicable. This method is called at
    #  the end of all resize operations
    #  @param sides List of sides which are allowed to snap. Order doesn't matter
    def SetProperties(self, sides: List[int]) -> None:
        grid = self.__canvas.gridsize
        canvasRange = self.__canvas.range
        for e in self.__selected:
            (x, y, w, h) = self.__queue[e]
            # This was determined to be the currently best place of rounding
            # and typcasting, since Element properties can only be ints
            (x, y, w, h) = (int(round(x)), int(round(y)), int(round(w)), int(round(h)))
            (nx, ny, nw, nh) = (x, y, w, h)
            for side in sides:
                if side == self.TOP:
                    if self.SNAP_ON_RESIZE:
                        dy = y % grid
                        # Check if the edge is within snapping canvasRange on either
                        # side of a gridline
                        if dy <= canvasRange:
                            ny = y - dy
                            nh = h + dy
                        elif dy >= grid - canvasRange:
                            ny = y - dy + grid
                            nh = h + dy - grid
                        # Verify that the element hasn't shrunk beyond
                        # allowable limits
                        if h == self.__c_box_wid * 2 or nh <= self.__c_box_wid * 2:
                            nh = self.__c_box_wid * 2
                            ny = y + h - nh

                elif side == self.LEFT:
                    if self.SNAP_ON_RESIZE:
                        dx = x % grid
                        if dx <= canvasRange:
                            nx = x - dx
                            nw = w + dx
                        elif dx >= grid - canvasRange:
                            nx = x - dx + grid
                            nw = w + dx - grid
                        if w == self.__c_box_wid * 2 or nw <= self.__c_box_wid * 2:
                            nw = self.__c_box_wid * 2
                            nx = x + w - nw

                elif side == self.RIGHT:
                    if self.SNAP_ON_RESIZE:
                        dw = (x + w) % grid
                        if dw <= canvasRange:
                            nw = w - dw
                        elif dw >= grid - canvasRange:
                            nw = w - dw + grid
                        if w == self.__c_box_wid * 2 or nw <= self.__c_box_wid * 2:
                            nw = self.__c_box_wid * 2

                elif side == self.BOTTOM:
                    if self.SNAP_ON_RESIZE:
                        dh = (y + h) % grid
                        if dh <= canvasRange:
                            nh = h - dh
                        elif dh >= grid - canvasRange:
                            nh = h - dh + grid
                        if h == self.__c_box_wid * 2 or nh <= self.__c_box_wid * 2:
                            nh = self.__c_box_wid * 2
                # In case anything changed on this iteration, those changes
                # are available to the next iteration
                (x, y, w, h) = (nx, ny, nw, nh)
            # All done verifying / snapping, go ahead and commit the new properties
            e.SetProperty('position', (x, y))
            e.SetProperty('dimensions', (w, h))

    # Delete all the temporary storage of Element properties
    def FlushQueue(self) -> None:
        self.__queue = {}

    # Original resize callbacks, these operations allow overlap
    __RESIZE_OPTIONS_ONE = {
        0:__TopLeftResize,
        1:__BottomLeftResize,
        2:__TopRightResize,
        3:__BottomRightResize,
        4:__TopMidResize,
        5:__BottomMidResize,
        6:__LeftMidResize,
        7:__RightMidResize
        }

    def __TopResizeScaling(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        '''
        Calculates the actual resize operation, relative to the queue, and
        saves results to the queue. This is 'Scale' mode
        @param pt The current mouse location
        @param base The element who owns the selection handle being dragged
        '''
        bx, by, bw, bh = self.__queue[base]
        assert self.__prev_y is not None
        delta = self.__prev_y - pt[1]
        current = self.__bottom - self.__prev_y
        # Check that the mouse is moving in a direction at a location worth
        # resizing, since due to snapping, the element may already be where
        # the mouse is headed
        if (((-delta < 0 and by > pt[1]) or (-delta > 0 and by < pt[1])) and
            current > 0):
            factor = -(delta / current + 1.0)
            mh = self.__queue[self.__min_ht][3]
            mh = int(-mh * factor)
            if mh >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    y = int((self.__bottom - y) * factor + self.__bottom)
                    h = int(-h * factor)
                    h = max(h, self.__c_box_wid * 2)
                    self.__queue[e] = (x, y, w, h)
        self.__prev_y = pt[1]

    # Calculates the actual resize operation, relative to the queue, and
    #  saves results to the queue. This is 'Scale' mode
    def __LeftResizeScaling(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        bx, by, bw, bh = self.__queue[base]
        assert self.__prev_x is not None
        delta = self.__prev_x - pt[0]
        current = self.__right - self.__prev_x
        if (((-delta < 0 and bx > pt[0]) or (-delta > 0 and bx < pt[0])) and
            current > 0):
            factor = -(delta / current + 1.0)
            mw = self.__queue[self.__min_wid][2]
            mw = int(-mw * factor)
            if mw >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    x = int((self.__right - x) * factor + self.__right)
                    w = int(-w * factor)
                    w = max(w, self.__c_box_wid * 2)
                    self.__queue[e] = (x, y, w, h)
        self.__prev_x = pt[0]

    # Calculates the actual resize operation, relative to the queue, and
    #  saves results to the queue. This is 'Scale' mode
    def __RightResizeScaling(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        bx, by, bw, bh = self.__queue[base]
        assert self.__prev_x is not None
        delta = pt[0] - self.__prev_x
        current = self.__prev_x - self.__left
        if (((delta < 0 and (bx + bw) > pt[0]) or (delta > 0 and (bx + bw) < pt[0]))
            and current > 0):
            factor = 1.0 + delta / current
            mw = self.__queue[self.__min_wid][2]
            mw = int(mw * factor)
            if mw >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    x = int((x - self.__left) * factor + self.__left)
                    w = int(w * factor)
                    w = max(w, self.__c_box_wid * 2)
                    self.__queue[e] = (x, y, w, h)
        self.__prev_x = pt[0]

    # Calculates the actual resize operation, relative to the queue, and
    #  saves results to the queue. This is 'Scale' mode
    def __BottomResizeScaling(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        bx, by, bw, bh = self.__queue[base]
        assert self.__prev_y is not None
        delta = pt[1] - self.__prev_y
        current = self.__prev_y - self.__top
        if (((delta < 0 and (by + bh) > pt[1]) or (delta > 0 and (by + bh) < pt[1]))
            and not (current == 0)):
            if current > 0:
                factor = 1.0 + delta / current
            else:
                factor = 1.0 - delta / current
            mh = self.__queue[self.__min_ht][3]
            mh = int(mh * factor)
            if mh >= self.__c_box_wid * 2:
                for e in self.__selected:
                    x, y, w, h = self.__queue[e]
                    h = int(h * factor)
                    y = int((y - self.__top) * factor + self.__top)
                    h = max(h, self.__c_box_wid * 2)
                    self.__queue[e] = (x, y, w, h)
        self.__prev_y = pt[1]

    # Forwarder method
    def __BottomMidResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResizeScaling(pt, base)
        self.SetProperties([self.BOTTOM, self.TOP])

    # Forwarder method
    def __TopMidResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResizeScaling(pt, base)
        self.SetProperties([self.TOP, self.BOTTOM])

    # Forwarder method
    def __LeftMidResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__LeftResizeScaling(pt, base)
        self.SetProperties([self.LEFT, self.RIGHT])

    # Forwarder method
    def __RightMidResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__RightResizeScaling(pt, base)
        self.SetProperties([self.RIGHT, self.LEFT])

    # Forwarder method
    def __TopLeftResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResizeScaling(pt, base)
        self.__LeftResizeScaling(pt, base)
        self.SetProperties([self.TOP, self.LEFT, self.RIGHT, self.BOTTOM])

    # Forwarder method
    def __TopRightResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__TopResizeScaling(pt, base)
        self.__RightResizeScaling(pt, base)
        self.SetProperties([self.TOP, self.RIGHT, self.LEFT, self.BOTTOM])

    # Forwarder method
    def __BottomLeftResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResizeScaling(pt, base)
        self.__LeftResizeScaling(pt, base)
        self.SetProperties([self.BOTTOM, self.LEFT, self.RIGHT, self.TOP])

    # Forwarder method
    def __BottomRightResize2(self, pt: Union[wx.Point, Tuple[int, int]], base: Element) -> None:
        self.__BottomResizeScaling(pt, base)
        self.__RightResizeScaling(pt, base)
        self.SetProperties([self.BOTTOM, self.RIGHT, self.TOP, self.LEFT])

    # This is 'Scaling'-mode resize op callbacks
    __RESIZE_OPTIONS_TWO = {
        0:__TopLeftResize2,
        1:__BottomLeftResize2,
        2:__TopRightResize2,
        3:__BottomRightResize2,
        4:__TopMidResize2,
        5:__BottomMidResize2,
        6:__LeftMidResize2,
        7:__RightMidResize2
        }

    # Prepare for a resize operation on the named side. Get the bounding box
    #  of the selection, find the smallest Element in the corresponding
    #  dimension. Called on MouseDown before the operation actually takes place
    def __TopMidResizePrep(self, base: Element) -> None:
        bh = base.GetYDim()
        by = base.GetYPos()
        self.__top = by
        self.__bottom = by + bh
        self.__min_ht = base
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__queue[e] = (x, y, w, h)
            if h < bh:
                bh = h
                self.__min_ht = e
            if y < self.__top:
                self.__top = y
            if y + h > self.__bottom:
                self.__bottom = y + h

    # Prepare for a resize operation on the named side. Get the bounding box
    #  of the selection, find the smallest Element in the corresponding
    #  dimension. Called on MouseDown before the operation actually takes place
    def __LeftMidResizePrep(self, base: Element) -> None:
        bw = base.GetXDim()
        bx = base.GetXPos()
        self.__min_wid = base
        self.__left = bx
        self.__right = bx + bw
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__queue[e] = (x, y, w, h)
            if w < bw:
                bw = w
                self.__min_wid = e
            if x < self.__left:
                self.__left = x
            if x + w > self.__right:
                self.__right = x + w

    # Prepare for a resize operation on the named side. Get the bounding box
    #  of the selection, find the smallest Element in the corresponding
    #  dimension. Called on MouseDown before the operation actually takes place
    def __RightMidResizePrep(self, base: Element) -> None:
        bw = base.GetXDim()
        bx = base.GetXPos()
        self.__min_wid = base
        self.__left = bx
        self.__right = bx + bw
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__queue[e] = (x, y, w, h)
            if w < bw:
                bw = w
                self.__min_wid = e
            if x < self.__left:
                self.__left = x
            if x + w > self.__right:
                self.__right = x + w

    # Prepare for a resize operation on the named side. Get the bounding box
    #  of the selection, find the smallest Element in the corresponding
    #  dimension. Called on MouseDown before the operation actually takes place
    def __BottomMidResizePrep(self, base: Element) -> None:
        bh = base.GetYDim()
        by = base.GetYPos()
        self.__top = by
        self.__bottom = by + bh
        self.__min_ht = base
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__queue[e] = (x, y, w, h)
            if h < bh:
                bh = h
                self.__min_ht = e
            if y < self.__top:
                self.__top = y
            if y + h > self.__bottom:
                self.__bottom = y + h

    # Forwarder method
    def __TopLeftResizePrep(self, base: Element) -> None:
        self.__TopMidResizePrep(base)
        self.__LeftMidResizePrep(base)

    # Forwarder method
    def __TopRightResizePrep(self, base: Element) -> None:
        self.__TopMidResizePrep(base)
        self.__RightMidResizePrep(base)

    # Forwarder method
    def __BottomRightResizePrep(self, base: Element) -> None:
        self.__BottomMidResizePrep(base)
        self.__RightMidResizePrep(base)

    # Forwarder method
    def __BottomLeftResizePrep(self, base: Element) -> None:
        self.__LeftMidResizePrep(base)
        self.__BottomMidResizePrep(base)

    # Callbacks for pre-resize op preparations
    __RESIZE_PREPARATIONS = {
        0:__TopLeftResizePrep,
        1:__BottomLeftResizePrep,
        2:__TopRightResizePrep,
        3:__BottomRightResizePrep,
        4:__TopMidResizePrep,
        5:__BottomMidResizePrep,
        6:__LeftMidResizePrep,
        7:__RightMidResizePrep
        }

    # It is worth noting that as of right now (8/8/2012) wx.CURSOR_SIZENESW &
    # wx.CURSOR_SIZENWSE don't work in this environment,  so options 0-3 are
    # less-than-ideal standins, till someone custom creates bitmaps for cursors
    __CURSOR_OPTIONS = {
        0:wx.CURSOR_SIZENWSE,
        1:wx.CURSOR_SIZENESW,
        2:wx.CURSOR_SIZENESW,
        3:wx.CURSOR_SIZENWSE,
        4:wx.CURSOR_SIZENS,
        5:wx.CURSOR_SIZENS,
        6:wx.CURSOR_SIZEWE,
        7:wx.CURSOR_SIZEWE,
        }

    # Available modes of rubber-band or lasso selecting
    ADD_TO_SELECTION = 1
    REMOVE_FROM_SELECTION = 2

    # The available history types for tracking. These are all positive numbers
    # so that boolean logic returns 'True' for these values. Mostly set, read,
    # and used by Input Decoder, for mouse operations
    CLICKED_ON_WHITESPACE = 1
    CLICKED_ON_ELEMENT = 2
    RUBBER_BAND_BOX = 3
    PREP_REMOVE = 4
    DRAGGED = 5
    RESIZE = 6

    # Available modes of snapping a selection during a move operation
    DOMINANT_ELEMENT = True
    EACH_ELEMENT_SELECTED = False
    NO_SNAP_ON_MOVE = False

    # Get prepared to start tracking and adjusting user-selected Elements
    #  @param canvas Layout_Canvas
    #  @param elpropsdlg Elements Properties Dialog
    def __init__(self, canvas: Layout_Canvas, elpropsdlg: Element_PropsDlg) -> None:
        self.__canvas = canvas
        self.__el_props_dlg = elpropsdlg
        # The Elements will be stored as the keys to a dict, where the values
        # corresponding to each Element are tuples indicating the offset between
        # a MouseEvt & the Element's top left corner
        self.__selected: Dict[Element, Tuple[int, int]] = {}
        self.__playback_selected: Optional[Element_Value] = None
        self.__is_edit_mode = False
        self.__dominant: Optional[Element] = None
        self.__position = (0, 0)
        self.__history = -1
        self.__resize_direction: Optional[Tuple[Element, int]] = None
        self.__default_cursor = canvas.GetCursor()
        self.__handle_color = (20, 220, 220)
        self.__playback_handle_color = (0, 0, 0)
        self.__alignment = self.NONE
        self.__edge: Optional[int] = None
        # This list is used to make sure that only Elements which were not
        # previously selected, but have been selected since the user began a
        # ctrl-clk&drg rubber-banding operation, are added to the selection
        # during/after the rubber-banding operation
        self.__temp_rubber: List[Element] = []
        # holds the coords of the corner to draw a rubber-band box (should
        # correspond to the mouse position)
        self.__rubberendpt: Optional[Tuple[int, int]] = None
        self.__which_rubber_band = 0
        self.__rubber_color = (20, 220, 220)

        # used to prevent insane quantities of rapid fire copied elements
        # being added to the layout
        self.__copy_completed = False

        # undo/redo stack
        self.__undo_stack: List[Checkpoint] = []
        self.__undo_next_ptr = 0 # Position of next undo checkpoint to allocate

        self.__cur_open_chkpt: Optional[Checkpoint] = None # Current move action checkpoint. To be closed after the move

        self.__undo_redo_hooks: List[Callable] = [] # Hooks for notifying clients about undo/redo.

        this_script_filename = os.path.join(os.getcwd(), __file__)
        nesw_image = wx.Image(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/resize_nesw.png", wx.BITMAP_TYPE_PNG)
        nesw_image.SetOption(wx.IMAGE_OPTION_CUR_HOTSPOT_X, 6)
        nesw_image.SetOption(wx.IMAGE_OPTION_CUR_HOTSPOT_Y, 6)
        self.__custom_cursor_nesw = wx.Cursor(nesw_image)
        nwse_image = wx.Image(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/resize_nwse.png", wx.BITMAP_TYPE_PNG)
        nwse_image.SetOption(wx.IMAGE_OPTION_CUR_HOTSPOT_X, 6)
        nwse_image.SetOption(wx.IMAGE_OPTION_CUR_HOTSPOT_Y, 6)
        self.__custom_cursor_nwse = wx.Cursor(nwse_image)

    # Open and return a checkpoint. The returned checkpoint must be 'after'ed to complete the delta
    #  so that it can be used as an undo/redo delta.
    #  Appends new delta to the undo stack. Removes everything at and after the current next undo
    #  pointer from the undo stack because it cannot be redone following this new change. Maybe an
    #  undo/redo tree could be built, but that is probably unnecessary
    #  @param description Describes the checkpoint
    #  @param force_elements Elements to use as the selection (if not self.__selected.keys())
    def __Checkpoint(self, description: str, force_elements: Optional[List[Element]] = None) -> Checkpoint:
        assert self.__cur_open_chkpt is None, \
              'Cannot start a undo/redo checkpoint before the current move checkpoint is closed'
        selection = list(self.__selected.keys()) if force_elements is None else force_elements
        layout = self.__canvas.context.GetLayout()
        assert layout is not None
        chkpt = Checkpoint(layout, selection, description)

        # Drop the first element if stack is at maximum size
        if len(self.__undo_stack) == self.MAX_UNDO_STACK_SIZE:
            self.__undo_stack = self.__undo_stack[1:]
            if self.__undo_next_ptr > 0:
                self.__undo_next_ptr -= 1

        self.__undo_stack = self.__undo_stack[:self.__undo_next_ptr]
        self.__undo_stack.append(chkpt)
        self.__undo_next_ptr += 1
        assert self.__undo_next_ptr == len(self.__undo_stack), \
               'Checkpoint undo stack next ({}) != size ({})'.format(self.__undo_next_ptr, len(self.__undo_stack))
        return chkpt

    # Add an undo/redo hook to be called when an undo/redo has taken place or
    #  the undo/redo stack has been modified. This can be used to update the
    #  menu or undo/redo lists
    #  @param hook nullary function
    #  @note Hooks should check NumUndos/NumRedos
    def AddUndoRedoHook(self, hook: Callable) -> None:
        self.__undo_redo_hooks.append(hook)

    # Removes a undo/redo hook installed by AddUndoRedoHook
    def RemoveUndoRedoHook(self, hook: Callable) -> None:
        self.__undo_redo_hooks.remove(hook)

    # Invoke all undo/redo hooks installed by AddUndoRedoHook
    def __CallUndoRedoHooks(self) -> None:
        for hook in self.__undo_redo_hooks:
            try:
                hook()
            except Exception as ex:
                print('Exception in Undo/Redo hook:', ex)

    # Undo the next delta
    #  @return True if checkpoint was undone
    def Undo(self) -> None:
        if self.__undo_next_ptr == 0:
            return

        chkpt = self.__undo_stack[self.__undo_next_ptr - 1]
        errors, current = chkpt.remove()
        if errors > 0:
            print('Undo errors: ', errors)
        self.__undo_next_ptr -= 1
        self.ClearSelection()
        self.Add(current) # Adds current elements after removing checkpoint and updates selection/elpropsdlg

        self.__canvas.context.GoToHC()
        self.__canvas.Refresh()

        self.__CallUndoRedoHooks()

    # Redo the next delta
    #  @return True if checkpoint was undone
    def Redo(self) -> None:
        if self.__undo_next_ptr >= len(self.__undo_stack):
            return

        chkpt = self.__undo_stack[self.__undo_next_ptr]
        errors, current = chkpt.apply()
        if errors > 0:
            print('Redo errors: ', errors)
        self.__undo_next_ptr += 1
        self.ClearSelection()
        if current is not None:
            self.Add(current) # Adds current elements after applying checkpoint and updates selection/elpropsdlg

        self.__canvas.context.GoToHC()
        self.__canvas.Refresh()

        self.__CallUndoRedoHooks()

    # Redo all deltas
    #  @return True if checkpoint was undone
    def RedoAll(self) -> None:
        current = None
        while self.__undo_next_ptr < len(self.__undo_stack):
            chkpt = self.__undo_stack[self.__undo_next_ptr]
            errors, current = chkpt.apply()
            if errors > 0:
                print('Redo errors: ', errors)
            self.__undo_next_ptr += 1

            self.ClearSelection()

            if current is not None:
                # current != None implies something was redone
                self.Add(current) # Adds current elements after applying checkpoint and updates selection/elpropsdlg

        self.__canvas.context.GoToHC()
        self.__canvas.Refresh()

        self.__CallUndoRedoHooks()

    # Number of undos remaining before the reaching the bottom of the stack
    def NumUndos(self) -> int:
        return self.__undo_next_ptr

    # Gets the description of the next undo. "" If NumUndos() is 0
    def GetNextUndoDesc(self) -> str:
        if self.NumUndos() == 0:
            return ''
        return self.__undo_stack[self.__undo_next_ptr - 1].description

    # Number of redos remainign before reaching the top of the stack
    def NumRedos(self) -> int:
        return len(self.__undo_stack) - self.__undo_next_ptr

    # Gets the description of the next redo. "" If NumRedos() is 0
    def GetNextRedoDesc(self) -> str:
        if self.NumRedos() == 0:
            return ''
        return self.__undo_stack[self.__undo_next_ptr].description

    # Used to prevent insane quantities of rapid fire copied elements
    #  being added to the layout
    def PrepNextCopy(self) -> None:
        self.__copy_completed = False

    @property
    def each(self) -> bool:
        return self.EACH_ELEMENT_SELECTED

    @property
    def dominant(self) -> bool:
        return self.DOMINANT_ELEMENT

    @property
    def add(self) -> int:
        return self.ADD_TO_SELECTION

    @property
    def remove(self) -> int:
        return self.REMOVE_FROM_SELECTION

    @property
    def dragged(self) -> int:
        return self.DRAGGED

    @property
    def prep_remove(self) -> int:
        return self.PREP_REMOVE

    @property
    def collision(self) -> int:
        return self.CLICKED_ON_ELEMENT

    @property
    def whitespace(self) -> int:
        return self.CLICKED_ON_WHITESPACE

    @property
    def rubber_band(self) -> int:
        return self.RUBBER_BAND_BOX

    @property
    def resize(self) -> int:
        return self.RESIZE

    # Clears the current selection
    def ClearSelection(self) -> None:
        self.SetSelection({})

    # Used for forcing the current selection, instantly disregarding what
    #  was stored previously
    #  @param selected Dictionary of elements {element:[rel_x,rel_y]}. The
    #  values in this dict are implementation details so are not explained here.
    #  @todo This is really an internal-only method and should be made hidden
    def SetSelection(self, selected: Dict[Element, Tuple[int, int]]) -> None:
        assert isinstance(selected, dict)
        self.__selected = selected
        self.__temp_rubber = []
        self.__el_props_dlg.SetElements(list(self.__selected.keys()), self)
        self.__canvas.Refresh()

    # Sets selected object to supplied pair
    def SetPlaybackSelected(self, pair: Optional[Element_Value]) -> None:
        self.__playback_selected = pair

    # Returns selected object pair (None if none selected)
    def GetPlaybackSelected(self) -> Optional[Element_Value]:
        return self.__playback_selected

    # Return the entire selection as a sequence
    def GetSelection(self) -> Dict[Element, Tuple[int, int]]:
        return self.__selected

    #  Toggle whether or not an Element is selected. Used for inveting selection
    #  @param e Element or sequence of elements to toggle
    def Toggle(self, e: Union[Element, Sequence[Element]]) -> None:
        if isinstance(e, Element):
            els = [e]
        else:
            els = list(e)

        to_remove = [e for e in els if self.IsSelected(e)]
        to_add = [e for e in els if not self.IsSelected(e)]

        self.Remove(to_remove)
        self.Add(to_add)

    # Sets whether selection manager should be in edit mode or playback mode
    def SetEditMode(self, is_edit_mode: bool) -> None:
        self.__is_edit_mode = is_edit_mode

    # Set which Element is the 'dominant' one for the selection. i.e. which
    #  is directly beneath the mouse, used for a snapping mode during move operations
    def SetDominant(self, e: Element) -> None:
        self.__dominant = e

    # Used for specifiying which mode of snapping during resize operations
    def SetSnapMode(self, mode: str) -> None:
        # Move  operation snap modes
        if mode == 'mdominant':
            self.DOMINANT_ELEMENT = True
            self.EACH_ELEMENT_SELECTED = False
            self.NO_SNAP_ON_MOVE = False
        elif mode == 'meach':
            self.DOMINANT_ELEMENT = False
            self.EACH_ELEMENT_SELECTED = True
            self.NO_SNAP_ON_MOVE = False
        elif mode == 'freemove':
            self.DOMINANT_ELEMENT = False
            self.EACH_ELEMENT_SELECTED = False
            self.NO_SNAP_ON_MOVE = True
        # Resize operation snap modes
        elif mode == 'reach':
            self.SNAP_ON_RESIZE = True
        elif mode == 'rdominant':
            pass # not implemented
        elif mode == 'freesize':
            self.SNAP_ON_RESIZE = False

    # Snap each side of each selected Element to the grid, if able
    def SnapFill(self) -> None:
        self.FlushQueue()
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__queue[e] = (x, y, w, h)
        self.SetProperties([self.TOP, self.BOTTOM, self.RIGHT, self.LEFT])
        self.__canvas.FullUpdate()

    # Snap the closest corner of each selected Element to the grid, if able
    def SnapCorner(self) -> None:
        self.FlushQueue()
        grid = self.__canvas.gridsize
        range = self.__canvas.range

        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            # Calculate how far away both edges are from the grid in either direction
            dx = x % grid
            dx1 = grid - dx
            dw = (x + w) % grid
            dw1 = grid - dw
            # each tuple is the absolute displacement from a gridline, and the
            # direction to the gridline
            horiz = [(dx, -1), (dx1, 1), (dw, -1), (dw1, 1)]

            # used so that the horiz/vert lists can be sorted by the first value of
            # each tuple
            def extract(tuple: Tuple[int, int]) -> int:
                return tuple[0]

            temp = sorted(horiz, key = extract)[0]
            # whichever edge is closest gets snapped
            x = x + temp[0] * temp[1]
            dy = y % grid
            dy1 = grid - dy
            dh = (y + h) % grid
            dh1 = grid - dh
            vert = [(dy, -1), (dy1, 1), (dh, -1), (dh1, 1)]
            temp = sorted(vert, key = extract)[0]
            y = y + temp[0] * temp[1]
            e.SetProperty('position', (x, y))
        self.__canvas.FullUpdate()

    # Add an Element (or overwrite the associated tuple)
    #  @param e Element or sequence of elements to append to selection
    def Add(self, element: Union[Element, Sequence[Element]]) -> None:
        if isinstance(element, Element):
            els = [element]
        else:
            els = list(element)
        for e in els:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            self.__selected[e] = (x - self.__position[0], y - self.__position[1])

            # Continue aligning, if the selection used to be aligned, and the
            # new guy is on the same edge
            if self.__alignment == self.LEFT:
                if not (x == self.__edge):
                    self.__alignment = self.NONE
            if self.__alignment == self.TOP:
                if not (y == self.__edge):
                    self.__alignment = self.NONE
            if self.__alignment == self.RIGHT:
                if not (x + w == self.__edge):
                    self.__alignment = self.NONE
            if self.__alignment == self.BOTTOM:
                if not (y + h == self.__edge):
                    self.__alignment = self.NONE
        self.__el_props_dlg.SetElements(list(self.__selected.keys()), self)
        self.__canvas.Refresh()

    def IsSelected(self, e: Element) -> bool:
        '''
        Checks whether or not an Element is currently in the selection
        '''
        if (e is not None) and e in self.__selected:
            return True
        return False

    def Remove(self, e: Union[Element, Tuple[Element, ...], Sequence[Element]]) -> None:
        '''
        Remove an Element from the selection, if present
        @param e Element to remove or sequence of elements
        '''
        if isinstance(e, Element):
            els = [e]
        else:
            els = list(e)

        for e in els:
            if self.IsSelected(e):
                del self.__selected[e]

        self.__el_props_dlg.SetElements(list(self.__selected.keys()), self)
        self.__canvas.Refresh()

    def Delete(self, layout: Layout) -> None:
        '''
        Permanently remove all Elements in the selection from the specified layout
        '''
        to_delete = self.__selected
        if to_delete:
            self.ClearSelection()
            self.Add(list(to_delete.keys()))
            self.BeginCheckpoint('delete element')
            for e in self.__selected:
                e.EnableDraw(False)
                layout.RemoveElement(e)
            self.Clear()
            self.CommitCheckpoint() # Nothing selected after checkpoint

    # Generate a brand new Element in the layout, select it
    #  @param add_to_selection Should the new element be added to the
    #  existent selection. If False, element becomes new selection
    #  @return Returns the newly-created element
    def GenerateElement(self, layout: Layout, type_string: str, add_to_selection: bool = False) -> Element:
        self.BeginCheckpoint('create element')
        e = layout.CreateAndAddElement(element_type = type_string)
        mouse: Union[wx.Point, Tuple[int, int]] = wx.GetMousePosition()
        mouse = self.__canvas.CalcUnscrolledPosition(mouse)
        screen = self.__canvas.GetScreenPosition()
        rel_pt = (mouse[0] - screen[0], mouse[1] - screen[1])
        self.SetPos(rel_pt)
        w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
        e.SetProperty('position', (int(rel_pt[0] - w / 2), int(rel_pt[1] - h / 2)))

        # Add to selection, clearing unless asked not to
        if add_to_selection is not True:
            self.Clear()
        self.Add(e)
        self.__canvas.GetFrame().Refresh()
        self.CommitCheckpoint()
        return e

    # Generate a complete copy of the currently selected Elements in the
    #  layout, offset by delta. This operation prevented from happening continuously
    #  @param layout Layout to modify
    #  @param delta Single integer specifying both the x and y coordinate
    #  adjustment of the new element
    def GenerateDuplicateSelection(self, layout: Layout, delta: int) -> None:
        if not self.__copy_completed:
            self.BeginCheckpoint('duplicate element')
            temp_buffer = []
            original_selection = list(self.__selected.keys())
            for e in original_selection:
                ce = layout.CreateAndAddElement(e, element_type = cast(str, e.GetProperty('type')))
                x, y = cast(Tuple[int, int], ce.GetProperty('position'))
                ce.SetProperty('position', (x + delta, y + delta))
                temp_buffer.append(ce)

            self.Clear()
            self.Add(temp_buffer)
            # after all the Elements are added, get their Element Values pairs
            # populated, then refresh the entire frame and prevent an
            # immediately consecutive duplication operation
            self.__canvas.context.GoToHC()
            self.__canvas.GetFrame().Refresh()
            # Requires that PrepNextCopy be called before this will work again
            self.__copy_completed = True

            # Committed checkpoint must know about all elements that still exist
            # from the selection with which the checkpoint was created
            self.CommitCheckpoint(force_elements = temp_buffer + original_selection)

    # Toggles the selection state of each element on the canvas
    def InvertSelection(self, layout: Layout) -> None:
        # Mouse info needed for accurate offsets to store in self.__selected
        # when self.Add() is eventually called
        mouse: Union[wx.Point, Tuple[int, int]] = wx.GetMousePosition()
        mouse = self.__canvas.CalcUnscrolledPosition(mouse)
        screen = self.__canvas.GetScreenPosition()
        rel_pt = (mouse[0] - screen[0], mouse[1] - screen[1])
        self.SetPos(rel_pt)
        self.Toggle(self.__canvas.GetElements())
        self.__canvas.FullUpdate()

    # Empty the current selection. Does not modify the layout.
    #  @note No current implementation for 'undo'
    def Clear(self) -> None:
        self.__selected = {}
        self.__el_props_dlg.SetElements(list(self.__selected.keys()), self)
        self.__canvas.Refresh()

    # Selects every Element on the Canvas/in the Layout. Ctrl-A
    def SelectEntireLayout(self) -> None:
        mouse: Union[wx.Point, Tuple[int, int]] = wx.GetMousePosition()
        mouse = self.__canvas.CalcUnscrolledPosition(mouse)
        screen = self.__canvas.GetScreenPosition()
        rel_pt = (mouse[0] - screen[0], mouse[1] - screen[1])
        self.SetPos(rel_pt)
        self.Add(self.__canvas.GetElements())
        if self.DetectCollision(rel_pt):
            self.SetCursor(wx.Cursor(wx.CURSOR_SIZING))
        self.__canvas.FullUpdate()

    # Begin checkpoint before a move
    #  @note This is separate from Move. It must be explicitly called to ensure
    #  that there are clear stats and ends to undo/redo checkpoint creation.
    #  Implicitly calling this may hide bugs.
    #  @param force_elements (kwarg only) Begins using a specific set of
    #  elements. These elements will be treated as if they were selected
    #  @pre self.__selected must contain all elements which will be affected by
    #  this checkpoint OR force_elements must be used to override. This
    #  obviously excludes any elements created as a result of the checkpoint
    def BeginCheckpoint(self, description: str, **kwargs: Any) -> None:
        assert self.__cur_open_chkpt is None, 'Should not enter BeginCheckpoint with an open checkpoint'
        self.__cur_open_chkpt = self.__Checkpoint(description, kwargs.get('force_elements'))

    # Complete checkpoint after a move
    #  @pre self.__selected must contain all elements which changed as a result
    #  of this checkpoint OR force_elements must be used to override
    #  @param force_elements (kwarg only) Begins using a specific set of
    #  elements. These elements will be treated as if they were selected
    def CommitCheckpoint(self, **kwargs: Any) -> None:
        if self.__cur_open_chkpt is None:
            print('Should not enter CommitCheckpoint without an open checkpoint. This is a bug:')
            print(traceback.format_stack())
            return

        chkpt = self.__cur_open_chkpt
        self.__cur_open_chkpt = None
        selected = kwargs.get('force_elements', self.__selected)
        chkpt.after(selected)
        self.__CallUndoRedoHooks()

    # Cancel the open checkpoint.
    #  @note does not require an open checkpoint. Has no effect if there is no
    #  open checkpoint
    def CancelCheckpoint(self) -> None:
        self.__cur_open_chkpt = None

    # Is there a checkpoint currently open (between BeginCheckpoint and
    #  CommitCheckpoint calls)
    def HasOpenCheckpoint(self) -> bool:
        return self.__cur_open_chkpt is not None

    # Translate the entire selection
    #  @param pos move to given position, usually a Mouse position
    #  @param delta move from current position by the specified offset
    def Move(self,
             pos: Optional[Union[wx.Point, Tuple[int, int]]] = None,
             delta: Optional[Tuple[int, int]] = None,
             **kwargs: Any) -> None:
        force_no_snap = False
        for k, v in kwargs.items():
            if k == 'force_no_snap':
                force_no_snap = v
            else:
                raise KeyError('Invalid argument to Move: {}={}'.format(k, v))

        grid = self.__canvas.gridsize
        range = self.__canvas.range
        # handle the move operation different dependent on the snap-mode
        if (pos is not None):
            if force_no_snap or self.NO_SNAP_ON_MOVE:
                for e in self.__selected:
                    x, y = self.__selected[e]
                    x1 = x + pos[0]
                    y1 = y + pos[1]
                    e.SetProperty('position', (int(x1), int(y1)))
            elif self.EACH_ELEMENT_SELECTED:
                for e in self.__selected:
                    x, y = self.__selected[e]
                    x1 = x + pos[0]
                    y1 = y + pos[1]
                    dx = x1 % grid
                    dy = y1 % grid
                    if dx <= range:
                        x1 = x1 - dx
                    elif dx >= grid - range:
                        x1 = x1 - dx + grid
                    if dy <= range:
                        y1 = y1 - dy
                    elif dy >= grid - range:
                        y1 = y1 - dy + grid
                    e.SetProperty('position', (int(x1), int(y1)))
            elif self.DOMINANT_ELEMENT:
                assert self.__dominant is not None
                x, y = self.__selected[self.__dominant]
                x1 = x + pos[0]
                y1 = y + pos[1]
                dx = x1 % grid
                dy = y1 % grid
                if dx <= range:
                    x1 = x1 - dx
                elif dx >= grid - range:
                    x1 = x1 - dx + grid
                if dy <= range:
                    y1 = y1 - dy
                elif dy >= grid - range:
                    y1 = y1 - dy + grid
                pos = (x1 - x, y1 - y)
                for e in self.__selected:
                    x, y = self.__selected[e]
                    x1 = x + pos[0]
                    y1 = y + pos[1]
                    e.SetProperty('position', (int(x1), int(y1)))
            else:
                raise ValueError('Move called with no snap types set and force_no_snap was not given as kwarg')
        elif delta is not None:
            for e in self.__selected:
                x, y = cast(Tuple[int, int], e.GetProperty('position'))
                e.SetProperty('position', (int(x + delta[0]), int(y + delta[1])))
            self.SetHistory(self.whitespace)
        else:
            raise ValueError('Move called with no position and no delta')

        self.__canvas.FullUpdate()

    # Add spacing between the selected Elements
    #  @param base The element to remain stationary
    #  @param subtractive When true the operation removes spacing
    def Indent(self, base: int, subtractive: bool = False) -> None:
        if not self.__selected:
            return

        self.BeginCheckpoint('indent elements')
        # Are we adding s pace or removing it?
        if subtractive:
            mod = -1
        else:
            mod = 1
        exes, x_sorted, whys, y_sorted, plus_w, w_sorted, plus_h, h_sorted = self.SortByPosition()

        if base == self.TOP or base == self.BOTTOM:
            # add space in the opposite direction, and work the list backwards
            if base == self.BOTTOM:
                whys.reverse()
                mod = -mod
            idx = 0
            # Cannot subtract if they are already aligned
            if len(whys) == 1 and not subtractive:
                y = whys[0]
                for x in exes:
                    for e in x_sorted[x]:
                        e.SetY(y + idx)
                        idx = idx + mod
            # The most normal case
            else:
                for y in whys:
                    for e in y_sorted[y]:
                        e.SetY(y + idx)
                    idx = idx + mod
        # Same math and logic as vertical operations
        elif base == self.RIGHT or base == self.LEFT:
            if base == self.RIGHT:
                exes.reverse()
                mod = -mod
            idx = 0
            if len(exes) == 1 and not subtractive:
                x = exes[0]
                for y in whys:
                    for e in y_sorted[y]:
                        e.SetX(x + idx)
                        idx = idx + mod
            else:
                for x in exes:
                    for e in x_sorted[x]:
                        e.SetX(x + idx)
                    idx = idx + mod
        self.__canvas.Refresh()
        self.CommitCheckpoint()

    # Flip/Reverse the selection in the given direction
    #  @param self.TOP & self.BOTTOM behave identically, same with RIGHT & LEFT
    def Flip(self, direction: int) -> None:
        if not self.__selected:
            return

        self.BeginCheckpoint('flip elements')
        exes, x_sorted, whys, y_sorted, plus_w, w_sorted, plus_h, h_sorted = self.SortByPosition()
        if direction == self.TOP or direction == self.BOTTOM:
            for y in whys:
                for e in y_sorted[y]:
                    e.SetY(plus_h[-1] - y + whys[0] - h_sorted[e])
        elif direction == self.RIGHT or direction == self.LEFT:
            for x in exes:
                for e in x_sorted[x]:
                    e.SetX(plus_w[-1] - x + exes[0] - w_sorted[e])
        self.__canvas.Refresh()
        self.CommitCheckpoint()

    def MoveToTop(self) -> None:
        if self.__selected:
            drawables = [e for e in self.__selected if e.IsDrawable()]
            drawable_pins = set([e.GetPIN() for e in drawables])

            self.BeginCheckpoint('move to top')
            try:
                self.__canvas.context.MoveElementsAbovePINs(drawables, [None])
            except:
                # TODO: other places should follow this protocol
                self.CancelCheckpoint()
                raise
            else:
                self.CommitCheckpoint()
            self.__canvas.FullUpdate()

    def MoveToBottom(self) -> None:
        if not self.__selected:
            return

        drawables = [e for e in self.__selected if e.IsDrawable()]
        drawable_pins = set([e.GetPIN() for e in drawables])

        self.BeginCheckpoint('move to bottom')
        try:
            self.__canvas.context.MoveElementsBelowPINs(drawables, [-1])
        except:
            # TODO: other places should follow this protocol
            self.CancelCheckpoint()
            raise
        else:
            self.CommitCheckpoint()

        self.__canvas.FullUpdate()

    def MoveUp(self) -> None:
        if not self.__selected:
            return

        drawables = [e for e in self.__selected if e.IsDrawable()]
        drawable_pins = set([e.GetPIN() for e in drawables])

        # All pairs - regardless of bounds. Note that no vis-tick filtering is needed because bounds=None
        draw_pairs = [p.GetElement() for p in self.__canvas.context.GetDrawPairs(bounds = None)]

        # Drawables found excluding elements in drawables up to the point where all drawables are
        # accounted for (found==num_drawables).
        # In other words, this is a list of PINS above which the selected elements should be moved
        # as as result of this operation
        above_list = self.__FindPINsUntilMatches(draw_pairs, drawable_pins, None, full_set = True)

        self.BeginCheckpoint('move up')
        try:
            self.__canvas.context.MoveElementsAbovePINs(drawables, above_list)
        except:
            # TODO: other places should follow this protocol
            self.CancelCheckpoint()
            raise
        else:
            self.CommitCheckpoint()
        self.__canvas.FullUpdate()

    def MoveDown(self) -> None:
        if not self.__selected:
            return

        drawables = [e for e in self.__selected if e.IsDrawable()]
        drawable_pins = set([e.GetPIN() for e in drawables])

        # All pairs - regardless of bounds. Note that no vis-tick filtering is needed because bounds=None
        draw_pairs = [p.GetElement() for p in self.__canvas.context.GetDrawPairs(bounds = None)]

        # Drawables found excluding elements in drawables up to the point where all drawables are
        # accounted for (found==num_drawables).
        # In other words, this is a list of PINS above which the selected elements should be moved
        # as as result of this operation
        below_list = self.__FindPINsUntilMatches(draw_pairs, drawable_pins, -1, full_set = False)
        if len(below_list) > 1:
            below_list = below_list[:-1] # Drop last item so that these elements are inserted before it!

        self.BeginCheckpoint('move down')
        try:
            self.__canvas.context.MoveElementsBelowPINs(drawables, below_list)
        except:
            # TODO: other places should follow this protocol
            self.CancelCheckpoint()
            raise
        else:
            self.CommitCheckpoint()
        self.__canvas.FullUpdate()

    # Iterate through a sequence of elements until every PIN (or the first
    #  encountered, depending on the value of full_set) in set_to_match
    #  has been matched with an element pin from the elements list while
    #  appending to a results list each element's PIN from elements which is not
    #  contained within set_to_match. If the end of the elements list is
    #  reached before every element in the elements list is matched to a PIN in
    #  set_to_match, then the result list will have incomplete_set_append
    #  appended to its end
    def __FindPINsUntilMatches(self,
                               elements: List[Element],
                               set_to_match: Set[int],
                               incomplete_set_append: Optional[int],
                               full_set: bool = True) -> List[Optional[int]]:
        num_drawables = len(set_to_match)
        found = 0 # set_to_match items found iterating the list

        results: List[Optional[int]] = []

        for e in elements:
            pin = e.GetPIN()
            if pin in set_to_match:
                if full_set is False:
                    if not results:
                        return [-1]
                    return results # Found first match. Stop here

                # full_set = True
                found += 1 # Found a match in the set. Assume no duplicates in the element sequence
                continue
            else:
                results.append(pin)
                if found == num_drawables:
                    break # Stop iterating now that the element above the last last drawable was found
        else:
            results.append(incomplete_set_append) # Indicates that things must be moved to the end of the list

        return results

    # Take the selection and make every element the average dimensions
    def Average(self) -> None:
        self.BeginCheckpoint('average elements')
        tot_w = 0.0
        tot_h = 0.0
        count = len(self.__selected)
        for e in self.__selected:
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            tot_w = tot_w + w
            tot_h = tot_h + h
        avg_w, avg_h = int(round(tot_w / count)), int(round(tot_h / count))
        for e in self.__selected:
            e.SetProperty('dimensions', (avg_w, avg_h))
        self.CommitCheckpoint()
        self.__canvas.Refresh()

    # Returns a bounding box in the form (l,t,r,b)
    #  If there is no selection, returns None
    def GetBoundingBox(self) -> Optional[Tuple[int, int, int, int]]:
        if not self.__selected:
            return None

        l, t, r, b = (None, None, None, None)
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            if l is None or x < l:
                l = x
            if r is None or x + w > r:
                r = x + w
            if t is None or y < t:
                t = y
            if b is None or y + h > b:
                b = y + h

        assert l is not None
        assert t is not None
        assert r is not None
        assert b is not None
        return (l, t, r, b)

    # Set the current reference position and re-compute the offsets to each
    #  Element in the selection. Usually happens due to a MouseDown evt
    def SetPos(self, position: Tuple[int, int]) -> None:
        self.__position = position
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            self.__selected[e] = (x - self.__position[0], y - self.__position[1])

    # Returns the selection mgr's last recorded position from which to
    #  compute offsets
    def GetPos(self) -> Tuple[int, int]:
        return self.__position

    # Accomplishes the calculating of what is selected by the rubber-band
    #  box and either adds or removes those Elements. Called while processing
    #  a MouseMove evt while history is set to rubber_band
    def ProcessRubber(self, xxx_todo_changeme: Tuple[int, int], action: Optional[int] = None) -> None:
        (x, y) = xxx_todo_changeme
        if action:
            self.__which_rubber_band = action
        self.__rubberendpt = (x, y)
        # Is this operation for adding new Elements?
        if self.__which_rubber_band == self.add:
            add_these = list(self.CalcAddable((x, y)))
            self.Add(add_these)
            # If an element was added earlier during this same operation (no
            # MouseUp evt yet), but is no longer covered by the rubber band,
            # remove it
            for e in self.__temp_rubber:
                if not (e in add_these):
                    self.Remove(e)
            # Keep a temporary listing of which Elements have been added due
            # to this rubber-band operation
            self.__temp_rubber = add_these
        elif self.__which_rubber_band == self.remove:
            remove_these = self.CalcRemovable((x, y))
            self.Remove(remove_these)
            self.Add([e for e in self.__temp_rubber if e not in remove_these])
            self.__temp_rubber = list(remove_these)

    # After the Layout Canvas has done its normal rendering, draw a faint
    #  outline & resize handles on the Elements in a selection
    #  @param dc The device context upon which to draw
    def Draw(self, dc: wx.DC) -> None:
        # Put selection handles on everything currently selected
        (xoff, yoff) = self.__canvas.GetRenderOffsets()
        brush = wx.Brush((255, 255, 255), wx.TRANSPARENT)
        if self.__is_edit_mode:
            pen = wx.Pen(self.__handle_color, 1)
            brush2 = wx.Brush((255, 255, 255), wx.SOLID)
            dc.SetPen(pen)
            for e in self.__selected:
                # Outline each element
                (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
                (x, y) = (x - xoff, y - yoff)
                dc.SetBrush(brush)
                dc.DrawRectangle(int(x), int(y), int(w), int(h))
                c_box_wid = self.__c_box_wid
                corners = [(x - c_box_wid / 2.0, y - c_box_wid / 2.0),
                           (x - c_box_wid / 2.0, y + h - c_box_wid / 2.0),
                           (x + w - c_box_wid / 2.0, y - c_box_wid / 2.0),
                           (x + w - c_box_wid / 2.0, y + h - c_box_wid / 2.0)]
                edges = [(x + w / 2.0 - c_box_wid / 2.0, y - c_box_wid / 2.0),
                           (x + w / 2.0 - c_box_wid / 2.0, y + h - c_box_wid / 2.0),
                           (x - c_box_wid / 2.0, y + h / 2.0 - c_box_wid / 2.0),
                           (x + w - c_box_wid / 2.0, y + h / 2.0 - c_box_wid / 2.0)]
                dc.SetBrush(brush2)
                # Put the selection 'hit' handles around the corners/middles of edges
                for coords in corners:
                    dc.DrawRectangle(int(coords[0]), int(coords[1]), int(c_box_wid), int(c_box_wid))
                for coords in edges:
                    dc.DrawRectangle(int(coords[0]), int(coords[1]), int(c_box_wid), int(c_box_wid))
            # Draw the rubber-band box if applicable
            if self.__history == self.rubber_band:
                assert self.__rubberendpt is not None
                (x, y) = self.__rubberendpt
                (x, y) = (x - xoff, y - yoff)
                pen = wx.Pen(self.__rubber_color, 2, style = wx.LONG_DASH)
                dc.SetPen(pen)
                brush = wx.Brush((255, 255, 255), wx.TRANSPARENT)
                dc.SetBrush(brush)
                top_left = (self.__position[0] - xoff, self.__position[1] - yoff)
                dc.DrawRectangle(int(top_left[0]), int(top_left[1]), int(x - top_left[0]), int(y - top_left[1]))
        elif self.__playback_selected is not None:
            # playback mode has selected object
            e = self.__playback_selected.GetElement()
            pen = wx.Pen(self.__playback_handle_color, 2)
            dc.SetPen(pen)
            dc.SetBrush(brush)
            (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
            (x, y) = (x - xoff, y - yoff)
            dc.DrawRectangle(int(x), int(y), int(w), int(h))

    # For detecting collisions with the resize-handles on elements within
    #  the selection
    def HitHandle(self, pt: Union[wx.Point, Tuple[int, int]]) -> bool:
        if not self.__is_edit_mode:
            return False
        c_box_wid = self.__c_box_wid
        # TODO:: Right now, checking every single
        # element. Inefficient. revise later
        for e in self.__selected:
            (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
            corners = [(x - c_box_wid / 2.0, y - c_box_wid / 2.0),
                       (x - c_box_wid / 2.0, y + h - c_box_wid / 2.0),
                       (x + w - c_box_wid / 2.0, y - c_box_wid / 2.0),
                       (x + w - c_box_wid / 2.0, y + h - c_box_wid / 2.0)]
            edges = [(x + w / 2.0 - c_box_wid / 2.0, y - c_box_wid / 2.0),
                       (x + w / 2.0 - c_box_wid / 2.0, y + h - c_box_wid / 2.0),
                       (x - c_box_wid / 2.0, y + h / 2.0 - c_box_wid / 2.0),
                       (x + w - c_box_wid / 2.0, y + h / 2.0 - c_box_wid / 2.0)]
            tx, ty = pt
            for i in range(4):
                if corners[i][0] < tx < corners[i][0] + c_box_wid and corners[i][1] < ty < corners[i][1] + c_box_wid:
                    # e will become the base element, and i is used for
                    # indexing to the proper callback for the appropriate
                    # resize prep & operations. Reference 'RESIZE_OPTIONS_*'
                    self.__resize_direction = (e, i)
                    self.SetCursor()
                    # deltas for resizing will be determined relative to
                    # previous mouse position, so the mouse position is stored
                    # when it hits a handle, before a mouse-move evt actually
                    # triggers the resize operation. Note, this is separate
                    # from the Selection Mgr's .__position, which has more
                    # 'public' uses
                    self.__prev_x, self.__prev_y = tx, ty
                    # Found a hit!
                    return True
                if edges[i][0] < tx < edges[i][0] + c_box_wid and edges[i][1] < ty < edges[i][1] + c_box_wid:
                    self.__resize_direction = (e, i + 4)
                    self.SetCursor()
                    self.__prev_x, self.__prev_y = tx, ty
                    return True
        self.SetCursor(self.__default_cursor)
        # Nope, mouse not over a selection handle
        return False

    # Set's the history to correspond to the most recent type of event.
    #  @param val should correspond to one of the previously declared options
    def SetHistory(self, val: int) -> None:
        self.__history = val
        if val == self.resize:
            assert self.__resize_direction is not None
            self.__RESIZE_PREPARATIONS[self.__resize_direction[1]](self, self.__resize_direction[0])
            self.__canvas.FullUpdate()

    # Returns the most recent class of event. NOTE: this cannot be used for
    #  redo/undo purposes
    def GetHistory(self) -> int:
        return self.__history

    # Resize the entire selection to match the mouse position. Forwards the
    #  call to one or more actually specific methods with the resize logic
    #  @param pt The current mouse location
    #  @param mode Which of the available implemented modes of resize to do
    def Resize(self, pt: Union[wx.Point, Tuple[int, int]], mode: str = 'one') -> None:
        if mode == 'one':
            assert self.__resize_direction is not None
            self.__RESIZE_OPTIONS_ONE[self.__resize_direction[1]](self, pt, self.__resize_direction[0])
        if mode == 'two':
            assert self.__resize_direction is not None
            self.__RESIZE_OPTIONS_TWO[self.__resize_direction[1]](self, pt, self.__resize_direction[0])
        self.__canvas.FullUpdate()

    # Sets the Canvas' cursor according to what sort of resize option the
    #  mouse is over. @note: cursor options/functionality currently limited 8/23/2012
    def SetCursor(self, cursor: Optional[wx.Cursor] = None) -> None:
        if cursor is None:
            assert self.__resize_direction is not None
            selected_cursor = self.__CURSOR_OPTIONS[self.__resize_direction[1]]
            if selected_cursor == wx.CURSOR_SIZENESW:
                self.__canvas.SetCursor(self.__custom_cursor_nesw)
            elif selected_cursor == wx.CURSOR_SIZENWSE:
                self.__canvas.SetCursor(self.__custom_cursor_nwse)
            else:
                self.__canvas.SetCursor(wx.Cursor(selected_cursor))
        else:
            self.__canvas.SetCursor(cursor)

    # Figure out what Elements are currently within the rubber-band box the
    #  user is drawing that are eligible to be added to the selection
    #  (clk-n-drag)
    #  @param pt Usually the mouse position. The point to use as the opposite
    #  corner from self.__position, in determining the bounding box
    def CalcAddable(self, pt: Union[wx.Point, Tuple[int, int]]) -> Sequence[Element]:
        res = []
        # convert the coords into top left and bottom right
        tlx = min(pt[0], self.__position[0])
        tly = min(pt[1], self.__position[1])
        brx = max(pt[0], self.__position[0])
        bry = max(pt[1], self.__position[1])
        for e in self.__canvas.GetElements():
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            (x1, y1) = (x + w, y + h)
            # Check that the element is completely enclosed by the rubber band
            # box @note: change here for intersection opposed to enclosure
            if (tlx <= x <= brx and tlx <= x1 <= brx and
                tly <= y <= bry and tly <= y1 <= bry):
                # Check that it is either on the temp list for previously
                # captured via rubber band, or else it is not yet selected
                if (e in self.__temp_rubber) or not self.IsSelected(e):
                    res.append(e)
        return res

    # Figure out what Elements are currently within the rubber-band box the
    #  user is drawing that are eligible to be removed from selection (alt
    #  clk-n-drag)
    #  @param pt Usually the mouse position. The point to use as the opposite
    #  corner from self.__position, in determining the bounding box
    def CalcRemovable(self, pt: Union[wx.Point, Tuple[int, int]]) -> Sequence[Element]:
        res = []
        # convert the coords into top left and bottom right
        tlx = min(pt[0], self.__position[0])
        tly = min(pt[1], self.__position[1])
        brx = max(pt[0], self.__position[0])
        bry = max(pt[1], self.__position[1])
        for e in self.__canvas.GetElements():
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            (x1, y1) = (x + w, y + h)
            # Check that the element is completely enclosed by the rubber band
            # box @note: change here for intersection opposed to enclosure
            if (tlx <= x <= brx and tlx <= x1 <= brx and
                tly <= y <= bry and tly <= y1 <= bry):
                # Check that it is either on the temp list for previously
                # captured via rubber band, or else it is currently selected
                if (e in self.__temp_rubber) or self.IsSelected(e):
                    res.append(e)
        return res

    # Returns lists and dictionaries of positions and elements in the
    #  selection, organized by their positions (and dimensions). Align,
    #  Indent, Flip, and Stack make use of this
    def SortByPosition(self) -> Tuple[List[int], Dict[int, List[Element]], List[int], Dict[int, List[Element]], List[int], Dict[Element, int], List[int], Dict[Element, int]]:
        assert self.__selected
        exes = [] # A list of the unique x-coords for Element positions, incrs.
        x_sorted: Dict[int, List[Element]] = {} # Key = x-coord; Val = [Element,Element, ...]
        whys = [] # A list of the unique y-coords for Element positions, incrs.
        y_sorted: Dict[int, List[Element]] = {} # Key = y-coord; Val = [Element,Element, ...]
        plus_w = []
        w_sorted = {} # Key = Element; Val = width (no extra calls  necessary)
        plus_h = []
        h_sorted = {} # Key = Element; Val = height (no extra calls)
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            if not (x in exes):
                exes.append(x)
            temp = x_sorted.get(x, [])
            temp.append(e)
            x_sorted[x] = temp
            if not (y in whys):
                whys.append(y)
            temp = y_sorted.get(y, [])
            temp.append(e)
            y_sorted[y] = temp
            plus_w.append(x + w)
            w_sorted[e] = w
            plus_h.append(y + h)
            h_sorted[e] = h
        exes.sort() # Put 'em all in increasing order
        whys.sort()
        plus_h.sort()
        plus_w.sort()
        return exes, x_sorted, whys, y_sorted, plus_w, w_sorted, plus_h, h_sorted

    # Align the given edge of every Element in the selection to the
    #  corresponding edge of the Element currently located farthest in that
    #  direction
    #  @param direction The side of each Element to align
    def Align(self, direction: str) -> None:
        if not self.__selected:
            return

        self.BeginCheckpoint('align elements')
        exes, x_sorted, whys, y_sorted, plus_w, w_sorted, plus_h, h_sorted = self.SortByPosition()
        if direction == 'left':
            for e in self.__selected:
                e.SetProperty('position', (exes[0], e.GetYPos()))
            self.__alignment = self.LEFT
            self.__edge = exes[0]
        elif direction == 'top':
            for e in self.__selected:
                e.SetProperty('position', (e.GetXPos(), whys[0]))
            self.__alignment = self.TOP
            self.__edge = whys[0]
        elif direction == 'bottom':
            for e in self.__selected:
                e.SetProperty('position', (e.GetXPos(), plus_h[-1] - h_sorted[e]))
            self.__alignment = self.BOTTOM
            self.__edge = plus_h[-1]
        elif direction == 'right':
            for e in self.__selected:
                e.SetProperty('position', (plus_w[-1] - w_sorted[e], e.GetYPos()))
            self.__alignment = self.RIGHT
            self.__edge = plus_w[-1]
        self.__canvas.FullUpdate()
        self.CommitCheckpoint()

    # Line the selection up in a row/column behind the element located
    #  farthest in the given direction
    #  @param direction The element located farthest in this direction
    #  becomes the stationary base of the 'stack' which will progress in the
    #  opposite direction
    def Stack(self, direction: str) -> None:
        if len(self.__selected) == 0:
            return

        self.BeginCheckpoint('stack elements')
        exes, x_sorted, whys, y_sorted, plus_w, w_sorted, plus_h, h_sorted = self.SortByPosition()
        # Stack from left
        if direction == 'left':
            bx = exes[0]
            base = x_sorted[bx][0]
            by = base.GetYPos()
            for x in exes:
                # Used in the event that the selection was already aligned
                # to the left edge
                if len(x_sorted[x]) > 1:
                    temp: Dict[int, List[Element]] = {}
                    subwhys = []
                    for e in x_sorted[x]:
                        x, y = cast(Tuple[int, int], e.GetProperty('position'))
                        # Vertical position is the secondary level of
                        # sorting, when precedence based on horizontal
                        # position fails
                        if not (y in subwhys):
                            subwhys.append(y)
                        subtemp = temp.get(y, [])
                        subtemp.append(e)
                        temp[y] = subtemp
                    subwhys.sort()
                    for y in subwhys:
                        for e in temp[y]:
                            e.SetProperty('position', (bx, by))
                            bx = bx + w_sorted[e]
                # 'Original' case. Stack them by preserving their
                # horizontal order
                else:
                    e = x_sorted[x][0]
                    e.SetProperty('position', (bx, by))
                    bx = bx + w_sorted[e]
            # Make note that the selection is currently aligned
            self.__alignment = self.TOP
            self.__edge = by
        # Stack from right
        elif direction == 'right':
            bx = plus_w[-1]
            exes.reverse()
            base = x_sorted[exes[0]][0]
            by = base.GetYPos()
            for x in exes:
                if len(x_sorted[x]) > 1:
                    temp = {}
                    subwhys = []
                    for e in x_sorted[x]:
                        x, y = cast(Tuple[int, int], e.GetProperty('position'))
                        if not (y in subwhys):
                            subwhys.append(y)
                        subtemp = temp.get(y, [])
                        subtemp.append(e)
                        temp[y] = subtemp
                    subwhys.sort()
                    for y in subwhys:
                        for e in temp[y]:
                            e.SetProperty('position', (bx - w_sorted[e], by))
                            bx = bx - w_sorted[e]
                else:
                    e = x_sorted[x][0]
                    e.SetProperty('position', (bx - w_sorted[e], by))
                    bx = bx - w_sorted[e]
            self.__alignment = self.TOP
            self.__edge = by
        # Stack from top
        elif direction == 'top':
            by = whys[0]
            base = y_sorted[by][0]
            bx = base.GetXPos()
            for y in whys:
                if len(y_sorted[y]) > 1:
                    temp = {}
                    subexes = []
                    for e in y_sorted[y]:
                        x, y = cast(Tuple[int, int], e.GetProperty('position'))
                        if not (x in subexes):
                            subexes.append(x)
                        subtemp = temp.get(x, [])
                        subtemp.append(e)
                        temp[x] = subtemp
                    subexes.sort()
                    for x in subexes:
                        for e in temp[x]:
                            e.SetProperty('position', (bx, by))
                            by = by + h_sorted[e]
                else:
                    e = y_sorted[y][0]
                    e.SetProperty('position', (bx, by))
                    by = by + h_sorted[e]
            self.__alignment = self.LEFT
            self.__edge = bx
        # Stack from bottom
        elif direction == 'bottom':
            by = plus_h[-1]
            whys.reverse()
            base = y_sorted[whys[0]][0]
            bx = base.GetXPos()
            for y in whys:
                if len(y_sorted[y]) > 1:
                    temp = {}
                    subexes = []
                    for e in y_sorted[y]:
                        x, y = cast(Tuple[int, int], e.GetProperty('position'))
                        if not (x in subexes):
                            subexes.append(x)
                        subtemp = temp.get(x, [])
                        subtemp.append(e)
                        temp[x] = subtemp
                    subexes.sort()
                    for x in subexes:
                        for e in temp[x]:
                            e.SetProperty('position', (bx, by - h_sorted[e]))
                            by = by - h_sorted[e]
                else:
                    e = y_sorted[y][0]
                    e.SetProperty('position', (bx, by - h_sorted[e]))
                    by = by - h_sorted[e]
            self.__alignment = self.LEFT
            self.__edge = bx
        self.__canvas.FullUpdate()
        self.CommitCheckpoint()

    # This should be called on mouse ups in order to make sure that the
    #  Elements selected by rubber-band operation stay selected
    def FlushTempRubber(self) -> None:
        self.__temp_rubber = []

    # Returns true if there is a selected Element beneath the given point
    def DetectCollision(self, pt: Union[wx.Point, Tuple[int, int]]) -> bool:
        mx, my = pt
        for e in self.__selected:
            x, y = cast(Tuple[int, int], e.GetProperty('position'))
            w, h = cast(Tuple[int, int], e.GetProperty('dimensions'))
            if x <= mx <= (x + w) and y <= my <= (y + h):
                return True
        return False

    # debug purposes only
    def __str__(self) -> str:
        return str(self.__selected)
