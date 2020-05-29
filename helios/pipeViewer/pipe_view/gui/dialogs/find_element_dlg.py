

import sys
import wx
from functools import partial
from gui.widgets.element_list import ElementList


# # FindElementDialog is a window that enables the user to enter a string, conduct a search and
# jump to a location and element based on the result. It gets its data from search_handle.py
class FindElementDialog(wx.Frame):
    START_COLUMN = 0
    LOCATION_COLUMN = 1
    ANNOTATION_COLUMN = 2

    INITIAL_SEARCH = 0

    def __init__(self, parent):
        self.__layout_frame = parent
        self.__context = parent.GetContext()
        self.__full_results = []
        # initialize graphical part
        wx.Frame.__init__(self, parent,
                          -1,
                          'Find Element in {}'.format(self.__layout_frame.ComputeTitle()),
                          size = (900, 600),
                          style = wx.MAXIMIZE_BOX | wx.RESIZE_BORDER | wx.CAPTION | wx.CLOSE_BOX | wx.SYSTEM_MENU)

        main_sizer = wx.BoxSizer(wx.VERTICAL)
        self.SetSizer(main_sizer)

        self.__search_sizer = wx.BoxSizer(wx.HORIZONTAL)
        main_sizer.Add(self.__search_sizer, 0, wx.EXPAND, 5)

        choices = set()
        for el in self.__context.GetElements():
            choices.update(el._properties.keys())
        self.__choices = sorted(list(choices))

        lbl_find = wx.StaticText(self, -1, "Elements where: ")
        self.__drop_content = wx.ComboBox(self,
                                          choices = self.__choices,
                                          size = (150, -1),
                                          style = wx.CB_DROPDOWN | wx.CB_READONLY)
        self.__drop_content.SetToolTip('Select a content option on which to search')
        DEFAULT_CHOICE = 'name'
        if DEFAULT_CHOICE in self.__choices:
            self.__drop_content.SetValue(DEFAULT_CHOICE)
        else:
            self.__drop_content.SetSelection(0)

        lbl_comparison = wx.StaticText(self, -1, "contains")
        self.__txt_input = wx.TextCtrl(self, -1, "")
        self.__btn_submit = wx.Button(self, -1, "Find")
        self.__search_sizer.Add(lbl_find, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 4)
        self.__search_sizer.Add(self.__drop_content, 0, 0, 4)
        self.__search_sizer.Add(lbl_comparison, 0, wx.ALIGN_CENTER_VERTICAL | wx.LEFT | wx.RIGHT, 4)
        self.__search_sizer.Add(self.__txt_input, 1, wx.EXPAND, 4)
        self.__search_sizer.Add(self.__btn_submit, 0, 0, 4)

        self.__results_box = ElementList(self,
                                         wx.NewId(),
                                         parent.GetCanvas(),
                                         name = 'listbox',
                                         properties = [''])
        main_sizer.Add(self.__results_box, 1, wx.EXPAND, 5)

        # bind to events
        self.__btn_submit.Bind(wx.EVT_BUTTON, self.OnSearch)
        self.__txt_input.Bind(wx.EVT_KEY_DOWN, self.OnKeyPress)

        self.Bind(wx.EVT_LIST_ITEM_ACTIVATED, self.OnClickElement, self.__results_box)
        # Hide instead of closing
        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide())

    # # defines how the dialog should pop up
    def Show(self):
        wx.Frame.Show(self)
        self.Raise()
        self.FocusQueryBox()

    def FocusQueryBox(self):
        self.__txt_input.SetFocus()

    # # Sets the location in the location box
    #  @pre Requires there be no filters created because this means that the
    #  original search (which defines location) cannot be replaced. Has no
    #  effect if there are filters.
    def SetSearchLocation(self, loc):
        if len(self.__filters) == 0:
            self.__location_to_search.SetValue(loc)

    # # callback that listens for enter being pressed to initiate search
    def OnKeyPress(self, evt):
        if evt.GetKeyCode() == wx.WXK_RETURN:
            self.OnSearch(None)
        else:
            evt.Skip()

    def OnSearch(self, evt):
        self.__results_box.Clear()
        self.__full_results = []
        self.__results_box.RefreshAll()

        prop = self.__drop_content.GetValue()
        term = self.__txt_input.GetValue().lower()

        matches = []
        for el in self.__context.GetElements():
            # Check for match
            if el.HasProperty(prop):
                prop_val = el.GetProperty(prop)
                if prop_val is not None and term in str(prop_val).lower():
                    matches.append(el)

        # Assemble properties based on matches
        properties = set()
        for m in matches:
            properties.update(m._properties.keys())
        properties = sorted(list(properties))

        # If no properties found, assume no matches and terminate the search
        if len(properties) == 0:
            return

        self.__results_box.SetProperties(properties)

        # Add matches to the results table
        for m in matches:
            entry = {}
            for p in properties:
                if m.HasProperty(p):
                    entry[p] = str(m.GetProperty(p))
                else:
                    entry[p] = ''

            self.__full_results.append(entry)
            self.__results_box.Add(m, entry)

        self.__results_box.FitColumns()

    # # Attempts to select the element in the associated layout
    def OnClickElement(self, evt):
        element = self.__results_box.GetElement(evt.GetIndex())
        if element is not None:
            if self.__layout_frame.GetCanvas().GetInputDecoder().GetEditMode():
                sel_mgr = self.__layout_frame.GetCanvas().GetSelectionManager()
                sel_mgr.ClearSelection()
                sel_mgr.Add(element)
            else:
                # Non-edit mode
                pair = self.__context.GetElementPair(element)
                self.__layout_frame.GetCanvas().GetSelectionManager().SetPlaybackSelected(pair)
                self.__layout_frame.Refresh()
