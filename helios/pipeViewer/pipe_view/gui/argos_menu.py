

import wx
import wx.adv
import os
import logging

from misc.version import get_version
from model.layout import Layout
from model.schedule_element import ScheduleLineElement
from .hover_preview import HoverPreviewOptionsDialog
from .dialogs.element_propsdlg import ElementTypeSelectionDialog
from .dialogs.layout_varsdlg import LayoutVariablesDialog
from .dialogs.watchlist_dialog import WatchListDlg
from .dialogs.console_dialog import ConsoleDlg
from .dialogs.select_layout_dlg import SelectLayoutDlg
from .dialogs.translate_elements_dlg import TranslateElementsDlg
from .dialogs.view_settings_dlg import ViewSettingsDialog
from .dialogs.shortcut_help import ShortcutHelp

# Name each ID by ID_MENU_SUBMENU_etc...
ID_FILE_NEW = wx.NewId()
ID_FILE_OPEN = wx.NewId()
ID_FILE_CLOSE = wx.NewId()
ID_FILE_QUIT = wx.NewId()
ID_FILE_IMPORT_PLAIN = wx.NewId()
ID_FILE_SAVE = wx.NewId()
ID_FILE_SAVEAS = wx.NewId()
ID_FILE_OPTIONS = wx.NewId()

ID_HELP_ABOUT = wx.NewId()

ID_SHORTCUT_HELP = wx.NewId()

# Undo/Redo menu string templates.
# Render as 'Undo [num available] (next action)\thotkey'
UNDO_FMT = 'Undo {} ({} remaining)\tCTRL+Z'
REDO_FMT = 'Redo {} ({} until current)\tCTRL+Y'
REDO_ALL_FMT = 'Redo All ({})\tCTRL+SHIFT+Y'


# # The menubar displayed within each Layout Frame that will have options to
#  interact with a Layout, Layout Context, Workspace, etc.
class Argos_Menu(wx.MenuBar):

    # # Set up all the menus and embedded sub-menus, with all their bindings/callbacks
    def __init__(self, frame, layout, update_enabled):
        this_script_filename = os.path.join(os.getcwd(), __file__)

        self.__parent = frame
        self.__layout = layout
        wx.MenuBar.__init__(self)
        accelentries = []

        self.__selection = self.__parent.GetCanvas().GetSelectionManager()
        self.__selection.AddUndoRedoHook(self.__OnUndoRedo)

        # keeps track of the last location a graph was imported from
        self.__last_loaded_graph_dir = None
        self.__shortcut_help_dlg = None

        # Setting up the menu(s).
        filemenu = wx.Menu()
        self.__editmenu = wx.Menu()

        # sub menus of edit
        selectmenu = wx.Menu()
        arrangemenu = wx.Menu()
        snappingmenu = wx.Menu()

        toolsmenu = wx.Menu()
        viewmenu = wx.Menu()
        dbmenu = wx.Menu()
        helpmenu = wx.Menu()

        # Accessibility sub menu
        accessibilitymenu = wx.Menu()

        # File

        menuNew = filemenu.Append(ID_FILE_NEW, "&New Layout", " Open a blank new frame")
        menuOpen = filemenu.Append(ID_FILE_OPEN, "&Open Layout", " Open a layout file in a new frame")

        menuSaveAs = filemenu.Append(ID_FILE_SAVEAS, "&Save Layout As\tCTRL+SHIFT+S", " Save this layout to a new file")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, ord('S'), menuSaveAs.GetId()))

        menuSave = filemenu.Append(ID_FILE_SAVE, "&Save Layout\tCTRL+S", " Save this layout to disk")
        accelentries.append((wx.ACCEL_CTRL, ord('S'), menuSave.GetId()))

        filemenu.AppendSeparator()

        menuOptions = filemenu.Append(ID_FILE_OPTIONS, "&Preferences (not implemented)", " User Preferences")
        menuOptions.Enable(False)

        filemenu.AppendSeparator()

        menuClose = filemenu.Append(ID_FILE_CLOSE, "&Close Frame\tCTRL+W", " Close this Frame")
        accelentries.append((wx.ACCEL_CTRL, ord('W'), menuClose.GetId()))

        menuQuit = filemenu.Append(ID_FILE_QUIT, "&Quit Argos\tCTRL+Q", " Quit Argos Entirely")
        accelentries.append((wx.ACCEL_CTRL, ord('Q'), menuQuit.GetId()))

        # Settings
        self.menuMoveSnap = snappingmenu.Append(wx.NewId(), "Snap during Move ops", kind = wx.ITEM_CHECK)
        self.menuMDominant = snappingmenu.Append(wx.NewId(), "Snap Dominant", "Selection snaps as a group, based on Element beneath mouse", wx.ITEM_RADIO)
        self.menuMEach = snappingmenu.Append(wx.NewId(), "Snap Each", "Each Element in selection freely snaps on its own", wx.ITEM_RADIO)
        snappingmenu.AppendSeparator()
        self.menuResizeSnap = snappingmenu.Append(wx.NewId(), "Snap during Resize ops", kind = wx.ITEM_CHECK)
        self.menuRDominant = snappingmenu.Append(wx.NewId(), "Snap dominant", "", wx.ITEM_RADIO)
        self.menuRDominant.Enable(False) # Don't have a clue how to make this work smoothly.
        self.menuREach = snappingmenu.Append(wx.NewId(), "Snap each", "", wx.ITEM_RADIO)

        snappingmenu.Check(self.menuMoveSnap.GetId(), True)
        snappingmenu.Check(self.menuResizeSnap.GetId(), True)
        snappingmenu.Check(self.menuREach.GetId(), True)

        # Select

        menuSelectAll = selectmenu.Append(wx.NewId(), "Select All\tCTRL+A", " Select all elements in the layout")
        accelentries.append((wx.ACCEL_CTRL, ord('A'), menuSelectAll.GetId()))

        menuInvertSelection = selectmenu.Append(wx.NewId(), "Invert Selection\tCTRL+I", " Toggle selection state of all elements")
        accelentries.append((wx.ACCEL_CTRL, ord('I'), menuInvertSelection.GetId()))

        # Arrange

        menuColumnUp = arrangemenu.Append(wx.NewId(), "Column from top\tCTRL+ALT+UP", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT, wx.WXK_UP, menuColumnUp.GetId()))

        menuColumnDown = arrangemenu.Append(wx.NewId(), "Column from bottom\tCTRL+ALT+DOWN", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT, wx.WXK_DOWN, menuColumnDown.GetId()))

        menuRowLeft = arrangemenu.Append(wx.NewId(), "Row from leftmost\tCTRL+ALT+LEFT", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT, wx.WXK_LEFT, menuRowLeft.GetId()))

        menuRowRight = arrangemenu.Append(wx.NewId(), "Row from rightmost\tCTRL+ALT+RIGHT", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT, wx.WXK_RIGHT, menuRowRight.GetId()))

        arrangemenu.AppendSeparator()
        menuAlignTop = arrangemenu.Append(wx.NewId(), "Align top edges\tCTRL+SHIFT+UP", "Can result in overlap")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_UP, menuAlignTop.GetId()))

        menuAlignBottom = arrangemenu.Append(wx.NewId(), "Align bottom edges\tCTRL+SHIFT+DOWN", "Can result in overlap")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_DOWN, menuAlignBottom.GetId()))

        menuAlignLeft = arrangemenu.Append(wx.NewId(), "Align left edges\tCTRL+SHIFT+LEFT", "Can result in overlap")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_LEFT, menuAlignLeft.GetId()))

        menuAlignRight = arrangemenu.Append(wx.NewId(), "Align right edges\tCTRL+SHIFT+RIGHT", "Can result in overlap")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_RIGHT, menuAlignRight.GetId()))

        arrangemenu.AppendSeparator()
        menuFlipHoriz = arrangemenu.Append(wx.NewId(), "Flip Horizontally\tHOME", "")
        accelentries.append((wx.ACCEL_NORMAL, wx.WXK_HOME, menuFlipHoriz.GetId()))

        menuFlipVert = arrangemenu.Append(wx.NewId(), "Flip Vertically\tPGDN", "")
        accelentries.append((wx.ACCEL_NORMAL, wx.WXK_PAGEDOWN, menuFlipVert.GetId()))

        arrangemenu.AppendSeparator()
        menuMoveToTop = arrangemenu.Append(wx.NewId(), "Move to Front\tCTRL+SHIFT+PGUP", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_PAGEUP, menuMoveToTop.GetId()))

        menuMoveToBottom = arrangemenu.Append(wx.NewId(), "Move to Back\tCTRL+SHIFT+PGDN", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, wx.WXK_PAGEDOWN, menuMoveToBottom.GetId()))

        menuMoveUp = arrangemenu.Append(wx.NewId(), "Move Forward\tCTRL+ALT+SHIFT+PGUP", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT | wx.ACCEL_SHIFT, wx.WXK_PAGEUP, menuMoveUp.GetId()))

        menuMoveDown = arrangemenu.Append(wx.NewId(), "Move Backward\tCTRL+ALT+SHIFT+PGDN", "")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_ALT | wx.ACCEL_SHIFT, wx.WXK_PAGEDOWN, menuMoveDown.GetId()))

        # Tools
        menuImportPlain = toolsmenu.Append(ID_FILE_IMPORT_PLAIN, "Import Graph", " Import Neato Plain File")

        # Edit
        self.menuEditMode = self.__editmenu.Append(wx.NewId(), "Edit mode\tCTRL+M", kind = wx.ITEM_CHECK)
        # Checkable menu items don't get toggled by keyboard shortcuts.
        # So, we assign the shortcut to a different ID/handler that toggles the menu item then calls the appropriate handler.
        menuEditModeKeyShortcutId = wx.NewId()
        accelentries.append((wx.ACCEL_CTRL, ord('M'), menuEditModeKeyShortcutId))

        self.__editmenu.Check(self.menuEditMode.GetId(), False)
        self.__editmenu.AppendSeparator()

        self.menuUndo = self.__editmenu.Append(wx.NewId(), UNDO_FMT.format('', 0), " Undo last action");
        accelentries.append((wx.ACCEL_CTRL, ord('Z'), self.menuUndo.GetId()))
        self.menuUndo.Enable(False)

        self.menuRedo = self.__editmenu.Append(wx.NewId(), REDO_FMT.format('', 0), " Redo last action");
        accelentries.append((wx.ACCEL_CTRL, ord('Y'), self.menuRedo.GetId()))
        self.menuRedo.Enable(False)

        self.menuRedoAll = self.__editmenu.Append(wx.NewId(), REDO_ALL_FMT.format(0), " Redo all");
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, ord('Y'), self.menuRedoAll.GetId()))
        self.menuRedoAll.Enable(False)

        self.__editmenu.AppendSeparator()
        self.__editmenu.AppendSubMenu(selectmenu, "Select")
        self.__editmenu.AppendSubMenu(arrangemenu, "Arrange")
        self.__editmenu.AppendSubMenu(snappingmenu, "Snapping")
        self.__editmenu.AppendSeparator()
        menuNewEl = self.__editmenu.Append(wx.NewId(), "New &Element\tCTRL+E", " Create a new element");
        accelentries.append((wx.ACCEL_CTRL, ord('E'), menuNewEl.GetId()))

        menuCloneEl = self.__editmenu.Append(wx.NewId(), "&Duplicate Elements\tCTRL+D", " Clones the selected elements");
        accelentries.append((wx.ACCEL_CTRL, ord('D'), menuCloneEl.GetId()))

        menuDeleteEl = self.__editmenu.Append(wx.NewId(), "Delete Elements\tDel", " Deletes the selected elements");
        accelentries.append((wx.ACCEL_NORMAL, wx.WXK_DELETE, menuDeleteEl.GetId()))

        self.__editmenu.AppendSeparator()
        menuTranslate = self.__editmenu.Append(wx.NewId(), "Translate Selection...\tT", " Moves a set of elements")
        accelentries.append((wx.ACCEL_NORMAL, ord('T'), menuTranslate.GetId()))

        menuAvg = self.__editmenu.Append(wx.NewId(), "Average Dimensions\tSHIFT+A", " Averages out the dimensions of each element")
        accelentries.append((wx.ACCEL_SHIFT, ord('A'), menuAvg.GetId()))

        menuIndRight = self.__editmenu.Append(wx.NewId(), "Add Spacing Right\tALT+END", " Spaces out the elements")
        accelentries.append((wx.ACCEL_ALT, wx.WXK_END, menuIndRight.GetId()))

        menuIndLeft = self.__editmenu.Append(wx.NewId(), "Add Spacing Left\tALT+HOME", " Spaces out the elements")
        accelentries.append((wx.ACCEL_ALT, wx.WXK_HOME, menuIndLeft.GetId()))

        menuIndUp = self.__editmenu.Append(wx.NewId(), "Add Spacing Up\tALT+PGUP", " Spaces out the elements")
        accelentries.append((wx.ACCEL_ALT, wx.WXK_PAGEUP, menuIndUp.GetId()))

        menuIndDown = self.__editmenu.Append(wx.NewId(), "Add Spacing Down\tALT+PGDN ", " Spaces out the elements")
        accelentries.append((wx.ACCEL_ALT, wx.WXK_PAGEDOWN, menuIndDown.GetId()))

        menuSubtIndRight = self.__editmenu.Append(wx.NewId(), "Remove Spacing Right\tCTRL+END", " Spaces out the elements")
        accelentries.append((wx.ACCEL_CTRL, wx.WXK_END, menuSubtIndRight.GetId()))

        menuSubtIndLeft = self.__editmenu.Append(wx.NewId(), "Remove Spacing Left\tCTRL+HOME", " Spaces out the elements")
        accelentries.append((wx.ACCEL_CTRL, wx.WXK_HOME, menuSubtIndLeft.GetId()))

        menuSubtIndUp = self.__editmenu.Append(wx.NewId(), "Remove Spacing Up\tCTRL+PGUP", " Spaces out the elements")
        accelentries.append((wx.ACCEL_CTRL, wx.WXK_PAGEUP, menuSubtIndUp.GetId()))

        menuSubtIndDown = self.__editmenu.Append(wx.NewId(), "Remove Spacing Down\tCTRL+PGDN ", " Spaces out the elements")
        accelentries.append((wx.ACCEL_CTRL, wx.WXK_PAGEDOWN, menuSubtIndDown.GetId()))

# Database

        menuShowLocations = dbmenu.Append(wx.NewId(), "Open Locations List...\tCTRL+L", "List of locations which can be referenced in this database")
        accelentries.append((wx.ACCEL_CTRL, ord('L'), menuShowLocations.GetId()))

        menuSearch = dbmenu.Append(wx.NewId(), "Search Database\tCTRL+F", "Search for strings in the database annotation.")
        accelentries.append((wx.ACCEL_CTRL, ord('F'), menuSearch.GetId()))

        self.menuPollDB = dbmenu.Append(wx.NewId(), "Poll Database For Updates\tCTRL+U", kind = wx.ITEM_CHECK)
        menuPollDBKeyShortcutId = wx.NewId()
        accelentries.append((wx.ACCEL_CTRL, ord('U'), menuPollDBKeyShortcutId))
        if update_enabled:
            self.menuPollDB.Check()

        menuRefreshDB = dbmenu.Append(wx.NewId(), "Refresh Database\tF5")
        accelentries.append((wx.ACCEL_NORMAL, wx.WXK_F5, menuRefreshDB.GetId()))

        # list of objects in edit menu that shouldn't be toggled between edit and playback modes
        self.__no_toggle_in_edit_mode = [self.menuEditMode.GetId()]

        # # \todo Show clocks info list
        # # \todo Show db .info file

        self.menuViewSettings = viewmenu.Append(wx.NewId(),
                                                "Settings...",
                                                "View and change display settings.")
        # View
        self.menuShuffleColors = accessibilitymenu.Append(wx.NewId(),
                                 "Shuffle Color Map",
                                 "Shuffle the color map so that it doesn't produce a smooth gradient.",
                                 kind = wx.ITEM_CHECK)
        accessibilitymenu.AppendSeparator()
        self.menuDefaultColors = accessibilitymenu.Append(wx.NewId(),
                                 "Default Color Map",
                                 "Use the default color map.",
                                 kind = wx.ITEM_RADIO)
        self.menuDeuteranopiaColors = accessibilitymenu.Append(wx.NewId(),
                                 "Deuteranopia",
                                 "Use a color map adjusted for deuteranopia.",
                                 kind = wx.ITEM_RADIO)
        self.menuProtanopiaColors = accessibilitymenu.Append(wx.NewId(),
                                 "Protanopia",
                                 "Use a color map adjusted for protanopia.",
                                 kind = wx.ITEM_RADIO)
        self.menuTritanopiaColors = accessibilitymenu.Append(wx.NewId(),
                                 "Tritanopia",
                                 "Use a color map adjusted for tritanopia.",
                                 kind = wx.ITEM_RADIO)

        # disable colorblindness menu options because the code that implemented it
        # had licensing issues and needs to be reimplemented
        self.menuDeuteranopiaColors.Enable(False)
        self.menuProtanopiaColors.Enable(False)
        self.menuTritanopiaColors.Enable(False)

        viewmenu.AppendSubMenu(accessibilitymenu, "Accessibility")

        self.menuHoverPreview = viewmenu.Append(wx.NewId(),
                                    "Hover Preview\tCTRL+H",
                                    "Displays annotation when mouse is over layout element.",
                                    kind = wx.ITEM_CHECK)
        menuHoverPreviewKeyShortcutId = wx.NewId()
        accelentries.append((wx.ACCEL_CTRL, ord('H'), menuHoverPreviewKeyShortcutId))

        viewmenu.Check(self.menuHoverPreview.GetId(), True)
        self.__parent.SetHoverPreview(viewmenu.IsChecked(self.menuHoverPreview.GetId()))

        self.menuHoverOptions = viewmenu.Append(wx.NewId(),
                                    "Hover Options",
                                    "Change what is shown in hover preview.")

        menuScheduleStyle = viewmenu.Append(wx.NewId(),
                                    "Schedule Element Style",
                                    "Set attributes about schedule layout element.")

        self.menuElementSettings = viewmenu.Append(wx.NewId(),
                                    "Element Settings",
                                    "Set attributes about layout element.")
        menuWatchlist = viewmenu.Append(wx.NewId(),
                                    "Transaction Watches",
                                    "Brings up a list of pinned hover view transactions.")

        self.menuElementSettings.Enable(False)

        menuLayoutVariables = viewmenu.Append(wx.NewId(),
                                              "Layout Location String Variables",
                                              "Show values of variables used in the layout.")

        menuConsole = viewmenu.Append(wx.NewId(),
                                              "Python Console",
                                              "Open a Python console.")

        menuFindElement = viewmenu.Append(wx.NewId(), "Search Layout\tCTRL+SHIFT+F", "Search for elements in this layout.")
        accelentries.append((wx.ACCEL_CTRL | wx.ACCEL_SHIFT, ord('F'), menuFindElement.GetId()))

        self.menuToggleControls = viewmenu.Append(wx.NewId(),
                                                  "Show navigation controls\tALT+C",
                                                  "Displays navigation controls at the bottom of the window",
                                                  kind = wx.ITEM_CHECK)
        menuToggleControlsKeyShortcutId = wx.NewId()
        accelentries.append((wx.ACCEL_ALT, ord('C'), menuToggleControlsKeyShortcutId))

        viewmenu.Check(self.menuToggleControls.GetId(), True)

        # Help

        menuInfo = helpmenu.Append(wx.NewId(), "&Information", "Show information about the current frame and database")
        menuShortcutsHelp = helpmenu.Append(wx.NewId(), "Shortcuts", "Show shortcut information")
        menuAbout = helpmenu.Append(ID_HELP_ABOUT, "&About", "Information about this program")

        # Creating the menubar.
        self.Append(filemenu, "&File")
        self.Append(self.__editmenu, "&Edit")
        self.Append(toolsmenu, "&Tools")
        self.Append(dbmenu, "&Database")
        self.Append(viewmenu, "&View")
        self.Append(helpmenu, "&Help")

        # Creating the toolbar
        self.__edit_toolbar = wx.ToolBar(self.__parent, style = wx.TB_FLAT | wx.TB_HORIZONTAL, name = 'Edit Toolbar')
        self.__edit_toolbar.Show(False)

        # File operations
        self.toolbarNew = self.__edit_toolbar.AddTool(wx.NewId(), 'New', wx.ArtProvider.GetBitmap(wx.ART_NEW), shortHelp = 'New layout')
        self.toolbarOpen = self.__edit_toolbar.AddTool(wx.NewId(), 'Open', wx.ArtProvider.GetBitmap(wx.ART_FILE_OPEN), shortHelp = 'Open layout...')
        self.toolbarSave = self.__edit_toolbar.AddTool(wx.NewId(), 'Save', wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE), shortHelp = 'Save current layout')
        self.toolbarSaveAs = self.__edit_toolbar.AddTool(wx.NewId(), 'Save As', wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE_AS), shortHelp = 'Save current layout as...')
        self.toolbarEditMode = self.__edit_toolbar.AddTool(wx.NewId(), 'Stop Editing', wx.ArtProvider.GetBitmap(wx.ART_CLOSE), shortHelp = 'Leave edit mode')

        self.__edit_toolbar.AddSeparator()

        # Undo/Redo
        self.toolbarUndo = self.__edit_toolbar.AddTool(wx.NewId(), 'Undo', wx.ArtProvider.GetBitmap(wx.ART_UNDO), shortHelp = 'Undo')

        redo_icon = wx.ArtProvider.GetBitmap(wx.ART_REDO, wx.ART_TOOLBAR)
        self.toolbarRedo = self.__edit_toolbar.AddTool(wx.NewId(), 'Redo', redo_icon, shortHelp = 'Redo')

        self.__edit_toolbar.AddSeparator()

        # Add/Duplicate/Delete Elements
        self.toolbarAddElement = self.__edit_toolbar.AddTool(wx.NewId(), 'Add Element', wx.ArtProvider.GetBitmap(wx.ART_PLUS), shortHelp = 'Add an element')
        self.toolbarCloneElement = self.__edit_toolbar.AddTool(wx.NewId(), 'Duplicate Element', wx.ArtProvider.GetBitmap(wx.ART_COPY), shortHelp = 'Duplicate an element')
        self.toolbarDeleteElement = self.__edit_toolbar.AddTool(wx.NewId(), 'Delete Element(s)', wx.ArtProvider.GetBitmap(wx.ART_MINUS), shortHelp = 'Delete selected element(s)')

        self.__edit_toolbar.AddSeparator()

        # Translate
        translate_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/Actions-transform-move-icon.png", wx.BITMAP_TYPE_PNG)
        self.toolbarTranslate = self.__edit_toolbar.AddTool(wx.NewId(), 'Translate', translate_icon, shortHelp = 'Translate element(s)')

        self.__edit_toolbar.AddSeparator()

        # Cursor location
        self.toolbarCursorLocation = wx.TextCtrl(self.__edit_toolbar, wx.NewId(), style = wx.TE_READONLY | wx.TE_LEFT)
        self.toolbarCursorLocation.SetSize((110, -1))
        self.__edit_toolbar.AddControl(self.toolbarCursorLocation)

        self.__edit_toolbar.AddSeparator()

        # Move Front/Forward/Backward/Back
        move_to_front_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-move-front.png", wx.BITMAP_TYPE_PNG)
        self.toolbarMoveToTop = self.__edit_toolbar.AddTool(wx.NewId(), 'Move To Front', move_to_front_icon, shortHelp = 'Move element(s) to the front')

        move_forward_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-move-forward.png", wx.BITMAP_TYPE_PNG)
        self.toolbarMoveUp = self.__edit_toolbar.AddTool(wx.NewId(), 'Move Forward', move_forward_icon, shortHelp = 'Move element(s) forward')

        move_backward_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-move-backward.png", wx.BITMAP_TYPE_PNG)
        self.toolbarMoveDown = self.__edit_toolbar.AddTool(wx.NewId(), 'Move Backward', move_backward_icon, shortHelp = 'Move element(s) backward')

        move_to_back_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-move-back.png", wx.BITMAP_TYPE_PNG)
        self.toolbarMoveToBottom = self.__edit_toolbar.AddTool(wx.NewId(), 'Move To Back', move_to_back_icon, shortHelp = 'Move elements(s) to the back')

        self.__edit_toolbar.AddSeparator()

        # Alignment
        align_left_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-align-left.png", wx.BITMAP_TYPE_PNG)
        self.toolbarAlignLeft = self.__edit_toolbar.AddTool(wx.NewId(), 'Align Left', align_left_icon, shortHelp = 'Align element(s) to their left edge')

        align_right_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-align-right.png", wx.BITMAP_TYPE_PNG)
        self.toolbarAlignRight = self.__edit_toolbar.AddTool(wx.NewId(), 'Align Right', align_right_icon, shortHelp = 'Align element(s) to their right edge')

        align_top_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-align-top.png", wx.BITMAP_TYPE_PNG)
        self.toolbarAlignTop = self.__edit_toolbar.AddTool(wx.NewId(), 'Align Top', align_top_icon, shortHelp = 'Align element(s) to their top edge')

        align_bottom_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-align-bottom.png", wx.BITMAP_TYPE_PNG)
        self.toolbarAlignBottom = self.__edit_toolbar.AddTool(wx.NewId(), 'Align Bottom', align_bottom_icon, shortHelp = 'Align element(s) to their bottom edge')

        self.__edit_toolbar.AddSeparator()

        # Flipping
        flip_horizontal_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-flip-horizontal.png", wx.BITMAP_TYPE_PNG)
        self.toolbarFlipHoriz = self.__edit_toolbar.AddTool(wx.NewId(), 'Horizontal Flip', flip_horizontal_icon, shortHelp = 'Flip element(s) horizontally')

        flip_vertical_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/shape-flip-vertical.png", wx.BITMAP_TYPE_PNG)
        self.toolbarFlipVert = self.__edit_toolbar.AddTool(wx.NewId(), 'Vertical Flip', flip_vertical_icon, shortHelp = 'Flip element(s) vertically')

        self.__edit_toolbar.AddSeparator()

        # Stacking
        self.toolbarColumnUp = self.__edit_toolbar.AddTool(wx.NewId(), 'Make Column Up', wx.ArtProvider.GetBitmap(wx.ART_GO_UP), shortHelp = 'Make column from bottom')
        self.toolbarColumnDown = self.__edit_toolbar.AddTool(wx.NewId(), 'Make Column Down', wx.ArtProvider.GetBitmap(wx.ART_GO_DOWN), shortHelp = 'Make column from top')
        self.toolbarRowLeft = self.__edit_toolbar.AddTool(wx.NewId(), 'Make Row Left', wx.ArtProvider.GetBitmap(wx.ART_GO_BACK), shortHelp = 'Make row from rightmost')
        self.toolbarRowRight = self.__edit_toolbar.AddTool(wx.NewId(), 'Make Row Right', wx.ArtProvider.GetBitmap(wx.ART_GO_FORWARD), shortHelp = 'Make row from leftmost')

        self.__edit_toolbar.AddSeparator()

        # Padding
        add_space_up_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/add-space-up.png", wx.BITMAP_TYPE_PNG)
        self.toolbarIndUp = self.__edit_toolbar.AddTool(wx.NewId(), 'Add Spacing Up', add_space_up_icon, shortHelp = 'Add spacing up')

        add_space_left_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/add-space-left.png", wx.BITMAP_TYPE_PNG)
        self.toolbarIndLeft = self.__edit_toolbar.AddTool(wx.NewId(), 'Add Spacing Left', add_space_left_icon, shortHelp = 'Add spacing left')

        add_space_right_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/add-space-right.png", wx.BITMAP_TYPE_PNG)
        self.toolbarIndRight = self.__edit_toolbar.AddTool(wx.NewId(), 'Add Spacing Right', add_space_right_icon, shortHelp = 'Add spacing right')

        add_space_down_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/add-space-down.png", wx.BITMAP_TYPE_PNG)
        self.toolbarIndDown = self.__edit_toolbar.AddTool(wx.NewId(), 'Add Spacing Down', add_space_down_icon, shortHelp = 'Add spacing down')

        sub_space_up_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/sub-space-up.png", wx.BITMAP_TYPE_PNG)
        self.toolbarSubtIndUp = self.__edit_toolbar.AddTool(wx.NewId(), 'Remove Spacing Up', sub_space_up_icon, shortHelp = 'Remove spacing up')

        sub_space_left_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/sub-space-left.png", wx.BITMAP_TYPE_PNG)
        self.toolbarSubtIndLeft = self.__edit_toolbar.AddTool(wx.NewId(), 'Remove Spacing Left', sub_space_left_icon, shortHelp = 'Remove spacing left')

        sub_space_right_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/sub-space-right.png", wx.BITMAP_TYPE_PNG)
        self.toolbarSubtIndRight = self.__edit_toolbar.AddTool(wx.NewId(), 'Remove Spacing Right', sub_space_right_icon, shortHelp = 'Remove spacing right')

        sub_space_down_icon = wx.Bitmap(os.path.dirname(os.path.dirname(this_script_filename)) + "/resources/sub-space-down.png", wx.BITMAP_TYPE_PNG)
        self.toolbarSubtIndDown = self.__edit_toolbar.AddTool(wx.NewId(), 'Remove Spacing Down', sub_space_down_icon, shortHelp = 'Remove spacing down')

        self.__edit_toolbar.Realize()

        # Disable some menus by default (edit mode)
        edit_mode = False
        # #refresh enabling/disabling of items
        self.SetEditModeSettings(edit_mode)

        self.hover_options_dialog = None

        # Event Bindings
        self.__parent.Bind(wx.EVT_MENU, self.OnNew, menuNew)
        self.__parent.Bind(wx.EVT_TOOL, self.OnNew, self.toolbarNew)
        self.__parent.Bind(wx.EVT_MENU, self.OnOpen, menuOpen)
        self.__parent.Bind(wx.EVT_TOOL, self.OnOpen, self.toolbarOpen)
        self.__parent.Bind(wx.EVT_MENU, self.OnImportPlain, menuImportPlain)
        self.__parent.Bind(wx.EVT_MENU, self.OnSave, menuSave)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSave, self.toolbarSave)
        self.__parent.Bind(wx.EVT_MENU, self.OnSaveAs, menuSaveAs)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSaveAs, self.toolbarSaveAs)
        self.__parent.Bind(wx.EVT_MENU, self.OnClose, menuClose)
        self.__parent.Bind(wx.EVT_MENU, self.OnExit, menuQuit)
        self.__parent.Bind(wx.EVT_MENU, self.OnNewElement, menuNewEl)
        self.__parent.Bind(wx.EVT_TOOL, self.OnNewElement, self.toolbarAddElement)
        self.__parent.Bind(wx.EVT_MENU, self.OnCloneElement, menuCloneEl)
        self.__parent.Bind(wx.EVT_TOOL, self.OnCloneElement, self.toolbarCloneElement)
        self.__parent.Bind(wx.EVT_MENU, self.OnDeleteElement, menuDeleteEl)
        self.__parent.Bind(wx.EVT_TOOL, self.OnDeleteElement, self.toolbarDeleteElement)
        self.__parent.Bind(wx.EVT_MENU, self.OnSelectAll, menuSelectAll)
        self.__parent.Bind(wx.EVT_MENU, self.OnInvertSelection, menuInvertSelection)
        self.__parent.Bind(wx.EVT_MENU, self.OnFrameInfo, menuInfo)
        self.__parent.Bind(wx.EVT_MENU, self.OnShortcutsHelp, menuShortcutsHelp)
        self.__parent.Bind(wx.EVT_MENU, self.OnAbout, menuAbout)
        self.__parent.Bind(wx.EVT_MENU, self.OnRowLeft, menuRowLeft)
        self.__parent.Bind(wx.EVT_TOOL, self.OnRowLeft, self.toolbarRowLeft)
        self.__parent.Bind(wx.EVT_MENU, self.OnRowRight, menuRowRight)
        self.__parent.Bind(wx.EVT_TOOL, self.OnRowRight, self.toolbarRowRight)
        self.__parent.Bind(wx.EVT_MENU, self.OnColumnUp, menuColumnUp)
        self.__parent.Bind(wx.EVT_TOOL, self.OnColumnUp, self.toolbarColumnUp)
        self.__parent.Bind(wx.EVT_MENU, self.OnColumnDown, menuColumnDown)
        self.__parent.Bind(wx.EVT_TOOL, self.OnColumnDown, self.toolbarColumnDown)
        self.__parent.Bind(wx.EVT_MENU, self.OnAlignTop, menuAlignTop)
        self.__parent.Bind(wx.EVT_TOOL, self.OnAlignTop, self.toolbarAlignTop)
        self.__parent.Bind(wx.EVT_MENU, self.OnAlignBottom, menuAlignBottom)
        self.__parent.Bind(wx.EVT_TOOL, self.OnAlignBottom, self.toolbarAlignBottom)
        self.__parent.Bind(wx.EVT_MENU, self.OnAlignLeft, menuAlignLeft)
        self.__parent.Bind(wx.EVT_TOOL, self.OnAlignLeft, self.toolbarAlignLeft)
        self.__parent.Bind(wx.EVT_MENU, self.OnAlignRight, menuAlignRight)
        self.__parent.Bind(wx.EVT_TOOL, self.OnAlignRight, self.toolbarAlignRight)
        self.__parent.Bind(wx.EVT_MENU, self.OnLocationsList, menuShowLocations)
        self.__parent.Bind(wx.EVT_MENU, self.OnSearch, menuSearch)
        self.__parent.Bind(wx.EVT_MENU, self.OnToggleUpdate, self.menuPollDB)
        self.__parent.Bind(wx.EVT_MENU, self.OnToggleUpdateKey, id = menuPollDBKeyShortcutId)
        self.__parent.Bind(wx.EVT_MENU, self.OnRefreshDB, menuRefreshDB)
        self.__parent.Bind(wx.EVT_MENU, self.OnMDominant, self.menuMDominant)
        self.__parent.Bind(wx.EVT_MENU, self.OnMEach, self.menuMEach)
        self.__parent.Bind(wx.EVT_MENU, self.OnMoveSnap, self.menuMoveSnap)
        self.__parent.Bind(wx.EVT_MENU, self.OnResizeSnap, self.menuResizeSnap)
        self.__parent.Bind(wx.EVT_MENU, self.OnREach, self.menuREach)
        self.__parent.Bind(wx.EVT_MENU, self.OnRDominant, self.menuRDominant)
        self.__parent.Bind(wx.EVT_MENU, self.OnTranslate, menuTranslate)
        self.__parent.Bind(wx.EVT_TOOL, self.OnTranslate, self.toolbarTranslate)
        self.__parent.Bind(wx.EVT_MENU, self.OnAvg, menuAvg)
        self.__parent.Bind(wx.EVT_MENU, self.OnIndUp, menuIndUp)
        self.__parent.Bind(wx.EVT_TOOL, self.OnIndUp, self.toolbarIndUp)
        self.__parent.Bind(wx.EVT_MENU, self.OnIndDown, menuIndDown)
        self.__parent.Bind(wx.EVT_TOOL, self.OnIndDown, self.toolbarIndDown)
        self.__parent.Bind(wx.EVT_MENU, self.OnIndRight, menuIndRight)
        self.__parent.Bind(wx.EVT_TOOL, self.OnIndRight, self.toolbarIndRight)
        self.__parent.Bind(wx.EVT_MENU, self.OnIndLeft, menuIndLeft)
        self.__parent.Bind(wx.EVT_TOOL, self.OnIndLeft, self.toolbarIndLeft)
        self.__parent.Bind(wx.EVT_MENU, self.OnSubtIndUp, menuSubtIndUp)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSubtIndUp, self.toolbarSubtIndUp)
        self.__parent.Bind(wx.EVT_MENU, self.OnSubtIndDown, menuSubtIndDown)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSubtIndDown, self.toolbarSubtIndDown)
        self.__parent.Bind(wx.EVT_MENU, self.OnSubtIndRight, menuSubtIndRight)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSubtIndRight, self.toolbarSubtIndRight)
        self.__parent.Bind(wx.EVT_MENU, self.OnSubtIndLeft, menuSubtIndLeft)
        self.__parent.Bind(wx.EVT_TOOL, self.OnSubtIndLeft, self.toolbarSubtIndLeft)
        self.__parent.Bind(wx.EVT_MENU, self.OnFlipHoriz, menuFlipHoriz)
        self.__parent.Bind(wx.EVT_TOOL, self.OnFlipHoriz, self.toolbarFlipHoriz)
        self.__parent.Bind(wx.EVT_MENU, self.OnFlipVert, menuFlipVert)
        self.__parent.Bind(wx.EVT_TOOL, self.OnFlipVert, self.toolbarFlipVert)
        self.__parent.Bind(wx.EVT_MENU, self.OnMoveToTop, menuMoveToTop)
        self.__parent.Bind(wx.EVT_TOOL, self.OnMoveToTop, self.toolbarMoveToTop)
        self.__parent.Bind(wx.EVT_MENU, self.OnMoveToBottom, menuMoveToBottom)
        self.__parent.Bind(wx.EVT_TOOL, self.OnMoveToBottom, self.toolbarMoveToBottom)
        self.__parent.Bind(wx.EVT_MENU, self.OnMoveUp, menuMoveUp)
        self.__parent.Bind(wx.EVT_TOOL, self.OnMoveUp, self.toolbarMoveUp)
        self.__parent.Bind(wx.EVT_MENU, self.OnMoveDown, menuMoveDown)
        self.__parent.Bind(wx.EVT_TOOL, self.OnMoveDown, self.toolbarMoveDown)
        self.__parent.Bind(wx.EVT_MENU, self.OnEditMode, self.menuEditMode)
        self.__parent.Bind(wx.EVT_TOOL, self.OnEditModeKey, self.toolbarEditMode)
        self.__parent.Bind(wx.EVT_MENU, self.OnEditModeKey, id = menuEditModeKeyShortcutId)
        self.__parent.Bind(wx.EVT_MENU, self.OnUndo, self.menuUndo)
        self.__parent.Bind(wx.EVT_TOOL, self.OnUndo, self.toolbarUndo)
        self.__parent.Bind(wx.EVT_MENU, self.OnRedo, self.menuRedo)
        self.__parent.Bind(wx.EVT_TOOL, self.OnRedo, self.toolbarRedo)
        self.__parent.Bind(wx.EVT_MENU, self.OnRedoAll, self.menuRedoAll)
        self.__parent.Bind(wx.EVT_MENU, self.OnHoverPreview, self.menuHoverPreview)
        self.__parent.Bind(wx.EVT_MENU, self.OnHoverPreviewKey, id = menuHoverPreviewKeyShortcutId)
        self.__parent.Bind(wx.EVT_MENU, self.OnHoverOptions, self.menuHoverOptions)
        self.__parent.Bind(wx.EVT_MENU, self.OnElementSettings, self.menuElementSettings)
        self.__parent.Bind(wx.EVT_MENU, self.OnChooseScheduleStyle, menuScheduleStyle)
        self.__parent.Bind(wx.EVT_MENU, self.OnShowLayoutVariables, menuLayoutVariables)
        self.__parent.Bind(wx.EVT_MENU, self.OnWatchList, menuWatchlist)
        self.__parent.Bind(wx.EVT_MENU, self.OnConsole, menuConsole)
        self.__parent.Bind(wx.EVT_MENU, self.OnFindElement, menuFindElement)
        self.__parent.Bind(wx.EVT_MENU, self.OnToggleControls, self.menuToggleControls)
        self.__parent.Bind(wx.EVT_MENU, self.OnToggleControlsKey, id = menuToggleControlsKeyShortcutId)
        self.__parent.Bind(wx.EVT_MENU, self.OnShuffleColors, self.menuShuffleColors)
        self.__parent.Bind(wx.EVT_MENU, self.OnColorMapChange, self.menuDefaultColors)
        self.__parent.Bind(wx.EVT_MENU, self.OnColorMapChange, self.menuDeuteranopiaColors)
        self.__parent.Bind(wx.EVT_MENU, self.OnColorMapChange, self.menuProtanopiaColors)
        self.__parent.Bind(wx.EVT_MENU, self.OnColorMapChange, self.menuTritanopiaColors)
        self.__parent.Bind(wx.EVT_MENU, self.OnViewSettings, self.menuViewSettings)

        self.__accel_tbl = wx.AcceleratorTable(accelentries)

        colorblindness_option = self.__parent.GetWorkspace().GetPalette()
        palette_shuffle_option = self.__parent.GetWorkspace().GetColorShuffleState()

        if colorblindness_option == 'default':
            self.menuDefaultColors.Check()
        elif colorblindness_option == 'd':
            self.menuDeuteranopiaColors.Check()
        elif colorblindness_option == 'p':
            self.menuProtanopiaColors.Check()
        elif colorblindness_option == 't':
            self.menuTritanopiaColors.Check()
        else:
            raise ValueError('Invalid value specified for ARGOS_COLORBLINDNESS_MODE: {}'.format(colorblindness_option))

        if palette_shuffle_option == 'default':
            self.menuShuffleColors.Check(False)
        elif palette_shuffle_option == 'shuffled':
            self.menuShuffleColors.Check()
        else:
            raise ValueError('Invalid value specified for ARGOS_PALETTE_SHUFFLE_MODE: {}'.format(palette_shuffle_option))

    def OnFlipHoriz(self, evt):
        self.__selection.Flip(self.__selection.RIGHT)

    def OnFlipVert(self, evt):
        self.__selection.Flip(self.__selection.TOP)

    def OnMoveToTop(self, evt):
        self.__selection.MoveToTop()

    def OnMoveToBottom(self, evt):
        self.__selection.MoveToBottom()

    def OnMoveUp(self, evt):
        self.__selection.MoveUp()

    def OnMoveDown(self, evt):
        self.__selection.MoveDown()

    def OnAvg(self, evt):
        self.__selection.Average()

    def OnIndUp(self, evt):
        self.__selection.Indent(self.__selection.BOTTOM)

    def OnIndDown(self, evt):
        self.__selection.Indent(self.__selection.TOP)

    def OnIndLeft(self, evt):
        self.__selection.Indent(self.__selection.RIGHT)

    def OnIndRight(self, evt):
        self.__selection.Indent(self.__selection.LEFT)

    def OnSubtIndUp(self, evt):
        self.__selection.Indent(self.__selection.TOP, True)

    def OnSubtIndDown(self, evt):
        self.__selection.Indent(self.__selection.BOTTOM, True)

    def OnSubtIndLeft(self, evt):
        self.__selection.Indent(self.__selection.LEFT, True)

    def OnSubtIndRight(self, evt):
        self.__selection.Indent(self.__selection.RIGHT, True)

    def OnRowLeft(self, evt):
        self.__selection.Stack('left')

    def OnColumnUp(self, evt):
        self.__selection.Stack('top')

    def OnRowRight(self, evt):
        self.__selection.Stack('right')

    def OnColumnDown(self, evt):
        self.__selection.Stack('bottom')

    def OnAlignTop(self, evt):
        self.__selection.Align('top')

    def OnAlignBottom(self, evt):
        self.__selection.Align('bottom')

    def OnAlignLeft(self, evt):
        self.__selection.Align('left')

    def OnAlignRight(self, evt):
        self.__selection.Align('right')

    def OnMoveSnap(self, evt):
        if not self.menuMoveSnap.IsChecked():
            self.__selection.SetSnapMode('freemove')
        else:
            print(self.menuMDominant.IsChecked())
            if self.menuMDominant.IsChecked():
                self.__selection.SetSnapMode('mdominant')
            else:
                self.__selection.SetSnapMode('meach')

    def OnMDominant(self, evt):
        if self.menuMoveSnap.IsChecked():
            self.__selection.SetSnapMode('mdominant')

    def OnMEach(self, evt):
        if self.menuMoveSnap.IsChecked():
            self.__selection.SetSnapMode('meach')

    def OnResizeSnap(self, evt):
        if not self.menuResizeSnap.IsChecked():
            self.__selection.SetSnapMode('freesize')
        else:
            if self.menuRDominant.IsChecked():
                self.__selection.SetSnapMode('rdominant')
            else:
                self.__selection.SetSnapMode('reach')

    def OnTranslate(self, evt):
        if self.__selection.GetSelection():
            # Translation is done within the dialog
            dlg = TranslateElementsDlg(self.__parent)
            try:
                dlg.ShowModal()
            finally:
                dlg.Destroy()

    def OnRDominant(self, evt):
        if self.menuResizeSnap.IsChecked():
            self.__selection.SetSnapMode('rdominant')

    def OnREach(self, evt):
        if self.menuResizeSnap.IsChecked():
            self.__selection.SetSnapMode('reach')

    # # sets settings based on status of edit mode
    def SetEditModeSettings(self, menuEditBool):
        edit_mode = self.menuEditMode.Check(menuEditBool)
        items_to_change = self.__editmenu.GetMenuItems()
        for item in items_to_change:
            if not item.GetId() in self.__no_toggle_in_edit_mode:
                item.Enable(menuEditBool)

        self.__UpdateUndoRedoItems()

        dialog = self.__parent.GetCanvas().GetDialog()
        self.menuElementSettings.Enable(menuEditBool)
        if menuEditBool:
            self.SetBackgroundColour('YELLOW');
            self.__parent.SetAcceleratorTable(self.__accel_tbl)
            dialog.Show()
            dialog.SetFocus()
            dialog.Raise()
        else:
            self.SetBackgroundColour('GRAY');
            self.__parent.SetAcceleratorTable(wx.NullAcceleratorTable)
            dialog.Show(False)

    def OnRefreshDB(self, evt):
        self.__parent.ForceDBUpdate()

    def OnToggleUpdate(self, evt):
        poll_mode = self.menuPollDB.IsChecked()
        self.__parent.SetPollMode(poll_mode)

    def OnToggleUpdateKey(self, evt):
        self.menuPollDB.Toggle()
        self.OnToggleUpdate(evt)

    def OnEditMode(self, evt):
        edit_mode = self.menuEditMode.IsChecked()
        self.__parent.SetEditMode(edit_mode)

    def OnEditModeKey(self, evt):
        self.menuEditMode.Check(not self.menuEditMode.IsChecked())
        self.OnEditMode(evt)

    def OnUndo(self, evt = None):
        self.__selection.Undo()

    def OnRedo(self, evt = None):
        self.__selection.Redo()

    def OnRedoAll(self, evt = None):
        self.__selection.RedoAll()

    def OnHoverPreview(self, evt):
        hover_preview = self.menuHoverPreview.IsChecked()
        self.__parent.SetHoverPreview(hover_preview)
        self.__parent.Refresh()

    def OnHoverPreviewKey(self, evt):
        self.menuHoverPreview.Toggle()
        self.OnHoverPreview(evt)

    def OnHoverOptions(self, evt):
        self.hover_options_dialog = HoverPreviewOptionsDialog(self, self.__parent.GetCanvas().GetHoverPreview())
        self.hover_options_dialog.ShowWindowModal()

    def OnViewSettings(self, evt):
        settings = self.__parent.GetSettings()
        with ViewSettingsDialog(self.__parent, settings) as view_settings_dialog:
            if view_settings_dialog.ShowModal() == wx.ID_OK:
                new_settings = view_settings_dialog.GetSettings()
                if new_settings:
                    self.__parent.UpdateSettings(new_settings)
                    settings.save()

    def OnElementSettings(self, evt):
        dialog = self.__parent.GetCanvas().GetDialog()
        dialog.Show()
        dialog.SetFocus()
        dialog.Raise()

    def OnConsole(self, evt):
        self.__parent.ShowDialog('console', ConsoleDlg)

    def OnWatchList(self, evt):
        self.__parent.ShowDialog('watchlist', WatchListDlg)

    def OnChooseScheduleStyle(self, evt):
        dl = ScheduleLineElement.DRAW_LOOKUP
        dialog = wx.SingleChoiceDialog(self, 'Global Style for Schedule Lines.', '',
                            list(dl.keys()),
                            wx.CHOICEDLG_STYLE)
        if dialog.ShowModal() == wx.ID_OK:
            self.__parent.GetCanvas().SetScheduleLineStyle(dl[dialog.GetStringSelection()])
        dialog.Destroy()

    # # Show dialog containing menu layout variables
    def OnShowLayoutVariables(self, evt):
        self.__parent.GetContext().UpdateLocationVariables()
        dialog = LayoutVariablesDialog(self,
                                       wx.NewId(),
                                       'Layout location Variables',
                                       self.__parent.GetContext())
        dialog.Show()

    # # Handle clicking of Open Locations List
    def OnLocationsList(self, evt):
        self.__parent.ShowLocationsList()

    # # Handle clicking of Search menu
    def OnSearch(self, evt):
        self.__parent.ShowSearch()

    def OnFindElement(self, evt):
        self.__parent.ShowFindElement()

    def OnToggleControls(self, evt):
        self.__parent.ShowNavigationControls(self.menuToggleControls.IsChecked())

    def OnToggleControlsKey(self, evt):
        self.menuToggleControls.Toggle()
        self.OnToggleControls(evt)

    def OnColorMapChange(self, evt):
        if self.menuDefaultColors.IsChecked():
            palette = 'default'
        elif self.menuDeuteranopiaColors.IsChecked():
            palette = 'd'
        elif self.menuProtanopiaColors.IsChecked():
            palette = 'p'
        elif self.menuTritanopiaColors.IsChecked():
            palette = 't'
        else:
            raise RuntimeError('None of the color menu options are checked!')

        self.__parent.GetWorkspace().SetPalette(palette)

    def OnShuffleColors(self, evt):
        if self.menuShuffleColors.IsChecked():
            shuffle_state = 'shuffled'
        else:
            shuffle_state = 'default'
        self.__parent.GetWorkspace().SetColorShuffleState(shuffle_state)

    # # Placeholder method
    def OnNew(self, evt):
        context = self.__parent.GetContext()
        loc_vars = {}
        self.__parent.GetWorkspace().OpenLayoutFrame(None, context.dbhandle.database, context.GetHC(), self.menuPollDB.IsChecked(), self.__parent.GetTitlePrefix(), self.__parent.GetTitleOverride(), self.__parent.GetTitleSuffix(), loc_vars)

    def OnOpen(self, evt):
        context = self.__parent.GetContext()
        loc_vars = {}

        layout = self.__parent.GetContext().GetLayout()
        if layout is not None:
            default = layout.GetFilename()
        else:
            default = None
        dlg = SelectLayoutDlg(default)
        dlg.Centre()
        if dlg.ShowModal() == wx.CANCEL:
            return
        dlg.Destroy()

        lf = dlg.GetFilename()
        self.__parent.GetWorkspace().OpenLayoutFrame(lf, context.dbhandle.database, context.GetHC(), self.menuPollDB.IsChecked(), self.__parent.GetTitlePrefix(), self.__parent.GetTitleOverride(), self.__parent.GetTitleSuffix(), loc_vars)

    # # Import graph into Argos of type 'plain' produced by neato
    def OnImportPlain(self, evt):
        if self.__last_loaded_graph_dir is None:
            fp = self.__parent.GetContext().dbhandle.database.filename
            if fp is not None:
                self.__last_loaded_graph_dir = os.path.dirname(os.path.abspath(fp)) + '/' # directory
            else:
                self.__last_loaded_graph_dir = os.getcwd() + '/'
        # Loop until user saves or cancels
        dlg = wx.FileDialog(self,
                            "Import Neato Plain File",
                            defaultFile = self.__last_loaded_graph_dir,
                            wildcard = 'Neato Plain Graph Files (*.plain)|*.plain',
                            style = wx.FD_OPEN | wx.FD_CHANGE_DIR)
        dlg.ShowModal()
        ret = dlg.GetReturnCode()
        fp = dlg.GetPath()
        self.__last_loaded_graph_dir = os.path.dirname(os.path.abspath(fp)) + '/'
        dlg.Destroy()

        if ret == wx.ID_CANCEL:
            return # No import

        shortpath = fp if len(fp) < 13 else '...' + fp[-10:]
        self.__selection.BeginCheckpoint('import plain "{}"'.format(shortpath), force_elements = [])
        try:
            new_els = self.__layout.ImportPlain(fp)
            self.__selection.ClearSelection()
            self.__selection.Add(new_els)
        finally:
            self.__selection.CommitCheckpoint() # Use current selection since it was just set

        self.__parent.Refresh()

    # # 'Saving' means saving the Layout to file, nothing else is currently
    #  preserved about a session (no user preferences, current selection, HC)
    def OnSave(self, evt):
        self.__parent.Save()

    # # Handle saving the file to a selected path
    def OnSaveAs(self, evt = None):
        self.__parent.SaveAs()

    # # Show information about this frame
    def OnFrameInfo(self, evt):
        layout_file = self.__layout.GetFilename()
        if layout_file is None:
            layout_file = '<New, Unsaved Layout file>'
        else:
            layout_file = '"{0}"'.format(layout_file)

        # There is a diferent between client size and screen size (GetSize).
        # Both are too small and do not include border.
        # Enlarge the apparent size so that the reported size includes the
        # border and can correctly be used as a command line geometry input.
        # There is complimentary logic in argos.py
        w, h = self.__parent.GetSize()
        wdiff = w - self.__parent.GetClientSize()[0]
        hdiff = h - self.__parent.GetClientSize()[1]
        w += wdiff
        h += hdiff
        x, y = self.__parent.GetPosition()

        message = 'Version:{}\n\nLayout:\n{}\n\nDatabase:\n"{}" (v{})\n\nLayout Elements: {}\n\nFrame Geometry (w,h,x,y): {},{},{},{}\n\nReader Library:\n{}' \
                  .format(get_version(),
                          layout_file,
                          self.__parent.GetContext().dbhandle.database.filename,
                          self.__parent.GetContext().dbhandle.api.getFileVersion(),
                          len(self.__layout.GetElements()),
                          w, h, x, y,
                          os.path.dirname(self.__parent.GetContext().dbhandle.database.dbmodule.__file__))
        dlg = wx.MessageDialog(self, message, "Argos Frame-Specific Information", wx.OK)
        dlg.ShowModal()
        dlg.Destroy()

    def OnShortcutsHelp(self, evt):
        if not self.__shortcut_help_dlg:
            self.__shortcut_help_dlg = ShortcutHelp(self.__parent, ID_SHORTCUT_HELP)

    def OnNewElement(self, evt):
        if self.__parent.GetCanvas().GetInputDecoder().GetEditMode():
            type_dialog = ElementTypeSelectionDialog(self.__parent.GetCanvas())
            type_dialog.Center()
            if type_dialog.ShowModal() == wx.ID_OK:
                self.__selection.GenerateElement(self.__layout,
                                                                            type_dialog.GetSelection())

    def OnCloneElement(self, evt):
        if self.__parent.GetCanvas().GetInputDecoder().GetEditMode():
            self.__selection.PrepNextCopy()
            self.__selection.GenerateDuplicateSelection(self.__layout, delta = 5)

    def OnDeleteElement(self, evt):
        if self.__parent.GetCanvas().GetInputDecoder().GetEditMode():
            self.__selection.Delete(self.__layout)

    def OnSelectAll(self, evt):
        if self.__parent.GetCanvas().GetInputDecoder().GetEditMode():
            self.__selection.SelectEntireLayout()

    def OnRelease(self, evt):
        # Allow clearing selection in edit mode
        self.__selection.Clear()

    def OnInvertSelection(self, evt):
        if self.__parent.GetCanvas().GetInputDecoder().GetEditMode():
            self.__selection.InvertSelection(self.__layout)

    # # Show about dialog
    def OnAbout(self, evt):
        # # @todo Forward to workspace!
        info = wx.adv.AboutDialogInfo()
        info.SetName('Argos')
        info.SetDescription('PipeViewer. This tools visualizes an Argos pipeline database and allows the editing of custom layout files')
        info.SetCopyright("""Copyright 2019""")
        wx.adv.AboutBox(info) # Show modal about box

    # # Handle exit menu event
    def OnClose(self, evt):
        self.__parent._HandleClose()

    # # Handle exit menu event
    def OnExit(self, evt):
        logging.info('OnExit')
        self.__parent.GetWorkspace().Exit()

    def GetEditToolbar(self):
        return self.__edit_toolbar

    def ShowEditToolbar(self, show):
        self.__edit_toolbar.Show(show)

    def UpdateMouseLocation(self, x, y):
        self.toolbarCursorLocation.SetValue('(%d, %d)' % (x, y))

    # # Undo/Redo hook from selection manager
    def __OnUndoRedo(self):
        self.__UpdateUndoRedoItems()

    # # Update the undo and redo items based on the number of undos/redos tracked
    #  in the selection manager
    def __UpdateUndoRedoItems(self):
        editmode = self.__parent.GetCanvas().GetInputDecoder().GetEditMode()
        undos = self.__selection.NumUndos()
        redos = self.__selection.NumRedos()
        self.menuUndo.SetItemLabel(UNDO_FMT.format(self.__selection.GetNextUndoDesc(), undos))
        undo_enabled = editmode and (undos != 0)
        self.menuUndo.Enable(undo_enabled)
        self.__edit_toolbar.EnableTool(self.toolbarUndo.GetId(), undo_enabled)
        self.menuRedo.SetItemLabel(REDO_FMT.format(self.__selection.GetNextRedoDesc(), redos))
        redo_enabled = editmode and (redos != 0)
        self.menuRedo.Enable(redo_enabled)
        self.__edit_toolbar.EnableTool(self.toolbarRedo.GetId(), redo_enabled)
        self.menuRedoAll.SetItemLabel(REDO_ALL_FMT.format(redos))
        self.menuRedoAll.Enable(editmode and (redos != 0))
