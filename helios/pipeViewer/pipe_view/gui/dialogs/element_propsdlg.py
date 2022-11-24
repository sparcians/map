

import time
import wx
import sys

import model.element_types as eltypes
from gui.widgets.element_property_list import ElementPropertyList
from gui.font_utils import ScaleFont

from typing import List, Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from model.element import Element
    from gui.layout_canvas import Layout_Canvas
    from gui.layout_frame import Layout_Frame
    from gui.selection_manager import Selection_Mgr

# # The GUI-side window for editing the properties of an Element
class Element_PropsDlg(wx.Frame):

    # # The constructor
    def __init__(self, parent: Layout_Frame, id: int, title: str) -> None:
        size = (500, 300)
        self.__parent = parent
        wx.Frame.__init__(self, parent, id, "Element Properties Editor", size = size, \
                          style = wx.RESIZE_BORDER | wx.CAPTION | wx.CLOSE_BOX | wx.CLIP_CHILDREN | wx.STAY_ON_TOP)

        # used for temporary display of error messages
        self.__timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.Draw, self.__timer)
        self.SetBackgroundColour("WHITE")
        self.CreateStatusBar()

        self.__fnt_location = wx.Font(ScaleFont(12), wx.NORMAL, wx.NORMAL, wx.NORMAL)
        self.SetFont(self.__fnt_location)

        self.__sizer = wx.BoxSizer(wx.VERTICAL)
        self.__list = ElementPropertyList(self, id)

        self.__sizer.Add(self.__list, 1, wx.EXPAND)
        self.SetSizerAndFit(self.__sizer)
        self.SetAutoLayout(True)

        self.Show(True)
        self.Move((200, 200)) # better than center

        self.Bind(wx.EVT_CLOSE, self.Hide)
        self.Bind(wx.EVT_KEY_DOWN, self.__OnKeyDown)

    # # Since each layout_frame has 1 element_propsdlg and their lifetimes are
    #  tied together, here we override the normal behaviour from clicking the
    #  close button, and simply hide the window from view, while keeping the
    #  object around. The next time the user want to see this dialog, rather
    #  than re-initializing or instantiating a new one, we Show this one and
    #  re-assign one or more Elements to it
    def Hide(self, event: Optional[wx.CommandEvent] = None) -> bool:
        return self.Show(False)

    def GetCanvas(self) -> Layout_Canvas:
        return self.__parent.GetCanvas()

    # # Gotta make sure the StatusBar displays the correct message
    def Draw(self, evt: Optional[wx.TimerEvent] = None) -> None:
        self.__timer.Stop()
        font = self.GetStatusBar().GetFont()
        font.SetWeight(wx.NORMAL)
        self.GetStatusBar().SetFont(font)
        self.GetStatusBar().SetBackgroundColour(wx.WHITE)
        # self.SetStatusText("I don't currently know what Frame I belong to"#fix this later
        self.SetStatusText('{0} elements'.format(self.__list.GetNumberOfElements()))

    def Refresh(self) -> None:
        self.__list.Refresh()
        self.Layout()
        self.Draw()

    # # If the validation of setting an Element property fails / raises an
    #  error, this method will be told to update the status bar accordingly
    def ShowError(self, error: str) -> None:
        self.SetStatusText(str(error))
        font = self.GetStatusBar().GetFont()
        font.SetWeight(wx.BOLD)
        self.GetStatusBar().SetFont(font)
        self.GetStatusBar().SetBackgroundColour(wx.RED)
        self.__timer.Start(2200)

    # # Simple & self-explanatory, but will eventually need to support
    #  multiple selections
    #  @note This must be the only method through which elements on which the dialog
    #  should operate can be set
    def SetElements(self, elements: List[Element], sel_mgr: Selection_Mgr) -> None:
        self.__list.SetElements(elements, sel_mgr)
        self.Fit() # Resize window to fit contents
        # self.__sizer.Layout() # No noticable effect, probably because autolayout is on

    # # Key handler
    def __OnKeyDown(self, evt: wx.KeyEvent) -> None:
        if evt.GetKeyCode() == ord('Z') and evt.ControlDown():
            self.__parent.GetCanvas().GetSelectionManager().Undo()
        elif evt.GetKeyCode() == ord('Y') and evt.ControlDown() and evt.ShiftDown():
            self.__parent.GetCanvas().GetSelectionManager().RedoAll()
        elif evt.GetKeyCode() == ord('Y') and evt.ControlDown():
            self.__parent.GetCanvas().GetSelectionManager().Redo()
        else:
            evt.Skip()


# # Type selection for element.
# Kind of fits in this module. It's small code so I'd rather not give it its own module.
class ElementTypeSelectionDialog(wx.Dialog):

    def __init__(self, parent: Layout_Canvas) -> None:
        wx.Dialog.__init__(self, parent, wx.NewId(), 'Select an Element Type', size = (200, 70))

        self.creatables = list(eltypes.creatables.keys())
        self.__drop_down = wx.ComboBox(self, wx.NewId(), choices = self.creatables, style = wx.TE_PROCESS_ENTER)
        self.__drop_down.SetStringSelection(self.creatables[0]) # assume always one creatable
        self.__drop_down.SetEditable(False)
        done = wx.Button(self, wx.NewId(), 'Done')
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self.__drop_down, 1, 0, 0)
        sizer.Add(done, 1, 0, 0)
        self.SetSizer(sizer)
        self.Bind(wx.EVT_TEXT_ENTER, self.OnDone, self.__drop_down)
        self.Bind(wx.EVT_BUTTON, self.OnDone, done)
        self.__type_string = ''
        self.__drop_down.SetFocus()

    def GetSelection(self) -> str:
        return self.__type_string

    def OnDone(self, evt: wx.CommandEvent) -> None:
        self.__type_string = self.creatables[int(self.__drop_down.GetSelection())]
        # close off selection
        self.EndModal(wx.ID_OK)
