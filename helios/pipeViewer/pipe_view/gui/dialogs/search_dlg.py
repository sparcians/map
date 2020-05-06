import sys
import wx
from functools import partial
from gui.widgets.transaction_list import TransactionList
from gui.widgets.location_entry import LocationEntry

## SearchDialog is a window that enables the user to enter a string, conduct a search and
# jump to a location and transaction based on the result. It gets its data from search_handle.py
class SearchDialog(wx.Frame):
    START_COLUMN = 0
    LOCATION_COLUMN = 1
    ANNOTATION_COLUMN = 2

    INITIAL_SEARCH = 0

    def __init__(self, parent):
        self.__context = parent.GetContext()
        self.__search_handle = self.__context.searchhandle
        self.__full_results = []
        self.__filters = []
        # initialize graphical part
        wx.Frame.__init__(self, parent, -1, 'Search', size=(700,600),
            style=wx.MAXIMIZE_BOX|wx.RESIZE_BORDER|wx.CAPTION|wx.CLOSE_BOX)

        main_sizer = wx.BoxSizer(wx.VERTICAL)
        self.__filter_sizer = wx.BoxSizer(wx.VERTICAL)
        main_sizer.Add(self.__filter_sizer, 0, wx.EXPAND, 5)

        # -- Initial Search Settings --
        self.__initial_search = wx.Panel(self, wx.NewId())
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizerbuttons = wx.BoxSizer(wx.HORIZONTAL)
        self.__input = wx.TextCtrl(self.__initial_search, wx.NewId())
        sizerbuttons.Add(self.__input, 2)
        search = wx.Button(self.__initial_search, wx.NewId(), 'Search')
        sizerbuttons.Add(search, 1)
        sizer.Add(sizerbuttons, 0, wx.EXPAND)

        sizer.Add(wx.StaticText(self.__initial_search, wx.NewId(), 'Location:'))

        location_tree = self.__context.dbhandle.database.location_manager.location_tree

        self.__location_to_search = LocationEntry(self.__initial_search, 'top', location_tree)
        sizer.Add(self.__location_to_search, 0, wx.EXPAND)

        self.__use_regex = wx.CheckBox(self.__initial_search, wx.NewId(), 'Use Boost Regex')
        self.__use_regex.SetValue(False)
        sizer.Add(self.__use_regex)

        self.__exclude_locations_not_shown = wx.CheckBox(self.__initial_search,
                                                        wx.NewId(), 'Exclude Locations Not Shown')
        self.__exclude_locations_not_shown.SetValue(False)
        sizer.Add(self.__exclude_locations_not_shown)

        self.__from_current_tick = wx.CheckBox(self.__initial_search, wx.NewId(), 'Search From Current Tick')
        self.__from_current_tick.SetValue(True)
        sizer.Add(self.__from_current_tick)

        self.__colorize = wx.CheckBox(self.__initial_search,
                                        wx.NewId(), 'Colorize Results (note: disregards color_basis)')
        self.__colorize.SetValue(True)
        sizer.Add(self.__colorize)
        self.__filter_sizer.Add(self.__initial_search, 4, wx.EXPAND)

        # Filter editor
        filter_editor = wx.Panel(self, wx.NewId(), style=wx.BORDER_SUNKEN)
        filter_sizer = wx.BoxSizer(wx.HORIZONTAL)
        entry_text = wx.StaticText(filter_editor, wx.NewId(), 'Filter by (string): \n(annotation)')
        filter_sizer.Add(entry_text, 2, wx.ALIGN_LEFT)
        self.__new_filter_query = wx.TextCtrl(filter_editor, wx.NewId())
        filter_sizer.Add(self.__new_filter_query, 4, wx.ALIGN_LEFT)
        add_filter = wx.Button(filter_editor, wx.NewId(), 'Add Filter')
        filter_sizer.Add(add_filter, 2, wx.ALIGN_RIGHT)
        filter_editor.SetSizer(filter_sizer)

        self.__filter_sizer.Add(filter_editor, 0, wx.EXPAND)
        self.__filter_sizer.Hide(1)

        # Everything else
        self.__results_box = TransactionList(self,
                    wx.NewId(),
                    parent.GetCanvas(),
                    name='listbox')

        main_sizer.Add(self.__results_box, 1, wx.EXPAND)
        self.__initial_search.SetSizer(sizer)
        self.SetSizer(main_sizer)

        # bind to events
        search.Bind(wx.EVT_BUTTON, self.OnSearch)
        add_filter.Bind(wx.EVT_BUTTON, self.__OnAddFilter)
        self.__input.Bind(wx.EVT_KEY_DOWN, self.OnKeyPress)
        self.Bind(wx.EVT_LIST_ITEM_ACTIVATED, self.OnClickTransaction, self.__results_box)
        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide()) # Hide instead of closing

    ## defines how the dialog should pop up
    def Show(self):
        wx.Frame.Show(self)
        self.Raise()
        self.FocusQueryBox()

    def FocusQueryBox(self):
        self.__input.SetFocus()

    ## Sets the location in the location box
    #  @pre Requires there be no filters created because this means that the
    #  original search (which defines location) cannot be replaced. Has no
    #  effect if there are filters.
    def SetSearchLocation(self, loc):
        if len(self.__filters) == 0:
            self.__location_to_search.SetValue(loc)

    ## callback that listens for enter being pressed to initiate search
    def OnKeyPress(self, evt):
        if evt.GetKeyCode() == wx.WXK_RETURN:
            self.OnSearch(None)
        else:
            evt.Skip()

    def ApplyFilters(self):
        # full N time complexity for any call because not doing incrementally
        # to speed up, keep track of currently already applied filters
        self.__results_box.Colorize(self.__colorize.GetValue())
        self.__results_box.Clear()
        self.__results_box.RefreshAll()
        for entry in self.__full_results:
            for filter in self.__filters:
                if entry['annotation'].find(filter.query) == -1:
                    break
            else:
                # we have a result
                self.__results_box.Add(entry)

    def OnSearch(self, evt):
        wx.BeginBusyCursor()
        query = self.__input.GetValue()
        dialog = wx.ProgressDialog('Progress',
                                   'Searching Database...',
                                   100, # Maximum
                                   parent=self,
                                   style=wx.PD_CAN_ABORT | wx.PD_REMAINING_TIME)

        ## Callback adapter to forward to wx.ProgressDialog.Update while ignoring some args
        def progress_update(percent, num_results, info):
            return dialog.Update(percent, info)

        if self.__use_regex.GetValue():
            mode = 'regex'
        else:
            mode = 'string'

        if self.__from_current_tick.GetValue():
            start_tick = self.__context.hc
        else:
            start_tick = None # indicate to use db start

        try:
            results = self.__search_handle.Search(mode,
                                                  query,
                                                  start_tick,
                                                  -1, # end_tick
                                                  [], # locations
                                                  progress_update)
        except Exception as ex:
            raise
        finally:
            dialog.Close()
            dialog.Destroy()

        # Limit to search size
        SEARCH_LIMIT = 10000 ## @todo Move this into Search function

        self.__results_box.Colorize(self.__colorize.GetValue())
        self.__results_box.Clear()
        del self.__full_results
        self.__full_results = []
        self.__results_box.RefreshAll()
        visible_locations = self.__context.GetVisibleLocations()

        #location_root_search = str(self.__location_to_search.GetValue()).translate(None, '[]')
        location_root_search = self.__location_to_search.GetValue()
        if location_root_search is None:
            location_root_search = "[]"
        convert_id_to_str = self.__context.dbhandle.database.location_manager.getLocationString
        truncated = False
        if self.__exclude_locations_not_shown.GetValue():
            for start, end, loc_id, annotation in results:
                loc = convert_id_to_str(loc_id)
                loc = str(loc).translate(None, '[]')
                if loc.startswith(location_root_search) and loc_id in visible_locations:
                    entry = {'start':start, 'location':loc, 'annotation':annotation}
                    self.__full_results.append(entry)
                    self.__results_box.Add(entry)
                    if len(self.__full_results) == SEARCH_LIMIT:
                        truncated = True
                        break
        else:
             for start, end, loc, annotation in results:
                loc = convert_id_to_str(loc)
                loc = str(loc).translate(None, '[]')
                if loc.startswith(location_root_search):
                    entry = {'start':start, 'location':loc, 'annotation':annotation}
                    self.__full_results.append(entry)
                    self.__results_box.Add(entry)
                    if len(self.__full_results) == SEARCH_LIMIT:
                        truncated = True
                        break

        self.__results_box.FitColumns()
        self.__filter_sizer.Hide(self.INITIAL_SEARCH)
        self.__filter_sizer.Show(1) # current index of filter editor
        self.__AddFilter(SearchFilter(query, True, location=location_root_search, num_results=len(self.__full_results)))
        self.Layout()
        wx.EndBusyCursor()

        if truncated is True:
            msg = 'Truncated search results to first {} results of {} total'.format(SEARCH_LIMIT, len(results))
            # Do not truncate here. This contains other locations.
            print >> sys.stderr, msg
            wx.MessageBox(msg)

    ## deletes filter and updates results.
    # index is indexed on filters not sizers
    # e.g. (0 is first filter, not search)
    def __OnRemoveFilter(self, evt, obj):
        index = self.__filters.index(obj)
        self.__RemoveFilter(index)
        self.ApplyFilters()
        self.Layout()

    def __RemoveFilter(self, index):
        filter_obj = self.__filters.pop(index)
        panel = filter_obj.GetPanel()
        self.__filter_sizer.Detach(panel)
        panel.Destroy()
        # never use this filter object again

    def OnClickTransaction(self, evt):
        transaction = self.__results_box.GetTransaction(evt.GetIndex())
        start_loc = transaction.get('start')
        if start_loc:
            self.__context.GoToHC(start_loc)

    def __OnResetSearch(self, evt):
        while len(self.__filters):
            self.__RemoveFilter(0)

        self.__filter_sizer.Show(self.INITIAL_SEARCH, True)
        # hide filter addition textbox
        self.__filter_sizer.Hide(1)
        self.Layout()
        # No need to clear. Wait until a new search is done... ##self.__results_box.Clear()
        self.FocusQueryBox()

    def __AddFilter(self, filter_object):
        panel, remove_button = filter_object.MakePanel(parent=self)
        index = len(self.__filters)
        self.__filter_sizer.Insert(index, panel, 0, wx.EXPAND)
        self.__filters.append(filter_object)
        # bind buttom
        if filter_object.initial:
            panel.Bind(wx.EVT_BUTTON, self.__OnResetSearch)
        else:
            panel.Bind(wx.EVT_BUTTON, partial(self.__OnRemoveFilter, obj=filter_object))

    def __OnAddFilter(self, evt):
        query = self.__new_filter_query.GetValue()
        if query:
            self.__AddFilter(SearchFilter(query, False))
            self.ApplyFilters()
            self.Layout()
            self.__new_filter_query.Clear()

## stores the settings for each filter applied
class SearchFilter:
    def __init__(self, query, initial=False, regex=False, location='', num_results=None):
        # string used for search
        self.query = query
        self.initial = initial
        self.location = location
        self.num_results = num_results
        self.__panel = None
        self.__remove = None
        # when we evntually get the python regex and boost regex consistent
        # or have our program do filtering (feed results into std-in and get std-out results) (pipe output)
        #self.regex = regex

    # Get panel, none if no panel
    def GetPanel(self):
        return self.__panel

    ## make visual portion
    def MakePanel(self, parent):
        if not self.__panel:
            self.__panel = wx.Panel(parent, wx.NewId(), style=wx.BORDER_SUNKEN)
            sizer = wx.BoxSizer(wx.HORIZONTAL)
            if self.initial:
                ratio = 2
                string = 'Initial Query: {}\nLocation: {}'.format(self.query, self.location)
                if self.num_results is not None:
                    string += '\nResults: {}'.format(self.num_results)
                button_string = 'Reset Search'
            else:
                ratio = 1
                string = 'Filter by: '+self.query
                button_string = 'Remove'
            info = wx.StaticText(self.__panel, -1, string)
            sizer.Add(info, 4, wx.ALIGN_LEFT)
            self.__remove = wx.Button(self.__panel, wx.NewId(), button_string)
            sizer.Add(self.__remove, ratio, wx.ALIGN_RIGHT)
            self.__panel.SetSizer(sizer)
        return self.__panel, self.__remove

