import wx
import wx.grid
import wx.lib.colourchooser.pycolourchooser

import model.content_options as copts
from model.schedule_element import ScheduleLineElement
from model.element import LocationallyKeyedElement
from model.rpc_element import RPCElement
from ast import literal_eval

MULTIPLE_VALS_STR = '<multiple values>'
MULTIPLE_VALS_COLOR = wx.Colour(200, 200, 100)


# # Grid descendent used to display and edit element properties
class ElementPropertyList(wx.grid.Grid):

    def __init__(self, parent, id):
        self.__in_resize = False

        wx.grid.Grid.__init__(self, parent, id)
        self.__elements = []
        self.__keys = []
        # population function (SetElements) is working
        self.__internal_editing = False
        self.__parent = parent
        self.__sel_mgr = None

        # make the graphical stuff
        self.CreateGrid(1, 1)
        self.SetColLabelSize(0)
        self.SetRowLabelSize(150)

        self.Bind(wx.grid.EVT_GRID_CELL_CHANGED, self.OnCellChange)
        self.Bind(wx.EVT_SIZE, self.OnResize)
        self.__db = self.__parent.GetParent().GetContext().dbhandle.database

        self.__InstallGridHint()

    def OnResize(self, evt):
        # #self.AutoSizeColumn(0, False)
        remaining = self.GetClientSize()[0] - self.GetRowLabelSize()

        # UPdate column sizes with recursion prevention
        if self.__in_resize == False and remaining != 0:
            # Do a resize of slots here but ignore the next resize event
            # setColSize can cause recusion
            self.__in_resize = True
            self.SetColSize(0, remaining)

            self.ForceRefresh() # Prevent ghosting of gridlines

        else:
            self.__in_resize = False

        evt.Skip()

    # # Set the elements.
    #  @param elements Elements being edited
    #  @param sem_mgr Selection manager
    def SetElements(self, elements, sel_mgr):
        self.__elements = elements[:]
        self.__sel_mgr = sel_mgr
        if self.__elements:
            # show intersection of all current selected properties
            props = set(self.__elements[0].GetProperties().keys())
            hidden_props = set(self.__elements[0].GetHiddenProperties())
            read_only_props = set(self.__elements[0].GetReadOnlyProperties())
            # #props.difference_update(self.__elements[0].GetHiddenProperties())
            for i in range(1, len(self.__elements)):
                el = self.__elements[i]
                # #props.intersection_update(el.GetElementProperties())
                props.union(el.GetElementProperties())
                # #props.difference_update(self.__elements[i].GetHiddenProperties())
                hidden_props.intersection_update(el.GetHiddenProperties())
                read_only_props.intersection_update(el.GetReadOnlyProperties())

            # Remove hidden keys
            props.difference_update(hidden_props)
            read_only_props.difference_update(hidden_props)
        else:
            props = []
            hidden_props = []
            read_only_props = []

        self.__internal_editing = True
        # make our GUI alterations
        number_old_rows = self.GetNumberRows()
        if number_old_rows:
            self.DeleteRows(0, number_old_rows)
        self.AppendRows(len(props), True)
        self.__keys = list(props)
        self.__keys = sorted(self.__keys)

        # move type to top
        for i, key in enumerate(self.__keys):
            if key == 'type':
                self.__keys.insert(0, self.__keys.pop(i))
                break

        # Move read only keys to bottom
        read_only_keys = list(filter(lambda k: k in read_only_props, self.__keys))
        self.__keys = list(filter(lambda k: k not in read_only_props, self.__keys)) + read_only_keys

        index = 0
        for key in self.__keys:
            self.SetRowLabelValue(index, key)
            self.__UpdateItem(index) # set value
            if key in ['type', 'children', 'name', 'connections_in', 'connections_out']:
                self.SetReadOnly(index, 0, True)
            elif key == 'Content':
                cont_opts = copts.GetContentOptions()
                self.SetCellEditor(index, 0, DropdownCellEditor(cont_opts))
            elif key == 'line_style':
                self.SetCellEditor(index, 0,
                            DropdownCellEditor(ScheduleLineElement.DRAW_LOOKUP.keys()))
            elif key == 'color_basis_type':
                self.SetCellEditor(index, 0,
                            DropdownCellEditor(LocationallyKeyedElement.COLOR_BASIS_TYPES))
            elif key == 'anno_basis_type':
                self.SetCellEditor(index, 0,
                            DropdownCellEditor(RPCElement.ANNO_BASIS_TYPES))
            elif key == 'color':
                self.SetCellEditor(index, 0, ColorCellEditor())
            elif key == 'LocationString':
                self.SetCellEditor(index, 0, TreeCellEditor(self.__db.location_manager.location_tree))
            elif key == 'short_format':
                self.SetCellEditor(index, 0,
                            DropdownCellEditor(ScheduleLineElement.SHORT_FORMAT_TYPES))
            elif key == 'clock':
                self.SetCellEditor(index, 0,
                            DropdownCellEditor([clk.name for clk in self.__db.clock_manager.getClocks()]))
            if key in read_only_props:
                self.SetReadOnly(index, 0, True)
                self.SetReadOnly(index, 1, True)
            index += 1
        self.currentItem = 0
        self.AutoSizeColumn(0, setAsMin = True)
        self.__internal_editing = False

    def OnCellChange(self, evt):
        row = evt.GetRow()

        value = self.GetCellValue(row, 0)
        # hopefully whatever the user input is valid... (data will pass
        # through the validation steps on the Element side)

        # if the population function isn't calling
        if not self.__internal_editing:
            k = self.__keys[row]
            # Begin checkpoint including all elements selected so that selection
            # is correct checkpoint apply/remove
            if self.__sel_mgr:
                self.__sel_mgr.BeginCheckpoint('set property {}'.format(k), force_elements = self.__elements)
            try:
                els = [e for e in self.__elements if e.HasProperty(k)]
                success = False
                for e in els:
                    try:
                        e.SetProperty(k, value)
                        # but if it throws one of these errors, we'll catch it and display it
                        # on the status bar. If other sorts of errors/expceptions are being
                        # raised, they will need to be hardcoded in the same fashion
                    except ValueError as v:
                        self.__parent.ShowError(v)
                        break # Show first error and break
                    except TypeError as t:
                        self.__parent.ShowError(t)
                        break # Show first error and break
                else:
                    # complete
                    if len(self.__elements) > 1:
                        # probably need to set multiple-value color to white
                        self.SetCellBackgroundColour(row, 0, (255, 255, 255))
                    self.__parent.GetCanvas().context.GoToHC()
                    self.__parent.GetCanvas().Refresh()
                    success = True

                if not success:
                    # not fully completed due to error
                    # set back
                    self.SetCellValue(row, 0, str(e.GetProperty(k)))
            except:
                raise
            finally:
                if self.__sel_mgr:
                    self.__sel_mgr.CommitCheckpoint(force_elements = self.__elements)

    def GetNumberOfElements(self):
        return len(self.__elements)

    # # override read-only setter to gray out text
    def SetReadOnly(self, row, col, is_read_only = True):
        wx.grid.Grid.SetReadOnly(self, row, col, is_read_only)
        if is_read_only:
            color = (128, 128, 128)
        else:
            color = (0, 0, 0)
        self.SetCellTextColour(row, col, color)

    def __UpdateItem(self, index):
        # Look through each element. If property value read all match, show that value. If
        # they differ, show a string saying that
        val = None
        prop = self.__keys[index]
        for e in self.__elements:
            if not e.HasProperty(prop):
                continue
            el_val = str(e.GetProperty(prop))
            if val is None: # First value encoutered in iteration
                val = el_val
            elif val != el_val: # Differs from last value
                val = MULTIPLE_VALS_STR
                self.SetCellBackgroundColour(index, 0, MULTIPLE_VALS_COLOR)
                break

        self.SetCellValue(index, 0, val)

    # From http://wiki.wxpython.org/wxGrid%20ToolTips
    def __InstallGridHint(self):
        prev_rowcol = [None, None]

        def OnMouseMotion(evt):
            # evt.GetRow() and evt.GetCol() would be nice to have here,
            # but as this is a mouse event, not a grid event, they are not
            # available and we need to compute them by hand.
            x, y = self.CalcUnscrolledPosition(evt.GetPosition())
            row = self.YToRow(y)
            col = self.XToCol(x)

            if (row, col) != prev_rowcol and row >= 0 and col >= 0:
                prev_rowcol[:] = [row, col]
                hinttext = self.GetCellValue(row, col)
                if hinttext is None:
                    hinttext = ''
                if self.IsReadOnly(row, col):
                    hinttext = "(read-only attribute)" + hinttext
                self.GetGridWindow().SetToolTipString(hinttext)
            evt.Skip()

        #wx.EVT_MOTION(self.GetGridWindow(), OnMouseMotion)
        self.GetGridWindow().Bind(wx.EVT_MOTION, OnMouseMotion, id = wx.ID_NONE)

# # Class which displays drop-down-list formatted options when the user edits a cell
class DropdownCellEditor(wx.grid.PyGridCellEditor):

    # # options is a list of options.
    def __init__(self, options):
        wx.grid.GridCellEditor.__init__(self)
        self.__options = options
        self.__chooser = None

    # Event handler for selecting an item
    def ChoiceEvent(self, evt):
        if self.__chooser:
            if self.__event_handler:
                self.__chooser.PushEventHandler(self.__event_handler)
            new_idx = self.__chooser.GetCurrentSelection()
            new_val = self.__chooser.GetString(new_idx)
            self.__chooser.SetSelection(new_idx)
            self.__grid.SetCellValue(self.__row, self.__column, new_val)
            if wx.MAJOR_VERSION == 3:
                grid_evt = wx.grid.GridEvent(id = wx.NewId(), type = wx.grid.EVT_GRID_CELL_CHANGE.typeId, obj = self.__chooser.GetParent(), row = self.__row, col = self.__column)
            else:
                grid_evt = wx.grid.GridEvent(id = wx.NewId(), type = wx.grid.EVT_GRID_CELL_CHANGE.typeId, obj = self.__grid, row = self.__row, col = self.__column)
            wx.PostEvent(self.__chooser.GetParent(), grid_evt)

    # make the selection dialog
    def Create(self, parent, id, event_handler):
        self.__chooser = wx.Choice(parent, id, choices = self.__options)
        self.__chooser.Bind(wx.EVT_CHOICE, self.ChoiceEvent)
        self.SetControl(self.__chooser)

        self.__event_handler = event_handler
        if event_handler:
            self.__chooser.PushEventHandler(event_handler)

    def SetSize(self, rect):
        if self.__chooser:
            if wx.MAJOR_VERSION == 3:
                offset = 0
            else:
                offset = 4
            self.__chooser.SetDimensions(rect.x, rect.y,
                                         rect.width + offset, rect.height + offset,
                                         wx.SIZE_ALLOW_MINUS_ONE)

    def BeginEdit(self, row, column, grid):
        if self.__event_handler:
            self.__chooser.PopEventHandler()
        self.__grid = grid
        self.__row = row
        self.__column = column
        self.__init_val = grid.GetTable().GetValue(row, column)
        self.__chooser.SetStringSelection(self.__init_val)
        self.__chooser.SetFocus()

    def EndEdit(self, row, column, grid, old_val = None, new_val = None):
        if self.__event_handler:
            self.__chooser.PushEventHandler(self.__event_handler)
        new_val = self.__chooser.GetStringSelection()
        if new_val != self.__init_val:
            grid.SetCellValue(row, column, new_val)
            return True
        return False

    def Reset(self):
        self.__chooser.SetStringSelection(self.__init_val)

    def Clone(self):
        return DropdownCellEditor(self.__options)


# # Class which allows user to edit a cell with text entry or a popup window
class PopupCellEditor(wx.grid.PyGridCellEditor):
    __chooser = None
    __popup = None
    __grid = None
    __row = None
    __column = None
    __init_val = None
    __event_handler = None
    __in_update_handler = False

    # The class to use for the popup window is specified in popup_type
    def __init__(self, popup_type = None):
        wx.grid.GridCellEditor.__init__(self)
        self.__chooser = None
        self.__popup_type = popup_type

    # make the selection dialog
    def Create(self, parent, id, event_handler):
        # A ComboCtrl handles the text entry and popup window
        self.__chooser = wx.ComboCtrl(parent, wx.NewId(), "", style = wx.TE_PROCESS_ENTER)
        if self.__popup_type:
            self.SetPopup(self.__popup_type())
        self.SetControl(self.__chooser)
        self.__event_handler = event_handler
        if event_handler:
            self.__chooser.PushEventHandler(event_handler)

    def SetPopup(self, popup):
        self.__popup = popup
        self.__chooser.SetPopupControl(self.__popup)

    def SetSize(self, rect):
        if self.__chooser:
            if wx.MAJOR_VERSION == 3:
                offset = 0
            else:
                offset = 4
            (popup_width, popup_height) = self.__popup.GetControl().GetBestSize()
            self.__chooser.setSize(rect.x, rect.y,
                                         rect.width + offset, rect.height + offset,
                                         wx.SIZE_ALLOW_MINUS_ONE)
            self.__chooser.SetPopupMinWidth(popup_width)

    def BeginEdit(self, row, column, grid):
        if self.__event_handler:
            self.__chooser.PopEventHandler()
        self.__init_val = grid.GetTable().GetValue(row, column)
        self.__grid = grid
        self.__row = row
        self.__column = column
        self.__chooser.SetValue(self.__init_val)
        # wx v2.8 has issues with this
        if wx.MAJOR_VERSION == 3:
            self.__chooser.Bind(wx.EVT_KILL_FOCUS, self.FocusLost)
        self.__chooser.SetFocus()

    def GetChooser(self):
        return self.__chooser

    def GetPopup(self):
        return self.__popup

    def GetGrid(self):
        return self.__grid

    def GetRow(self):
        return self.__row

    def GetColumn(self):
        return self.__column

    def FocusLost(self, evt):
        if evt.GetWindow() != self.__chooser:
            if not self.__in_update_handler:
                self.__in_update_handler = True
                if self.__event_handler:
                    self.__chooser.PushEventHandler(self.__event_handler)
                self.UpdateGrid()
                grid_evt = wx.grid.GridEvent(id = wx.NewId(), type = wx.grid.EVT_GRID_CELL_CHANGE.typeId, obj = self.GetChooser().GetParent(), row = self.GetRow(), col = self.GetColumn())
                wx.PostEvent(self.GetChooser().GetParent(), grid_evt)
            else:
                self.__in_update_handler = False

    def UpdateGrid(self):
        new_val = self.GetChooser().GetValue()
        self.GetGrid().SetCellValue(self.GetRow(), self.GetColumn(), new_val)

    def EndEdit(self, row, column, grid, old_val = None, new_val = None):
        if self.__event_handler:
            self.__chooser.PushEventHandler(self.__event_handler)
        new_val = self.__chooser.GetValue()
        if new_val != self.__init_val:
            grid.SetCellValue(row, column, new_val)
            return True
        return False

    def Reset(self):
        self.__chooser.SetValue(self.__init_val)

    def Clone(self):
        return PopupCellEditor(self.__popup_type)


# Subclass of ComboPopup that allows us to set a flag for whether it should call back to the parent ComboCtrl and update its text
class TextUpdatePopup(wx.ComboPopup):

    def __init__(self):
        wx.ComboPopup.__init__(self)
        self.__should_update_text = True

    def SetUpdateText(self, update_text):
        self.__should_update_text = update_text

    def ShouldUpdateText(self):
        return self.__should_update_text


# Special event fired by an ArgosColorChooser when the color has been changed
ColorChangedEvent, EVT_COLOR_CHANGED_EVENT = wx.lib.newevent.NewEvent()


# Simple subclass of PyColourChooser that also fires a ColorChangedEvent when they color is changed
class ArgosColorChooser(wx.lib.colourchooser.pycolourchooser.PyColourChooser):

    def __init__(self, parent, id):
        super(ArgosColorChooser, self).__init__(parent, id)

    def UpdateColour(self, colour):
        super(ArgosColorChooser, self).UpdateColour(colour)
        evt = ColorChangedEvent()
        evt.SetEventObject(self)
        self.GetEventHandler().ProcessEvent(evt)


# Color chooser popup class
class ColorPopup(TextUpdatePopup):

    def __init__(self):
        TextUpdatePopup.__init__(self)
        self.__colour_chooser = None
        self.__should_update_text = True

    def Create(self, parent):
        self.__colour_chooser = ArgosColorChooser(parent, wx.NewId())
        self.__colour_chooser.Bind(EVT_COLOR_CHANGED_EVENT, self.OnColorChanged)
        return True

    def OnColorChanged(self, evt):
        if self.ShouldUpdateText():
            self.GetCombo().SetText(str(self.__colour_chooser.GetValue().Get()))

    def SetValue(self, color):
        self.__colour_chooser.SetValue(color)

    def GetValue(self):
        return self.__colour_chooser.GetValue()

    def GetControl(self):
        return self.__colour_chooser


# Color cell editor class
class ColorCellEditor(PopupCellEditor):

    def __init__(self):
        PopupCellEditor.__init__(self, ColorPopup)

    # Event handler for selecting a color - updates the GridEditor
    def OnColorPickerChanged(self, evt):
        self.GetChooser().SetText(str(self.GetPopup().GetValue().Get()))
        self.UpdateGrid()

    # make the selection dialog
    def Create(self, parent, id, event_handler):
        super(ColorCellEditor, self).Create(parent, id, event_handler)
        # This event doesn't exist in wx v2.8
        if wx.MAJOR_VERSION == 3:
            self.GetChooser().Bind(wx.EVT_COMBOBOX_CLOSEUP, self.OnColorPickerChanged)
        self.GetChooser().Bind(wx.EVT_TEXT_ENTER, self.OnColorPickerChanged)
        self.GetChooser().Bind(wx.EVT_TEXT, self.OnTextChanged)

    def OnTextChanged(self, evt):
        if evt.GetEventObject() == self.GetChooser():
            # If a valid color tuple has been entered, update the color chooser's value
            try:
                color_tuple = literal_eval(self.GetChooser().GetTextCtrl().GetValue())
                color = wx.Colour()
                color.Set(color_tuple[0], color_tuple[1], color_tuple[2], 255)
                self.GetPopup().SetUpdateText(False)
                self.GetPopup().SetValue(color)
                self.GetPopup().SetUpdateText(True)
            # Otherwise, ignore the entered value
            except:
                pass
        else:
            evt.Skip()

    def Clone(self):
        return ColorCellEditor()


# Tree view popup class
class TreePopup(TextUpdatePopup):

    def __init__(self):
        TextUpdatePopup.__init__(self)
        self.__tree = None
        # Maps full path strings (top.path.to.object) to their respective nodes in the TreeCtrl
        self.__tree_dict = {}
        self.__loc_tree = {}
        self.__loc_added = {}

    # Set the location tree dictionary for this tree view and populate the top level entities
    def SetLocationTree(self, loc_tree):
        self.__loc_tree = loc_tree
        self.__tree.DeleteChildren(self.__root)
        self.__tree_dict = {}
        for key in self.__loc_tree.iterkeys():
            child = self.__tree.AppendItem(self.__root, key)
            if self.__loc_tree[key] != {}:
                self.__tree.SetItemHasChildren(child, True)
            self.__tree_dict[key] = child

    def Create(self, parent):
        self.__tree = wx.TreeCtrl(parent, wx.NewId(), style = wx.TR_HIDE_ROOT | wx.TR_HAS_BUTTONS | wx.TR_SINGLE)
        # Ensure that the tree calculates its best size correctly
        self.__tree.SetQuickBestSize(False)
        # Add the invisible root node
        self.__root = self.__tree.AddRoot('__root')
        self.__tree.Bind(wx.EVT_TREE_ITEM_EXPANDING, self.OnExpandItem)
        self.__tree.Bind(wx.EVT_TREE_ITEM_EXPANDED, self.OnExpandedItem)
        self.__tree.Bind(wx.EVT_TREE_SEL_CHANGED, self.OnSelChanged)
        self.__tree.Bind(wx.EVT_TREE_KEY_DOWN, self.OnKeyDown)
        return True

    # Update the ComboCtrl text box with the selected value in the tree
    def UpdateParentValue(self):
        self.GetCombo().SetValue(self.GetValue())

    def OnSelChanged(self, evt):
        if self.ShouldUpdateText():
            self.UpdateParentValue()

    # If the user presses Enter, update the value and close the popup
    def OnKeyDown(self, evt):
        if evt.GetKeyEvent().GetKeyCode() == wx.WXK_RETURN:
            self.UpdateParentValue()
            self.Dismiss()
        else:
            evt.Skip()

    def GetControl(self):
        return self.__tree

    # Resize the popup to account for what is being shown
    def AutoSize(self):
        (width, height) = self.__tree.GetBestSize()
        self.__tree.SetSize((width, -1))
        self.GetCombo().GetPopupWindow().SetSize((width, -1))

    def OnExpandedItem(self, evt):
        self.AutoSize()

    # This intelligently populates the tree as its nodes are expanded instead of doing it all at once - reduces time to open the popup
    def OnExpandItem(self, evt):
        item = evt.GetItem()
        path = self.__tree.GetItemText(item)
        curdict = self.__loc_tree
        for token in path.split('.'):
            curdict = curdict[token]
        self.SetTree(curdict, item, path)

    def GetValue(self):
        curnode = self.__tree.GetSelection()
        if not curnode.IsOk():
            retval = ''
        else:
            retval = self.__tree.GetItemText(curnode)
        return retval

    def OnPopup(self):
        self.AutoSize()

    # Sets the selected node in the tree
    def SetValue(self, val):
        # If we haven't already added the node for this value
        if val not in self.__tree_dict:
            tokens = val.split('.')
            curdict = self.__loc_tree
            # Iterate through the tokens - if one of them doesn't appear at the correct
            # level of the location tree dictionary, then this is an invalid value and we
            # should stop processing.
            for token in tokens:
                if token not in curdict:
                    return
                curdict = curdict[token]
            # Otherwise, expand the tree from the deepest existing node until all necessary nodes are added
            while val not in self.__tree_dict:
                for i in xrange(len(tokens) - 1, -1, -1):
                    cur_path = '.'.join(tokens[:i])
                    if cur_path in self.__tree_dict:
                        self.__tree.Expand(self.__tree_dict[cur_path])
                        break

        self.__tree.SelectItem(self.__tree_dict[val])

    # Adds every member at the top level of location dictionary "tree" to node "item"
    def SetTree(self, tree, item = None, path = ""):
        if not item:
            item = self.__root
        # Sort keys by length ascending then string-comparison alphabetically
        for k, v in sorted(tree.iteritems(),
                          lambda x, y: cmp(len(x[0]), len(y[0])) if len(x[0]) != len(y[0]) else cmp(x[0], y[0])):
            if len(path) == 0:
                child_path = k
            else:
                child_path = path + '.' + k

            if child_path in self.__tree_dict:
                child = self.__tree_dict[child_path]
            else:
                child = self.__tree.AppendItem(item, child_path)
                self.__tree_dict[child_path] = child
            # Leaf nodes aren't expandable
            if v != {}:
                self.__tree.SetItemHasChildren(child, True)


# Location tree cell editor class
class TreeCellEditor(PopupCellEditor):

    def __init__(self, loc_tree):
        PopupCellEditor.__init__(self, TreePopup)
        self.__loc_tree = loc_tree

    def Create(self, parent, id, event_handler):
        super(TreeCellEditor, self).Create(parent, id, event_handler)
        self.GetPopup().SetLocationTree(self.__loc_tree)
        if wx.MAJOR_VERSION == 3:
            self.GetChooser().Bind(wx.EVT_COMBOBOX_CLOSEUP, self.OnTreeChanged)
        self.GetChooser().Bind(wx.EVT_TEXT, self.OnTextChanged)
        self.GetChooser().Bind(wx.EVT_TEXT_ENTER, self.OnTreeChanged)

    def Clone(self):
        return TreeCellEditor()

    def OnTreeChanged(self, evt):
        self.GetChooser().SetValue(self.GetPopup().GetValue())
        self.UpdateGrid()

    def OnTextChanged(self, evt):
        if evt.GetEventObject() == self.GetChooser():
            self.GetPopup().SetUpdateText(False)
            self.GetPopup().SetValue(self.GetChooser().GetTextCtrl().GetValue())
            self.GetPopup().SetUpdateText(True)
        else:
            evt.Skip()
