
from __future__ import annotations
import wx
import os
import logging
import sys
from typing import Dict, List, Optional, TYPE_CHECKING

import wx.lib.mixins.listctrl  as  listmix
from gui.font_utils import ScaleFont

if TYPE_CHECKING:
    from model.layout_context import Layout_Context


# # A helper class for building a list of variables and values in separate columns
class VarsListCtrl(wx.ListCtrl,
                   listmix.ListCtrlAutoWidthMixin,
                   listmix.TextEditMixin):

    # # The Constructor
    #  Calls Populate() which actually sets up the ListCtrl
    def __init__(self, parent: LayoutVariablesDialog, ID: int, variables: Optional[Dict[str, str]] = None) -> None:
        wx.ListCtrl.__init__(self,
                             parent,
                             ID,
                             style = wx.LC_REPORT | wx.BORDER_NONE | wx.LC_SORT_ASCENDING)

        self.__parent = parent
        listmix.ListCtrlAutoWidthMixin.__init__(self)
        self.__vars = variables if variables is not None else {}
        self.__keys: List[str] = [] # index in list maps to variable name
        self.__is_init = False
        self.Populate()
        listmix.TextEditMixin.__init__(self)

    # # Convenience method for forwarding calls to Populate(), which clears
    #  and reconstitutes the table with up-to-date values
    def Refresh(self) -> None:
        self.Populate()

    # # Returns the number of items in the dialog
    def GetNumItems(self) -> int:
        return self.GetItemCount()

    # # Allows users to edit the second column (values)
    def OpenEditor(self, col: int, row: int) -> None:
        if col == 1:
            index = row
            super().OpenEditor(col, row)

    # # Get's called when something is edited in the ListCtrl (in the GUI
    #  window, by the user)
    def SetItem(self, index: int, col: int, data: str, imageId: int = -1) -> bool:
        # hopefully whatever the user input is valid... (data will pass
        # through the validation steps on the Element side)
        if not self.__is_init:
            var = self.__keys[index]
            self.__vars[var] = data

        # go ahead and update the text fields on display and redraw the canvas
        retcode = self.__UpdateItem(index)

        if not self.__is_init:
            old_cursor = self.__parent.GetCursor()
            new_cursor = wx.Cursor(wx.CURSOR_WAIT)
            wx.SetCursor(new_cursor)
            wx.SafeYield()

            try:
                # self.__parent.GetContext().ReValueAll()
                self.__parent.GetContext().ReSortAll()
                self.__parent.GetContext().GoToHC()
                self.__parent.GetContext().RefreshFrame()
            except Exception as ex:
                raise ex
            finally:
                wx.SetCursor(old_cursor)
                wx.SafeYield()
        return retcode

    # # Used for creating the columns and rows and dumping in initial values
    #  (or current values every time the user changes what Element is being edited)
    def Populate(self) -> None:
        # for normal, simple columns, you can add them like this:
        self.DeleteAllColumns()
        self.DeleteAllItems()
        self.InsertColumn(0, 'Variable')
        self.InsertColumn(5, 'Value', wx.LIST_FORMAT_LEFT)

        self.__keys = []

        index = 0
        self.__is_init = True
        for var in sorted(self.__vars.keys()):
            self.__keys.append(var)
            self.InsertItem(index, str(var))
            self.__UpdateItem(index)
            # #self.SetItem(index, 1, str(val))
            # #self.SetItemBackgroundColour(index, self.GetItemBackgroundColour(index))
            index += 1
        self.__is_init = False

        self.SetColumnWidth(0, wx.LIST_AUTOSIZE)
        self.SetColumnWidth(1, 100)
        self.currentItem = 0

    # # Returns a wx color (or string equivalent) based on row index
    def GetItemBackgroundColour(self, index: int) -> str:
        if index % 2 != 0:
            return "white"
        return "gray"

    # # Updates an item based on the current elements
    #  @param index Index of parameter
    def __UpdateItem(self, index: int) -> bool:
        self.SetItemBackgroundColour(index, self.GetItemBackgroundColour(index)) # Default background color

        if len(self.__vars) == 0:
            return super().SetItem(index, 1, '') # Set to no value. Whole window should be disabled

        val = self.__vars[self.__keys[index]]

        return super().SetItem(index, 1, val)


# # The GUI-side window for editing the properties of an Element
class LayoutVariablesDialog(wx.Frame):

    # # The constructor
    def __init__(self, parent: wx.Window, id: int, title: str, layout_context: Layout_Context) -> None:
        self.__layout_context = layout_context
        size = (600, 100)
        self.__parent = parent
        wx.Frame.__init__(self, parent, id, title + " properties dialog", size,
                          style = wx.RESIZE_BORDER | wx.SYSTEM_MENU | wx.CAPTION | wx.CLOSE_BOX | wx.CLIP_CHILDREN)

        self.__fnt_location = wx.Font(ScaleFont(12), wx.NORMAL, wx.NORMAL, wx.NORMAL)
        self.SetFont(self.__fnt_location)

        # work could be done to make these prettier
        self.__sizer = wx.BoxSizer(wx.VERTICAL)
        self.__list = VarsListCtrl(self, id,
                                   variables = layout_context.GetLocationVariables())
        self.__sizer.Add(self.__list, 1, wx.EXPAND)
        self.SetSizer(self.__sizer)
        self.SetAutoLayout(True)

    def GetContext(self) -> Layout_Context:
        return self.__layout_context
