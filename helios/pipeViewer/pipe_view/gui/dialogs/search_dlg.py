from __future__ import annotations
import sys
import wx
from functools import partial
from ..widgets.transaction_list import TransactionList
from ..widgets.location_entry import LocationEntry
from typing import List, Optional, Tuple, TypedDict, TYPE_CHECKING

if TYPE_CHECKING:
    from ..layout_frame import Layout_Frame


class SearchResult(TypedDict):
    start: int
    location: int
    annotation: str


# SearchDialog is a window that enables the user to enter a string, conduct a
# search and jump to a location and transaction based on the result. It gets
# its data from search_handle.py
class SearchDialog(wx.Frame):
    START_COLUMN = 0
    LOCATION_COLUMN = 1
    ANNOTATION_COLUMN = 2

    INITIAL_SEARCH = 0

    def __init__(self, parent: Layout_Frame) -> None:
        self.__context = parent.GetContext()
        self.__canvas = parent.GetCanvas()
        self.__search_handle = self.__context.searchhandle
        self.__full_results: List[SearchResult] = []
        self.__filters: List[SearchFilter] = []
        # initialize graphical part
        wx.Frame.__init__(self,
                          parent,
                          -1,
                          'Search',
                          size=(700, 600),
                          style=(wx.MAXIMIZE_BOX |
                                 wx.RESIZE_BORDER |
                                 wx.CAPTION |
                                 wx.CLOSE_BOX |
                                 wx.SYSTEM_MENU))

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

        sizer.Add(wx.StaticText(self.__initial_search,
                                wx.NewId(),
                                'Location:'))

        location_tree = self.__context.dbhandle.database.location_manager.location_tree  # noqa: E501

        self.__location_to_search = LocationEntry(self.__initial_search,
                                                  'top',
                                                  location_tree)
        sizer.Add(self.__location_to_search, 0, wx.EXPAND)

        self.__use_regex = wx.CheckBox(self.__initial_search,
                                       wx.NewId(),
                                       'Use Regex')
        self.__use_regex.SetValue(False)
        sizer.Add(self.__use_regex)

        self.__exclude_locations_not_shown = wx.CheckBox(
            self.__initial_search,
            wx.NewId(),
            'Exclude Locations Not Shown'
        )
        self.__exclude_locations_not_shown.SetValue(False)
        sizer.Add(self.__exclude_locations_not_shown)

        self.__from_current_tick = wx.CheckBox(self.__initial_search,
                                               wx.NewId(),
                                               'Search From Current Tick')
        self.__from_current_tick.SetValue(True)
        sizer.Add(self.__from_current_tick)

        self.__colorize = wx.CheckBox(
            self.__initial_search,
            wx.NewId(),
            'Colorize Results (note: disregards color_basis)'
        )
        self.__colorize.SetValue(True)
        sizer.Add(self.__colorize)
        self.__filter_sizer.Add(self.__initial_search, 4, wx.EXPAND)

        # Filter editor
        filter_editor = wx.Panel(self, wx.NewId(), style=wx.BORDER_SUNKEN)
        filter_sizer = wx.BoxSizer(wx.HORIZONTAL)
        entry_text = wx.StaticText(filter_editor,
                                   wx.NewId(),
                                   'Filter by (string): \n(annotation)')
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
                                             parent.GetCanvas(),
                                             name='listbox')

        main_sizer.Add(self.__results_box, 1, wx.EXPAND)
        self.__initial_search.SetSizer(sizer)
        self.SetSizer(main_sizer)

        # bind to events
        search.Bind(wx.EVT_BUTTON, self.OnSearch)
        add_filter.Bind(wx.EVT_BUTTON, self.__OnAddFilter)
        self.__input.Bind(wx.EVT_KEY_DOWN, self.OnKeyPress)
        self.Bind(wx.EVT_LIST_ITEM_ACTIVATED,
                  self.OnClickTransaction,
                  self.__results_box)
        # Hide instead of closing
        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide())

    # defines how the dialog should pop up
    def Show(self, show: bool = True) -> bool:
        res = wx.Frame.Show(self, show)
        self.Raise()
        self.FocusQueryBox()
        return res

    def FocusQueryBox(self) -> None:
        self.__input.SetFocus()

    # Sets the location in the location box
    # @pre Requires there be no filters created because this means that the
    # original search (which defines location) cannot be replaced. Has no
    # effect if there are filters.
    def SetSearchLocation(self, loc: str) -> None:
        if not self.__filters:
            self.__location_to_search.SetValue(loc)

    # callback that listens for enter being pressed to initiate search
    def OnKeyPress(self, evt: wx.KeyEvent) -> None:
        if evt.GetKeyCode() == wx.WXK_RETURN:
            self.OnSearch(None)
        else:
            evt.Skip()

    def __AddResult(self, entry: SearchResult) -> None:
        self.__results_box.Add(entry)
        self.__context.AddSearchResult(entry)

    def __ClearResults(self) -> None:
        self.__results_box.Clear()
        self.__context.ClearSearchResults()

    def __UpdateSearchHighlighting(self) -> None:
        self.__canvas.UpdateTransactionHighlighting()
        self.__context.RedrawHighlightedElements()
        self.__canvas.FullUpdate()

    def ApplyFilters(self) -> None:
        # full N time complexity for any call because not doing incrementally
        # to speed up, keep track of currently already applied filters
        self.__results_box.Colorize(self.__colorize.GetValue())
        self.__ClearResults()
        self.__results_box.RefreshAll()
        for entry in self.__full_results:
            for filter in self.__filters:
                if entry['annotation'].find(filter.query) == -1:
                    break
            else:
                # we have a result
                self.__AddResult(entry)

        self.__UpdateSearchHighlighting()

    def OnSearch(self, evt: Optional[wx.CommandEvent]) -> None:
        wx.BeginBusyCursor()
        query = self.__input.GetValue()
        dialog = wx.ProgressDialog(
            'Progress',
            'Searching Database...',
            100,  # Maximum
            parent=self,
            style=wx.PD_CAN_ABORT | wx.PD_REMAINING_TIME
        )

        # Callback adapter to forward to wx.ProgressDialog.Update while
        # ignoring some args
        def progress_update(percent: int,
                            num_results: int,
                            info: str) -> Tuple[bool, bool]:
            return dialog.Update(percent, info)

        if self.__use_regex.GetValue():
            mode = 'regex'
        else:
            mode = 'string'

        if self.__from_current_tick.GetValue():
            start_tick = self.__context.hc
        else:
            start_tick = None  # indicate to use db start

        try:
            results = self.__search_handle.Search(mode,
                                                  query,
                                                  start_tick,
                                                  -1,  # end_tick
                                                  [],  # locations
                                                  progress_update)
        except Exception:
            raise
        finally:
            dialog.Close()
            dialog.Destroy()

        # Limit to search size
        SEARCH_LIMIT = 10000  # @todo Move this into Search function

        self.__results_box.Colorize(self.__colorize.GetValue())
        self.__ClearResults()
        del self.__full_results
        self.__full_results = []
        self.__results_box.RefreshAll()
        visible_locations = self.__context.GetVisibleLocations()

        location_root_search = self.__location_to_search.GetValue()
        if location_root_search is None:
            location_root_search = "[]"
        truncated = False

        def loc_str(loc_id: int) -> str:
            loc = self.__context.dbhandle.database.location_manager.getLocationString(loc_id)  # noqa: E501
            loc = str(loc).translate({ord(c): None for c in '[]'})
            return loc

        for start, end, loc_id, annotation in results:
            if loc_str(loc_id).startswith(location_root_search) and \
                    (not self.__exclude_locations_not_shown.GetValue() or
                     loc_id in visible_locations):
                entry: SearchResult = {
                    'start': start,
                    'location': loc_id,
                    'annotation': annotation
                }
                self.__full_results.append(entry)
                self.__AddResult(entry)
                if len(self.__full_results) == SEARCH_LIMIT:
                    truncated = True
                    break

        self.__results_box.FitColumns()
        self.__filter_sizer.Hide(self.INITIAL_SEARCH)
        self.__filter_sizer.Show(1)  # current index of filter editor
        self.__AddFilter(SearchFilter(query,
                                      True,
                                      location=location_root_search,
                                      num_results=len(self.__full_results)))
        self.Layout()

        self.__UpdateSearchHighlighting()

        wx.EndBusyCursor()

        if truncated is True:
            msg = f'Truncated search results to first {SEARCH_LIMIT} ' \
                  f'results of {len(results)} total'
            # Do not truncate here. This contains other locations.
            print(msg, file=sys.stderr)
            wx.MessageBox(msg)

    # deletes filter and updates results.
    # index is indexed on filters not sizers
    # e.g. (0 is first filter, not search)
    def __OnRemoveFilter(self,
                         evt: wx.CommandEvent,
                         obj: SearchFilter) -> None:
        index = self.__filters.index(obj)
        self.__RemoveFilter(index)
        self.ApplyFilters()
        self.Layout()

    def __RemoveFilter(self, index: int) -> None:
        filter_obj = self.__filters.pop(index)
        panel = filter_obj.GetPanel()
        if panel is not None:
            self.__filter_sizer.Detach(panel)
            panel.Destroy()
            # never use this filter object again

    def OnClickTransaction(self, evt: wx.ListEvent) -> None:
        transaction = self.__results_box.GetTransaction(evt.GetIndex())
        start_loc = transaction.get('start')
        if start_loc:
            self.__context.GoToHC(start_loc)

    def __OnResetSearch(self, evt: wx.CommandEvent) -> None:
        while self.__filters:
            self.__RemoveFilter(0)

        self.__filter_sizer.Show(self.INITIAL_SEARCH, True)
        # hide filter addition textbox
        self.__filter_sizer.Hide(1)
        self.Layout()
        self.FocusQueryBox()

    def __AddFilter(self, filter_object: SearchFilter) -> None:
        panel, remove_button = filter_object.MakePanel(parent=self)
        index = len(self.__filters)
        self.__filter_sizer.Insert(index, panel, 0, wx.EXPAND)
        self.__filters.append(filter_object)
        # bind buttom
        if filter_object.initial:
            panel.Bind(wx.EVT_BUTTON, self.__OnResetSearch)
        else:
            panel.Bind(wx.EVT_BUTTON, partial(self.__OnRemoveFilter,
                                              obj=filter_object))

    def __OnAddFilter(self, evt: wx.CommandEvent) -> None:
        query = self.__new_filter_query.GetValue()
        if query:
            self.__AddFilter(SearchFilter(query, False))
            self.ApplyFilters()
            self.Layout()
            self.__new_filter_query.Clear()


# stores the settings for each filter applied
class SearchFilter:
    def __init__(self,
                 query: str,
                 initial: bool = False,
                 regex: bool = False,
                 location: str = '',
                 num_results: Optional[int] = None) -> None:
        # string used for search
        self.query = query
        self.initial = initial
        self.location = location
        self.num_results = num_results
        self.__panel: Optional[wx.Panel] = None
        self.__remove: Optional[wx.Button] = None

    # Get panel, none if no panel
    def GetPanel(self) -> Optional[wx.Panel]:
        return self.__panel

    # make visual portion
    def MakePanel(self, parent: SearchDialog) -> Tuple[wx.Panel, wx.Button]:
        if self.__panel is None:
            self.__panel = wx.Panel(parent, wx.NewId(), style=wx.BORDER_SUNKEN)
            sizer = wx.BoxSizer(wx.HORIZONTAL)
            if self.initial:
                ratio = 2
                string = f'Initial Query: {self.query}\n' \
                         f'Location: {self.location}'
                if self.num_results is not None:
                    string += f'\nResults: {self.num_results}'
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
        assert self.__remove is not None
        return self.__panel, self.__remove
