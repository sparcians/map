from __future__ import annotations
from typing import Any, Dict, List, Optional, Tuple, TYPE_CHECKING
import wx
from gui.font_utils import GetMonospaceFont

if TYPE_CHECKING:
    from gui.layout_canvas import Layout_Canvas
    from model.element import Element

## This class is a GUI list control element that shows elements and allows selection of them.
class ElementList(wx.ListCtrl):
    def __init__(self, parent: wx.Frame, canvas: Layout_Canvas, name: str = '', properties: Optional[List[str]] = None) -> None:
        if properties is None:
            properties = ['element']
        wx.ListCtrl.__init__(self, parent=parent, id=wx.NewId(), name=name, style=wx.LC_REPORT|wx.SUNKEN_BORDER)
        self.SetFont(GetMonospaceFont(canvas.GetSettings().layout_font_size))

        # used for coloring
        self.__canvas = canvas
        # list of dictionary of properties
        self.__elements: List[Dict[str, Any]] = []
        # list of element pointers
        self.__element_ptrs: List[Element] = []
        # properties to show.
        self.__properties = properties[:] #must have at least 1
        #insertion point for elements at end
        self.__current_new_idx = 0
        self.RefreshAll()

    def GetProperties(self) -> Tuple[str, ...]:
        return tuple(self.__properties)

    def SetProperties(self, properties: List[str]) -> None:
        self.__properties = properties[:]
        self.RefreshAll()

    def Clear(self) -> None:
        self.__elements = []
        self.__element_ptrs = []

    ## Destroy all graphical elements and recreate them
    def RefreshAll(self) -> None:
        self.ClearAll()
        # make header
        width =  int(self.GetClientSize()[0]/len(self.__properties))
        if width < 100:
            width = 100
        for col_idx, column in enumerate(self.__properties):
            self.InsertColumn(col_idx, column)
            self.SetColumnWidth(col_idx, width)
        self.__current_new_idx = 0
        for el in self.__elements:
            self.__AddGraphicalElement(el)

    def RefreshElement(self, index: int) -> None:
        element = self.__elements[index]
        for col_idx, prop in enumerate(self.__properties):
            self.SetItem(index, col_idx, str(element.get(prop)))
        color = (255, 255, 255)
        self.SetItemBackgroundColour(index, color)

    def GetElement(self, index: int) -> Element:
        return self.__element_ptrs[index]

    def __AddGraphicalElement(self, element: Dict[str, Any]) -> None:
        self.InsertItem(self.__current_new_idx, str(element.get(self.__properties[0])))
        self.RefreshElement(self.__current_new_idx)
        self.__current_new_idx+=1

    ## Add a new element to bottom of list. New item must be dictionary of properties.
    def Add(self, element_ptr: Element, element_properties: Dict[str, Any]) -> int:
        self.__element_ptrs.append(element_ptr)
        self.__elements.append(element_properties)
        self.__AddGraphicalElement(element_properties)
        return self.__current_new_idx-1

    def Remove(self, index: int) -> None:
        del self.__element_ptrs[index]
        del self.__elements[index]
        self.DeleteItem(index)
        self.__current_new_idx-=1

    ## Attempts to resize the columns based on size
    #  @pre All content should be added before this is called (once)
    def FitColumns(self) -> None:
        for idx,_ in enumerate(self.__properties):
            self.SetColumnWidth(idx, wx.LIST_AUTOSIZE)
        self.Layout()
