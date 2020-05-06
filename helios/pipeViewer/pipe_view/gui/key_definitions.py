

import wx

# #This module provides a central place to define how the keys
# are mapped to the interface.

# playback mode
STEP_FORWARD = [wx.WXK_UP, wx.WXK_RIGHT]
STEP_BACKWARD = [wx.WXK_DOWN, wx.WXK_LEFT]
STEP_FORWARD_10 = [wx.WXK_PAGEUP]
STEP_BACKWARD_10 = [wx.WXK_PAGEDOWN]

# edit mode
# #element moving keys
MOVE_EL_UP = wx.WXK_UP
MOVE_EL_DOWN = wx.WXK_DOWN
MOVE_EL_LEFT = wx.WXK_LEFT
MOVE_EL_RIGHT = wx.WXK_RIGHT

# mode independent mappings
ADVANCE_HYPERCYCLE = ord(' ')
JUMP_KEY = ord('G')
FLIP_SELECTION_HORIZ = [wx.WXK_HOME, wx.WXK_END]
FLIP_SELECTION_VERT = [wx.WXK_PAGEDOWN, wx.WXK_PAGEUP]


# #checks if motion of element should be fast
def isFastMove(event):
    return event.ShiftDown() and not event.ControlDown()


def isSlowMove(event):
    return event.ControlDown() and not event.ShiftDown()


# #when selecting objects, negative selection behavior
def isNegativeSelection(key, event):
    if (key == wx.WXK_CONTROL) and event.ShiftDown():
        return True
    elif (key == wx.WXK_SHIFT) and event.ControlDown():
        return True
    return False


def isToggleBackgroundGrid(key, event):
    return key == ord('G') and event.ControlDown()


def isSnapToGrid(key, event):
    return key == ord('G') and event.ShiftDown()


def isZoomInKey(key, event):
    return key == ord('=') and event.ControlDown()


def isZoomOutKey(key, event):
    return key == ord('-') and event.ControlDown()


def isZoomResetKey(key, event):
    return key == ord('0') and event.ControlDown()


# # Goto Next annotation change
def isNextChange(key, event):
    return key == ord('N') and not event.ShiftDown()


# # Goto Prev annotation change
def isPrevChange(key, event):
    return key == ord('N') and event.ShiftDown()

