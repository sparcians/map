

import wx
import os
import logging
import sys

import wx.lib.mixins.listctrl  as  listmix
from gui.font_utils import ScaleFont


# # A helper class for building a list of variables and values in separate columns
class VarsListCtrl(wx.ListCtrl,
                   listmix.ListCtrlAutoWidthMixin,
                   listmix.TextEditMixin):

    # # The Constructor
    #  Calls Populate() which actually sets up the ListCtrl
    def __init__(self, parent, ID, pos = wx.DefaultPosition,
                 size = wx.DefaultSize, style = 0, variables = None):
        wx.ListCtrl.__init__(self, parent, ID, pos, size, style)

        self.__parent = parent
        listmix.ListCtrlAutoWidthMixin.__init__(self)
        self.__vars = variables if variables is not None else {}
        self.__keys = [] # index in list maps to variable name
        self.Populate()
        listmix.TextEditMixin.__init__(self)

    # # Convenience method for forwarding calls to Populate(), which clears
    #  and reconstitutes the table with up-to-date values
    def Refresh(self):
        self.Populate()

    # # Returns the number of items in the dialog
    def GetNumItems(self):
        return self.GetItemCount()

    # # Allows users to edit the second column (values)
    def OpenEditor(self, col, row):
        if col == 1:
            index = row
            super(VarsListCtrl, self).OpenEditor(col, row)

    # # Get's called when something is edited in the ListCtrl (in the GUI
    #  window, by the user)
    def SetItem(self, index, col, data, is_init = False):
        # hopefully whatever the user input is valid... (data will pass
        # through the validation steps on the Element side)
        if not is_init:
            var = self.__keys[index]
            self.__vars[var] = data

        # go ahead and update the text fields on display and redraw the canvas
        self.__UpdateItem(index)

        if not is_init:
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

    # # Used for creating the columns and rows and dumping in initial values
    #  (or current values every time the user changes what Element is being edited)
    def Populate(self):
        # for normal, simple columns, you can add them like this:
        self.DeleteAllColumns()
        self.DeleteAllItems()
        self.InsertColumn(0, 'Variable')
        self.InsertColumn(5, 'Value', wx.LIST_FORMAT_LEFT)

        self.__keys = []

        index = 0
        for var in sorted(self.__vars.keys()):
            self.__keys.append(var)
            self.InsertItem(index, str(var))
            self.__UpdateItem(index)
            # #self.SetItem(index, 1, str(val), is_init = True)
            # #self.SetItemBackgroundColour(index, self.GetItemBackgroundColour(index))
            index += 1

        self.SetColumnWidth(0, wx.LIST_AUTOSIZE)
        self.SetColumnWidth(1, 100)
        self.currentItem = 0

    # # Returns a wx color (or string equivalent) based on row index
    def GetItemBackgroundColour(self, index):
        if index % 2 != 0:
            return "white"
        return "gray"

    # # Updates an item based on the current elements
    #  @param index Index of parameter
    def __UpdateItem(self, index):
        self.SetItemBackgroundColour(index, self.GetItemBackgroundColour(index)) # Default background color

        if len(self.__vars) == 0:
            super(VarsListCtrl, self).SetItem(index, 1, '') # Set to no value. Whole window should be disabled
            return

        val = self.__vars[self.__keys[index]]

        super(VarsListCtrl, self).SetItem(index, 1, val)


# # The GUI-side window for editing the properties of an Element
class LayoutVariablesDialog(wx.Frame):

    # # The constructor
    def __init__(self, parent, id, title, layout_context):
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
                                   style = wx.LC_REPORT
                                         | wx.BORDER_NONE
                                         | wx.LC_SORT_ASCENDING,
                                   variables = layout_context.GetLocationVariables())
        self.__sizer.Add(self.__list, 1, wx.EXPAND)
        self.SetSizer(self.__sizer)
        self.SetAutoLayout(True)

    def GetContext(self):
        return self.__layout_context
