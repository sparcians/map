from __future__ import annotations
import wx
import wx.grid
from wx.lib.colourchooser.pycolourchooser import ColourChangedEvent, PyColourChooser, EVT_COLOUR_CHANGED
import model.content_options as copts
from model.schedule_element import ScheduleLineElement
from model.element import Element, LocationallyKeyedElement
from model.rpc_element import RPCElement
from ast import literal_eval
from functools import cmp_to_key
from typing import Dict, List, Optional, Tuple, Type, Union, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from gui.dialogs.element_propsdlg import Element_PropsDlg
    from gui.layout_frame import Layout_Frame
    from gui.selection_manager import Selection_Mgr
    from model.location_manager import LocationTree

MULTIPLE_VALS_STR = '<multiple values>'
MULTIPLE_VALS_COLOR = wx.Colour(200, 200, 100)


# # Grid descendent used to display and edit element properties
class ElementPropertyList(wx.grid.Grid):

    def __init__(self, parent: Element_PropsDlg, id: int) -> None:
        self.__in_resize = False

        wx.grid.Grid.__init__(self, parent, id)
        self.__elements: List[Element] = []
        self.__keys: List[str] = []
        # population function (SetElements) is working
        self.__internal_editing = False
        self.__parent = parent
        self.__sel_mgr: Optional[Selection_Mgr] = None

        # make the graphical stuff
        self.CreateGrid(1, 1)
        self.SetColLabelSize(0)
        self.SetRowLabelSize(150)

        self.Bind(wx.grid.EVT_GRID_CELL_CHANGED, self.OnCellChange)
        self.Bind(wx.EVT_SIZE, self.OnResize)
        self.__db = cast('Layout_Frame', self.__parent.GetParent()).GetContext().dbhandle.database

        self.__InstallGridHint()

    def OnResize(self, evt: wx.SizeEvent) -> None:
        # #self.AutoSizeColumn(0, False)
        remaining = max(0, self.GetClientSize()[0] - self.GetRowLabelSize())

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
    def SetElements(self, elements: List[Element], sel_mgr: Selection_Mgr) -> None:
        self.__elements = elements[:]
        self.__sel_mgr = sel_mgr
        if self.__elements:
            # show intersection of all current selected properties
            props_set = set(self.__elements[0].GetProperties().keys())
            hidden_props_set = set(self.__elements[0].GetHiddenProperties())
            read_only_props_set = set(self.__elements[0].GetReadOnlyProperties())
            # #props_set.difference_update(self.__elements[0].GetHiddenProperties())
            for i in range(1, len(self.__elements)):
                el = self.__elements[i]
                # #props_set.intersection_update(el.GetElementProperties())
                props_set.union(el.GetElementProperties())
                # #props_set.difference_update(self.__elements[i].GetHiddenProperties())
                hidden_props_set.intersection_update(el.GetHiddenProperties())
                read_only_props_set.intersection_update(el.GetReadOnlyProperties())

            # Remove hidden keys
            props_set.difference_update(hidden_props_set)
            read_only_props_set.difference_update(hidden_props_set)
            props = list(props_set)
            hidden_props = list(hidden_props_set)
            read_only_props = list(read_only_props_set)
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
                            DropdownCellEditor(list(ScheduleLineElement.DRAW_LOOKUP.keys())))
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

    def OnCellChange(self, evt: wx.grid.GridEvent) -> None:
        row = evt.GetRow()

        value = self.GetCellValue(row, 0)
        # hopefully whatever the user input is valid... (data will pass
        # through the validation steps on the Element side)

        # if the population function isn't calling
        if not self.__internal_editing:
            k = self.__keys[row]
            # Begin checkpoint including all elements selected so that selection
            # is correct checkpoint apply/remove
            if self.__sel_mgr is not None:
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
                if self.__sel_mgr is not None:
                    self.__sel_mgr.CommitCheckpoint(force_elements = self.__elements)

    def GetNumberOfElements(self) -> int:
        return len(self.__elements)

    # # override read-only setter to gray out text
    def SetReadOnly(self, row: int, col: int, is_read_only: bool = True) -> None:
        wx.grid.Grid.SetReadOnly(self, row, col, is_read_only)
        if is_read_only:
            color = (128, 128, 128)
        else:
            color = (0, 0, 0)
        self.SetCellTextColour(row, col, color)

    def __UpdateItem(self, index: int) -> None:
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
    def __InstallGridHint(self) -> None:
        prev_rowcol = [None, None]

        def OnMouseMotion(evt: wx.MouseEvent) -> None:
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
                self.GetGridWindow().SetToolTip(hinttext)
            evt.Skip()

        #wx.EVT_MOTION(self.GetGridWindow(), OnMouseMotion)
        self.GetGridWindow().Bind(wx.EVT_MOTION, OnMouseMotion, id = wx.ID_NONE)

# # Class which displays drop-down-list formatted options when the user edits a cell
class DropdownCellEditor(wx.grid.GridCellEditor):

    # # options is a list of options.
    def __init__(self, options: List[str]) -> None:
        wx.grid.GridCellEditor.__init__(self)
        self.__options = options
        self.__chooser: Optional[wx.Choice] = None

    # Event handler for selecting an item
    def ChoiceEvent(self, evt: wx.CommandEvent) -> None:
        if self.__chooser is not None:
            if self.__event_handler:
                self.__chooser.PushEventHandler(self.__event_handler)
            new_idx = self.__chooser.GetCurrentSelection()
            new_val = self.__chooser.GetString(new_idx)
            self.__chooser.SetSelection(new_idx)
            self.__grid.SetCellValue(self.__row, self.__column, new_val)
            grid_evt = wx.grid.GridEvent(id = wx.NewId(), type = wx.grid.EVT_GRID_CELL_CHANGED.typeId, obj = self.__grid, row = self.__row, col = self.__column)
            wx.PostEvent(self.__chooser.GetParent(), grid_evt)

    # make the selection dialog
    def Create(self, parent: ElementPropertyList, id: int, event_handler: wx.EvtHandler) -> None:
        self.__chooser = wx.Choice(parent, id, choices = self.__options)
        self.__chooser.Bind(wx.EVT_CHOICE, self.ChoiceEvent)
        self.SetControl(self.__chooser)

        self.__event_handler = event_handler
        if event_handler:
            self.__chooser.PushEventHandler(event_handler)

    def SetSize(self, rect: wx.Rect) -> None:
        if self.__chooser is not None:
            offset = 4
            self.__chooser.SetSize(rect.x, rect.y,
                                   rect.width + offset, rect.height + offset,
                                   wx.SIZE_ALLOW_MINUS_ONE)

    def BeginEdit(self, row: int, column: int, grid: wx.grid.Grid) -> None:
        assert self.__chooser is not None
        if self.__event_handler:
            self.__chooser.PopEventHandler()
        self.__grid = grid
        self.__row = row
        self.__column = column
        self.__init_val: str = cast(str, grid.GetTable().GetValue(row, column))
        self.__chooser.SetStringSelection(self.__init_val)
        self.__chooser.SetFocus()

    def ApplyEdit(self, row: int, column: int, grid: wx.grid.Grid) -> None:
        grid.SetCellValue(row, column, self.__new_val)

    def EndEdit(self,
                row: int,
                column: int,
                grid: wx.grid.Grid,
                old_val: Optional[str] = None,
                new_val: Optional[str] = None) -> Optional[str]:
        assert self.__chooser is not None
        if self.__event_handler:
            self.__chooser.PushEventHandler(self.__event_handler)
        new_val = self.__chooser.GetStringSelection()
        if new_val != self.__init_val:
            self.__new_val = new_val
            return str(new_val)
        return None

    def Reset(self) -> None:
        assert self.__chooser is not None
        self.__chooser.SetStringSelection(self.__init_val)

    def Clone(self) -> DropdownCellEditor:
        return DropdownCellEditor(self.__options)


# # Class which allows user to edit a cell with text entry or a popup window
class PopupCellEditor(wx.grid.GridCellEditor):
    __chooser = None
    __popup = None
    __grid = None
    __row = None
    __column = None
    __init_val = ''
    __event_handler = None
    __in_update_handler = False

    # The class to use for the popup window is specified in popup_type
    def __init__(self, popup_type: Optional[Type[wx.ComboPopup]] = None) -> None:
        wx.grid.GridCellEditor.__init__(self)
        self.__chooser = None
        self.__popup_type = popup_type

    # make the selection dialog
    def Create(self, parent: ElementPropertyList, id: int, event_handler: wx.EvtHandler) -> None:
        # A ComboCtrl handles the text entry and popup window
        self.__chooser = wx.ComboCtrl(parent, wx.NewId(), "", style = wx.TE_PROCESS_ENTER)
        if self.__popup_type is not None:
            self.SetPopup(self.__popup_type())
        self.SetControl(self.__chooser)
        self.__event_handler = event_handler
        if event_handler is not None:
            self.__chooser.PushEventHandler(event_handler)
        self.__chooser.Bind(wx.EVT_KILL_FOCUS, self.FocusLost)

    def SetPopup(self, popup: wx.ComboPopup) -> None:
        self.__popup = popup
        assert self.__chooser is not None
        self.__chooser.SetPopupControl(self.__popup)

    def SetSize(self, rect: wx.Rect) -> None:
        if self.__chooser is not None:
            offset = 4
            assert self.__popup is not None
            (popup_width, popup_height) = self.__popup.GetControl().GetBestSize()
            self.__chooser.SetSize(rect.x, rect.y,
                                   rect.width + offset, rect.height + offset,
                                   wx.SIZE_ALLOW_MINUS_ONE)
            self.__chooser.SetPopupMinWidth(popup_width)

    def BeginEdit(self, row: int, column: int, grid: wx.grid.Grid) -> None:
        assert self.__chooser is not None
        if self.__event_handler is not None:
            self.__chooser.PopEventHandler()
        self.__init_val = cast(str, grid.GetTable().GetValue(row, column))
        self.__grid = grid
        self.__row = row
        self.__column = column
        self.__chooser.SetValue(self.__init_val)
        # wx v2.8 has issues with this
        self.__chooser.SetFocus()

    def GetChooser(self) -> Optional[wx.ComboCtrl]:
        return self.__chooser

    def GetPopup(self) -> Optional[wx.ComboPopup]:
        return self.__popup

    def GetGrid(self) -> Optional[wx.grid.Grid]:
        return self.__grid

    def GetRow(self) -> Optional[int]:
        return self.__row

    def GetColumn(self) -> Optional[int]:
        return self.__column

    def FocusLost(self, evt: wx.FocusEvent) -> None:
        if evt.GetWindow() != self.__chooser:
            if not self.__in_update_handler:
                self.__in_update_handler = True
                assert self.__chooser is not None
                if self.__event_handler:
                    self.__chooser.PushEventHandler(self.__event_handler)
                self.UpdateGrid()
                grid_evt = wx.grid.GridEvent(id = wx.NewId(),
                                             type = wx.grid.EVT_GRID_CELL_CHANGED.typeId,
                                             obj = self.__chooser.GetParent(),
                                             row = self.GetRow(),
                                             col = self.GetColumn())
                wx.PostEvent(self.__chooser.GetParent(), grid_evt)
            else:
                self.__in_update_handler = False

    def UpdateGrid(self) -> None:
        assert self.__chooser is not None
        new_val = self.__chooser.GetValue()
        assert self.__grid is not None
        self.__grid.SetCellValue(self.GetRow(), self.GetColumn(), new_val)

    def ApplyEdit(self, row: int, column: int, grid: wx.grid.Grid) -> None:
        grid.SetCellValue(row, column, self.__new_val)

    def EndEdit(self,
                row: int,
                column: int,
                grid: wx.grid.Grid,
                old_val: Optional[str] = None,
                new_val: Optional[str] = None) -> Optional[str]:
        assert self.__chooser is not None
        if self.__event_handler:
            self.__chooser.PushEventHandler(self.__event_handler)
        new_val = self.__chooser.GetValue()
        if new_val != self.__init_val:
            self.__new_val = new_val
            return str(new_val)
        return None

    def Reset(self) -> None:
        assert self.__chooser is not None
        self.__chooser.SetValue(self.__init_val)

    def Clone(self) -> PopupCellEditor:
        return PopupCellEditor(self.__popup_type)


# Subclass of ComboPopup that allows us to set a flag for whether it should call back to the parent ComboCtrl and update its text
class TextUpdatePopup(wx.ComboPopup):

    def __init__(self) -> None:
        wx.ComboPopup.__init__(self)
        self.__should_update_text = True

    def SetUpdateText(self, update_text: bool) -> None:
        self.__should_update_text = update_text

    def ShouldUpdateText(self) -> bool:
        return self.__should_update_text


# Color chooser popup class
class ColorPopup(TextUpdatePopup):

    def __init__(self) -> None:
        TextUpdatePopup.__init__(self)
        self.__colour_chooser: Optional[PyColourChooser] = None
        self.__should_update_text = True

    def Create(self, parent: ElementPropertyList) -> bool:
        self.__colour_chooser = PyColourChooser(parent, wx.NewId())
        self.__colour_chooser.Bind(EVT_COLOUR_CHANGED, self.OnColorChanged)
        return True

    def OnColorChanged(self, evt: ColourChangedEvent) -> None:
        if self.ShouldUpdateText():
            assert self.__colour_chooser is not None
            self.GetComboCtrl().SetText(str(self.__colour_chooser.GetValue().Get(includeAlpha=False)))

    def SetValue(self, color: wx.Colour) -> None:
        assert self.__colour_chooser is not None
        self.__colour_chooser.SetValue(color)

    def GetValue(self) -> wx.Colour:
        assert self.__colour_chooser is not None
        return self.__colour_chooser.GetValue()

    def GetControl(self) -> wx.Window:
        assert self.__colour_chooser is not None
        return self.__colour_chooser


# Color cell editor class
class ColorCellEditor(PopupCellEditor):

    def __init__(self) -> None:
        PopupCellEditor.__init__(self, ColorPopup)

    # Event handler for selecting a color - updates the GridEditor
    def OnColorPickerChanged(self, evt: wx.CommandEvent) -> None:
        chooser = self.GetChooser()
        assert chooser is not None
        popup = self.GetPopup()
        assert popup is not None
        popup = cast(ColorPopup, popup)
        chooser.SetText(str(popup.GetValue().Get()))
        self.UpdateGrid()

    # make the selection dialog
    def Create(self, parent: ElementPropertyList, id: int, event_handler: wx.EvtHandler) -> None:
        super().Create(parent, id, event_handler)
        chooser = self.GetChooser()
        assert chooser is not None
        # This event doesn't exist in wx v2.8
        chooser.Bind(wx.EVT_TEXT_ENTER, self.OnColorPickerChanged)
        chooser.Bind(wx.EVT_TEXT, self.OnTextChanged)

    def OnTextChanged(self, evt: wx.CommandEvent) -> None:
        chooser = self.GetChooser()
        if evt.GetEventObject() == chooser:
            # If a valid color tuple has been entered, update the color chooser's value
            try:
                assert chooser is not None
                color_tuple = literal_eval(chooser.GetTextCtrl().GetValue())
                color = wx.Colour()
                color.Set(color_tuple[0], color_tuple[1], color_tuple[2], 255)
                popup = self.GetPopup()
                assert popup is not None
                popup = cast(ColorPopup, popup)
                popup.SetUpdateText(False)
                popup.SetValue(color)
                popup.SetUpdateText(True)
            # Otherwise, ignore the entered value
            except:
                pass
        else:
            evt.Skip()

    def Clone(self) -> ColorCellEditor:
        return ColorCellEditor()


# Tree view popup class
class TreePopup(TextUpdatePopup):

    def __init__(self) -> None:
        TextUpdatePopup.__init__(self)
        self.__tree: Optional[wx.TreeCtrl] = None
        # Maps full path strings (top.path.to.object) to their respective nodes in the TreeCtrl
        self.__tree_dict: Dict[str, wx.TreeItemId] = {}
        self.__loc_tree: LocationTree = {}

    # Set the location tree dictionary for this tree view and populate the top level entities
    def SetLocationTree(self, loc_tree: LocationTree) -> None:
        self.__loc_tree = loc_tree
        assert self.__tree is not None
        self.__tree.DeleteChildren(self.__root)
        self.__tree_dict = {}
        for key in self.__loc_tree.keys():
            child = self.__tree.AppendItem(self.__root, key)
            if self.__loc_tree[key] != {}:
                self.__tree.SetItemHasChildren(child, True)
            self.__tree_dict[key] = child

    def Create(self, parent: ElementPropertyList) -> bool:
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
    def UpdateParentValue(self) -> None:
        self.GetComboCtrl().SetValue(self.GetValue())

    def OnSelChanged(self, evt: wx.TreeEvent) -> None:
        if self.ShouldUpdateText():
            self.UpdateParentValue()

    # If the user presses Enter, update the value and close the popup
    def OnKeyDown(self, evt: wx.TreeEvent) -> None:
        if evt.GetKeyEvent().GetKeyCode() == wx.WXK_RETURN:
            self.UpdateParentValue()
            self.Dismiss()
        else:
            evt.Skip()

    def GetControl(self) -> wx.Window:
        assert self.__tree is not None
        return self.__tree

    # Resize the popup to account for what is being shown
    def AutoSize(self) -> None:
        assert self.__tree is not None
        (width, height) = self.__tree.GetBestSize()
        self.__tree.SetSize((width, -1))
        self.GetComboCtrl().GetPopupWindow().SetSize((width, -1))

    def OnExpandedItem(self, evt: wx.TreeEvent) -> None:
        self.AutoSize()

    # This intelligently populates the tree as its nodes are expanded instead of doing it all at once - reduces time to open the popup
    def OnExpandItem(self, evt: wx.TreeEvent) -> None:
        item = evt.GetItem()
        assert self.__tree is not None
        path = self.__tree.GetItemText(item)
        curdict = self.__loc_tree
        for token in path.split('.'):
            curdict = curdict[token]
        self.SetTree(curdict, item, path)

    def GetValue(self) -> str:
        assert self.__tree is not None
        curnode = self.__tree.GetSelection()
        if not curnode.IsOk():
            retval = ''
        else:
            retval = self.__tree.GetItemText(curnode)
        return retval

    def OnPopup(self) -> None:
        self.AutoSize()

    # Sets the selected node in the tree
    def SetValue(self, val: str) -> None:
        assert self.__tree is not None
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
                for i in range(len(tokens) - 1, -1, -1):
                    cur_path = '.'.join(tokens[:i])
                    if cur_path in self.__tree_dict:
                        self.__tree.Expand(self.__tree_dict[cur_path])
                        break

        self.__tree.SelectItem(self.__tree_dict[val])

    # Adds every member at the top level of location dictionary "tree" to node "item"
    def SetTree(self, tree: LocationTree, item: Optional[wx.TreeItemId] = None, path: str = "") -> None:
        if not item:
            item = self.__root

        def cmp_pair(x: Tuple[str, LocationTree], y: Tuple[str, LocationTree]) -> int:
            # Fix missing cmp() function in Python 3
            def cmp(a: Union[int, str], b: Union[int, str]) -> int:
                if isinstance(a, int):
                    assert isinstance(b, int)
                    return (a > b) - (a < b)
                else:
                    assert isinstance(b, str)
                    return (a > b) - (a < b)

            return cmp(len(x[0]), len(y[0])) if len(x[0]) != len(y[0]) else cmp(x[0], y[0])

        # Sort keys by length ascending then string-comparison alphabetically
        for k, v in sorted(tree.items(),
                           key=cmp_to_key(cmp_pair)):
            if not path:
                child_path = k
            else:
                child_path = path + '.' + k

            assert self.__tree is not None
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

    def __init__(self, loc_tree: LocationTree) -> None:
        PopupCellEditor.__init__(self, TreePopup)
        self.__loc_tree = loc_tree

    def Create(self, parent: ElementPropertyList, id: int, event_handler: wx.EvtHandler) -> None:
        super().Create(parent, id, event_handler)
        popup = self.GetPopup()
        assert popup is not None
        popup = cast(TreePopup, popup)
        popup.SetLocationTree(self.__loc_tree)
        chooser = self.GetChooser()
        assert chooser is not None
        chooser.Bind(wx.EVT_TEXT, self.OnTextChanged)
        chooser.Bind(wx.EVT_TEXT_ENTER, self.OnTreeChanged)

    def Clone(self) -> TreeCellEditor:
        return TreeCellEditor(self.__loc_tree.copy())

    def OnTreeChanged(self, evt: wx.CommandEvent) -> None:
        chooser = self.GetChooser()
        assert chooser is not None
        popup = self.GetPopup()
        assert popup is not None
        popup = cast(TreePopup, popup)
        chooser.SetValue(popup.GetValue())
        self.UpdateGrid()

    def OnTextChanged(self, evt: wx.CommandEvent) -> None:
        chooser = self.GetChooser()
        if evt.GetEventObject() == chooser:
            assert chooser is not None
            popup = self.GetPopup()
            assert popup is not None
            popup = cast(TreePopup, popup)
            popup.SetUpdateText(False)
            popup.SetValue(chooser.GetTextCtrl().GetValue())
            popup.SetUpdateText(True)
        else:
            evt.Skip()
