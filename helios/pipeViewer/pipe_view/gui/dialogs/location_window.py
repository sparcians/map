import fnmatch
from gui.dialogs.element_propsdlg import ElementTypeSelectionDialog
import logging
import wx
import wx.lib.gizmos


class LocationWindow(wx.Frame):
    '''
    Window showing all available locations in particular database
    '''

    # ID of node column
    COL_NODE = 0

    # ID of clock column
    COL_CLOCK = 1

    # ID of full-path column
    COL_PATH = 2

    def  __init__(self, parent, elpropsdlg):
        '''
        @param parent Parent Layout_Frame
        @param elpropsdlg Element Properties Dialog for this frame
        '''
        self.__layout_frame = parent
        self.__el_props_dlg = elpropsdlg

        self.__db = parent.GetContext().dbhandle.database
        self.__tree_dict = {}

        # Create Controls

        title = "Locations for {0}".format(self.__db.filename)
        wx.Frame.__init__(self, parent, -1, title, size = (1025, 600),
                          style = wx.MAXIMIZE_BOX | wx.RESIZE_BORDER | wx.CAPTION | wx.CLOSE_BOX)

        self.__fnt_small = wx.Font(12, wx.NORMAL, wx.NORMAL, wx.NORMAL)

        static_heading = wx.StaticText(self, -1, "Showing all locations for:")
        static_filename = wx.StaticText(self, -1, self.__db.filename)
        static_filename.SetFont(self.__fnt_small)
        filter_label = wx.StaticText(self, -1, "Filter")
        self.__filter_ctrl = wx.TextCtrl(self, -1)

        # TODO style had wx.TR_EXTENDED but seemed missing from the API
        self.__tree_ctrl = wx.lib.gizmos.treelistctrl.TreeListCtrl(self,
                                                                   -1,
                                                                   size = wx.Size(-1, 100),
                                                                   style = wx.TR_DEFAULT_STYLE | wx.TR_FULL_ROW_HIGHLIGHT | \
                                                                   wx.TR_MULTIPLE | wx.TR_HIDE_ROOT)
        self.__tree_ctrl.AddColumn('Node')
        self.__tree_ctrl.SetColumnWidth(self.COL_NODE, 400)
        self.__tree_ctrl.AddColumn('Clock')
        self.__tree_ctrl.SetColumnWidth(self.COL_CLOCK, 100)
        self.__tree_ctrl.AddColumn('Full Path')
        self.__tree_ctrl.SetColumnWidth(self.COL_PATH, 500)

        tree = self.__db.location_manager.location_tree
        self.__root = self.__tree_ctrl.AddRoot('<root>')
        self.__use_filter = False
        self.__SetLocationTree(tree)
        # User preference is to NOT expand tree. ##self.__tree_ctrl.ExpandAll(root)

        self.__btn_create_element = wx.Button(self, -1, "Create Element(s)", style = wx.BU_EXACTFIT)
        self.__btn_create_element.SetToolTip('Creates a new element for each of the selected ' \
                                             'locations in the tree')
        self.__btn_set = wx.Button(self, -1, "Set Location to Selected", style = wx.BU_EXACTFIT)
        self.__btn_set.SetToolTip('Sets the location string for all selected elements to the ' \
                                  'selected location in the tree. Generally, the selected ' \
                                  'location should be a leaf node')

        # Prepare

        self.__UpdateButtons()

        # Bindings

        self.__tree_ctrl.Bind(wx.EVT_TREE_SEL_CHANGED, self.__OnTreeSelChanged)
        self.__btn_create_element.Bind(wx.EVT_BUTTON, self.__OnCreateElements)
        self.__btn_set.Bind(wx.EVT_BUTTON, self.__OnSetLocation)
        self.__filter_ctrl.Bind(wx.EVT_TEXT, self.__OnFilterChanged)
        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide()) # Hide instead of closing
        self.__tree_ctrl.Bind(wx.EVT_TREE_ITEM_EXPANDING, self.__OnExpandItem)

        # Layout

        button_sizer = wx.BoxSizer(wx.HORIZONTAL)
        button_sizer.Add(self.__btn_create_element, 0, wx.RIGHT, 2)
        button_sizer.Add(self.__btn_set, 0)
        button_sizer.Add((1, 1), 1, wx.EXPAND)

        sz = wx.BoxSizer(wx.VERTICAL)
        sz.Add((1, 3), 0)
        sz.Add(static_heading, 0, wx.EXPAND | wx.ALL, 2)
        sz.Add(static_filename, 0, wx.EXPAND | wx.ALL, 4)
        filter_sizer = wx.BoxSizer(wx.HORIZONTAL)
        filter_sizer.Add((2, 1), 0)
        filter_sizer.Add(filter_label, 0, wx.EXPAND | wx.ALL)
        filter_sizer.Add(self.__filter_ctrl, 0, wx.EXPAND | wx.ALL)
        sz.Add(filter_sizer, 0, wx.EXPAND | wx.ALL)
        sz.Add(self.__tree_ctrl, 2, wx.EXPAND | wx.ALL, 1)
        sz.Add(button_sizer, 0, wx.EXPAND | wx.ALL, 4)
        self.SetSizer(sz)
        self.Bind(wx.EVT_ACTIVATE, self.__OnActivate)

        # Fire a text event to load the filter for the first time
        wx.PostEvent(self.__filter_ctrl.GetEventHandler(), wx.PyCommandEvent(wx.EVT_TEXT.typeId, self.GetId()))

    def __OnActivate(self, evt):
        '''
        Sets the focus on the filter text control when the window is shown
        '''
        self.__filter_ctrl.SetFocus()

    def __SetLocationTree(self, tree):
        '''
        Set the location tree dictionary for this tree view and populate the
        top-level entities
        '''
        self.__tree_ctrl.DeleteChildren(self.__root)
        self.__tree_dict = {}
        for key in sorted(tree.keys(),
                          key = len):
            child = self.__tree_ctrl.AppendItem(self.__root, key)
            self.__tree_ctrl.SetItemText(child, key, self.COL_PATH)
            self.__tree_dict[key] = child

            # set clock column
            clock_id = self.__db.location_manager.getLocationInfo(key, {})[2]
            if clock_id != self.__db.location_manager.NO_CLOCK:
                clk = self.__db.clock_manager.getClockDomain(clock_id)
                self.__tree_ctrl.SetItemText(child, clk.name, self.COL_CLOCK)
            else:
                self.__tree_ctrl.SetItemText(child, '<unknown>', self.COL_CLOCK)
            if tree[key] != {}:
                self.__tree_ctrl.SetItemHasChildren(child, True)

    def __OnExpandItem(self, evt):
        '''
        This intelligently populates the tree as its nodes are expanded instead
        of doing it all at once - reduces time to open the popup
        '''
        item = evt.GetItem()
        path = self.__tree_ctrl.GetItemText(item, self.COL_PATH)
        if path:
            curdict = self.__loc_tree
            for token in path.split('.'):
                curdict = curdict[token]
            self.SetTree(curdict, item, path)

    def SetTree(self, tree, item = None, path = ""):
        '''
        Adds every member at the top level of location dictionary "tree" to
        node "item"
        '''
        if not item:
            item = self.__root
        # Sort keys by length ascending then string-comparison alphabetically
        for k, v in sorted(tree.items(),
                           key = len):
            if not path:
                child_path = k
            else:
                child_path = path + '.' + k

            if child_path in self.__tree_dict:
                child = self.__tree_dict[child_path]
            else:
                child = self.__tree_ctrl.AppendItem(item, k)
                self.__tree_ctrl.SetItemText(child, child_path, self.COL_PATH)
                self.__tree_dict[child_path] = child

                # set clock column
                clock_id = self.__db.location_manager.getLocationInfo(child_path, {})[2]
                if clock_id != self.__db.location_manager.NO_CLOCK:
                    clk = self.__db.clock_manager.getClockDomain(clock_id)
                    self.__tree_ctrl.SetItemText(child, clk.name, self.COL_CLOCK)
                else:
                    self.__tree_ctrl.SetItemText(child, '<unknown>', self.COL_CLOCK)

            # Leaf nodes aren't expandable
            if v != {}:
                self.__tree_ctrl.SetItemHasChildren(child, True)

    def __RecursiveExpand(self, node, limit, level = 0):
        '''
        Recursively expand tree nodes up to a given level
        '''
        if node != self.__root:
            self.__tree_ctrl.Expand(node)

        (cur_node, cookie) = self.__tree_ctrl.GetFirstChild(node)
        while cur_node and cur_node.IsOk():
            if level < limit:
                self.__RecursiveExpand(cur_node, limit, level + 1)
            cur_node = self.__tree_ctrl.GetNextSibling(cur_node)

    def __GenerateFilteredTree(self, tree, path = ''):
        '''
        Generates a filtered tree to populate the location tree control
        '''
        subtree = {}
        for k, v in sorted(tree.items(), key = len):
            if not path:
                child_path = k
            else:
                child_path = path + '.' + k

            # If the filter matches the child's path, add the child to the subtree
            if fnmatch.fnmatch(child_path, self.__filter):
                subtree[k] = self.__GenerateFilteredTree(v, child_path)
            # Otherwise, if it isn't a leaf node, then it might be the parent of a match
            elif v != {}:
                # So, check its children
                result = self.__GenerateFilteredTree(v, child_path)
                # If it isn't empty, then it has children that match, so we add it to the subtree
                if result != {}:
                    subtree[k] = result

        return subtree

    def __OnFilterChanged(self, evt):
        '''
        Handles changes to the filter
        '''
        self.__loc_tree = self.__db.location_manager.location_tree
        # If a filter was specified
        if self.__filter_ctrl.GetValue():
            self.__use_filter = True
            self.__filter = self.__filter_ctrl.GetValue()
            # Generate a filtered ttee
            self.__loc_tree = self.__GenerateFilteredTree(self.__loc_tree)
        else:
            self.__use_filter = False
        # Setup the tree control with the (possibly) filtered tree
        self.__SetLocationTree(self.__loc_tree)
        # Expand everything up to the last level of the filter
        if self.__use_filter:
            self.__RecursiveExpand(self.__root, self.__filter.count('.'))

    def __OnTreeSelChanged(self, evt):
        '''
        Handles selection changes in the location tree
        Updates button enable/caption states based on selection
        '''
        self.__UpdateButtons()

    def __OnCreateElements(self, evt):
        '''
        Handles Clicks on "create elements" button
        Sets the selected location string as the location for the entire selection
        '''
        sels = self.__tree_ctrl.GetSelections()
        assert sels, 'Create Elements Location button should not have been enabled if there were no locations selected'

        canvas = self.__layout_frame.GetCanvas()
        sel_mgr = canvas.GetSelectionManager()
        layout = self.__layout_frame.GetContext().GetLayout()
        canvas_visible = self.__layout_frame.GetCanvas().GetVisibleArea()

        sel_mgr.Clear()

        # Place in middle of the screen
        last_el_pos = (int(canvas_visible[0] + canvas_visible[2] / 2), \
                       int(canvas_visible[1] + canvas_visible[3] / 2))

        if not canvas.GetInputDecoder().GetEditMode():
            # go into edit mode
            self.__layout_frame.SetEditMode(True)
        type_dialog = ElementTypeSelectionDialog(canvas)
        type_dialog.ShowModal()
        type_dialog.Center()
        for item in sels:
            e = sel_mgr.GenerateElement(layout, type_dialog.GetSelection(), add_to_selection = True) # Create and add to selection
            e.SetProperty('position', last_el_pos)

            loc_str = self.__tree_ctrl.GetItemText(item, self.COL_PATH)
            e.SetProperty('LocationString', loc_str)
            dims = e.GetProperty('dimensions')

            # Shift down by grid
            # #last_el_pos = (last_el_pos[0] + canvas.gridsize, last_el_pos[1] + canvas.gridsize)

            # ## Shift by half-dimensions of new element
            # #last_el_pos = (last_el_pos[0] + dims[0]/2, last_el_pos[1] + dims[1]/2)

            # Stack elements by default
            last_el_pos = (last_el_pos[0], last_el_pos[1] + dims[1])

        # Need to reflect new selection in element properties dialog
        self.__el_props_dlg.Refresh()

    def __OnSetLocation(self, evt):
        '''
        Handles Clicks on "set location" button
        Sets the selected location string as the location for the entire selection
        '''
        sels = self.__tree_ctrl.GetSelections()
        assert len(sels) == 1, 'Set Location button should not have been enabled if there was not exactly one location selected'
        item = sels[0]
        els = self.__layout_frame.GetCanvas().GetSelectionManager().GetSelection()
        loc_str = self.__tree_ctrl.GetItemText(item, self.COL_PATH)
        logging.debug(f'Setting all selection location to "{loc_str}"')
        for e in els:
            e.SetProperty('LocationString', loc_str)

        # Elements changed. Need to redraw and reflect changes in element properties dialog
        self.__layout_frame.Refresh()
        self.__el_props_dlg.Refresh()

    def __UpdateButtons(self):
        '''
        Enables buttons based on the selection state
        '''
        sels = self.__tree_ctrl.GetSelections()
        self.__btn_set.Enable(len(sels) == 1)
        self.__btn_create_element.Enable(len(sels) > 0)
