from __future__ import annotations
from typing import Callable, List, Optional, Tuple, Union, overload

def NewId() -> int: ...

HORIZONTAL: int
VERTICAL: int
ALIGN_CENTER_HORIZONTAL: int
ALIGN_CENTER_VERTICAL: int
ALIGN_LEFT: int
ALIGN_BOTTOM: int
ALL: int
EXPAND: int
ID_ANY: int
ID_CANCEL: int
ID_OK: int
ID_FIND: int
ID_EXIT: int
DEFAULT_DIALOG_STYLE: int
MAXIMIZE_BOX: int
RESIZE_BORDER: int
CAPTION: int
CLOSE_BOX: int
SYSTEM_MENU: int
TAB_TRAVERSAL: int

OK: int
CANCEL: int

EVT_BUTTON: int
EVT_SPINCTRL: int
EVT_CLOSE: int
EVT_MENU: int
EVT_TEXT: int

KeyCode = int
WXK_UP: KeyCode
WXK_DOWN: KeyCode
WXK_RIGHT: KeyCode
WXK_LEFT: KeyCode
WXK_PAGEUP: KeyCode
WXK_PAGEDOWN: KeyCode
WXK_HOME: KeyCode
WXK_END: KeyCode
WXK_SHIFT: KeyCode
WXK_CONTROL: KeyCode

class Size:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, width: int, height: int) -> None: ...
    def __getitem__(self, idx: int) -> int: ...

class Event:
    def GetId(self) -> int: ...

class CommandEvent(Event):
    def GetString(self) -> str: ...

class NotifyEvent(CommandEvent): ...

class MenuEvent(Event): ...

class KeyboardState:
    def ShiftDown(self) -> bool: ...
    def ControlDown(self) -> bool: ...

class KeyEvent(Event, KeyboardState): ...

class EvtHandler:
    def Bind(self,
             event: int,
             handler: Callable,
             source: Optional[EvtHandler] = None,
             id: int = ID_ANY,
             id2: int = ID_ANY) -> None: ...

ALPHA_OPAQUE: int

class Colour:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, red: int, green: int, blue: int, alpha: int = ALPHA_OPAQUE) -> None: ...
    @overload
    def __init__(self, colRGB: int) -> None: ...
    @overload
    def __init__(self, colour: Colour) -> None: ...

class Window(EvtHandler):
    def SetMinSize(self, size: Size) -> None: ...
    def GetId(self) -> int: ...
    def GetTextExtent(self, string: str) -> Size: ...
    def SetSizer(self, sizer: Sizer, deleteOld: bool = True) -> None: ...
    def Hide(self) -> None: ...
    def Destroy(self) -> None: ...
    def GetSize(self) -> Size: ...
    def GetBackgroundColour(self) -> Colour: ...
    def SetBackgroundColour(self, colour: Colour) -> None: ...
    def SetAutoLayout(self, autoLayout: bool) -> None: ...
    def Enable(self, enable: bool = True) -> None: ...

class SizerFlags: ...

class PyUserData: ...

class SizerItem: ...

class Point: ...

class Rect: ...

class Bitmap: ...

BrushStyle = int
BRUSHSTYLE_SOLID: BrushStyle

class Brush:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, colour: Union[Tuple[int, int, int], List[int], Colour], style: BrushStyle = BRUSHSTYLE_SOLID) -> None: ...
    @overload
    def __init__(self, stippleBitmap: Bitmap) -> None: ...
    @overload
    def __init__(self, brush: Brush) -> None: ...

DefaultPosition: Point

DefaultSize: Size

class Sizer:
    @overload
    def Add(self, window: Window, flags: SizerFlags) -> None: ...
    @overload
    def Add(self, window: Window, proportion: int = 0, flag: int = 0, border: int = 0, userData: Optional[PyUserData] = None) -> None: ...
    @overload
    def Add(self, sizer: Sizer, flags: SizerFlags) -> None: ...
    @overload
    def Add(self, sizer: Sizer, proportion: int = 0, flag: int = 0, border: int = 0, userData: Optional[PyUserData] = None) -> None: ...
    @overload
    def Add(self, width: int, height: int, proportion: int = 0, flag: int = 0, border: int = 0, userData: Optional[PyUserData] = None) -> None: ...
    @overload
    def Add(self, width: int, height: int, flags: SizerFlags) -> None: ...
    @overload
    def Add(self, item: SizerItem) -> None: ...
    @overload
    def Add(self,
            size: Union[Tuple[int, int], Size],
            proportion: int = 0,
            flag: int = 0,
            border: int = 0,
            userData: Optional[PyUserData] = None) -> None: ...
    @overload
    def Add(self, size: Union[Tuple[int, int], Size], flags: SizerFlags) -> None: ...
    def SetSizeHints(self, window: Window) -> None: ...
    def Fit(self, window: Window) -> Size: ...

class BoxSizer(Sizer):
    def __init__(self, orient: int = HORIZONTAL) -> None: ...

class StaticBoxSizer(Sizer):
    @overload
    def __init__(self,
                 box: StaticBox,
                 orient: int = HORIZONTAL) -> None: ...
    @overload
    def __init__(self,
                 orient: int,
                 parent: Window,
                 label: str = '') -> None: ...

class GridSizer(Sizer):
    ...

class FlexGridSizer(GridSizer):
    @overload
    def __init__(self, cols: int, vgap: int, hgap: int) -> None: ...
    @overload
    def __init__(self, cols: int, gap: Union[Tuple[int, int], Size] = Size(0, 0)) -> None: ...
    @overload
    def __init__(self, rows: int, cols: int, vgap: int, hgap: int) -> None: ...
    @overload
    def __init__(self, rows: int, cols: int, gap: Size = Size(0, 0)) -> None: ...

class NonOwnedWindow(Window): ...

class TopLevelWindow(NonOwnedWindow): ...

PanelNameStr: str

class Panel(Window):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 style: int = TAB_TRAVERSAL,
                 name: str = PanelNameStr) -> None: ...

class MenuItem: ...

ItemKind = int
ITEM_NORMAL: ItemKind

class Menu(EvtHandler):
    @overload
    def Append(self, id: int, item: str = '', helpString: str = '', kind: ItemKind = ITEM_NORMAL) -> None: ...
    @overload
    def Append(self, menuItem: MenuItem) -> None: ...

class MenuBar(Window):
    def Append(self, menu: Menu, title: str) -> None: ...

FrameNameStr: str

class Frame(TopLevelWindow):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 title: str = '',
                 pos: Point = DefaultPosition,
                 size: Union[Tuple[int, int], Size] = DefaultSize,
                 style: int = DEFAULT_DIALOG_STYLE,
                 name: str = FrameNameStr) -> None: ...
    def SetMenuBar(self, menuBar: MenuBar) -> None: ...

DialogNameStr: str

class Dialog(TopLevelWindow):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Optional[Window],
                 id: int = ID_ANY,
                 title: str = '',
                 pos: Point = DefaultPosition,
                 size: Union[Tuple[int, int], Size] = DefaultSize,
                 style: int = DEFAULT_DIALOG_STYLE,
                 name: str = DialogNameStr) -> None: ...
    def CreateButtonSizer(self, flags: int) -> Sizer: ...
    def GetReturnCode(self) -> int: ...
    def EndModal(self, retCode: int) -> None: ...

FileSelectorPromptStr: str
FileSelectorDefaultWildcardStr: str
FileDialogNameStr: str

FD_DEFAULT_STYLE: int
FD_OPEN: int
FD_CHANGE_DIR: int

class FileDialog(Dialog):
    def __init__(self,
                 parent: Window,
                 message: str = FileSelectorPromptStr,
                 defaultDir: str = '',
                 defaultFile: str = '',
                 wildcard: str = FileSelectorDefaultWildcardStr,
                 style: int = FD_DEFAULT_STYLE,
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 name: str = FileDialogNameStr) -> None: ...
    def ShowModal(self) -> int: ...
    def GetPath(self) -> str: ...

class Control(Window):
    @overload
    def GetSizeFromTextSize(self, xlen: int, ylen: int = -1) -> Size: ...
    @overload
    def GetSizeFromTextSize(self, tsize: Size) -> Size: ...
    def SetLabel(self, label: str) -> None: ...

StaticTextNameStr: str

class StaticText(Control):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 label: str = '',
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 style: int = 0,
                 name: str = StaticTextNameStr) -> None: ...
    def Wrap(self, width: int) -> None: ...

class SpinEvent(NotifyEvent): ...

class SpinCtrl(Control):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 value: str = '',
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 style: int = 0,
                 min: int = 0,
                 max: int = 100,
                 initial: int = 0,
                 name: str = 'wxSpinCtrl') -> None: ...
    def GetValue(self) -> int: ...
    def SetRange(self, minVal: int, maxVal: int) -> None: ...

ComboBoxNameStr: str

class Validator(EvtHandler): ...

DefaultValidator: Validator

ClientData = int

class ItemContainer:
    @overload
    def Append(self, item: str) -> None: ...
    @overload
    def Append(self, item: str, clientData: ClientData) -> None: ...
    @overload
    def Append(self, items: List[str]) -> None: ...
    def AppendItems(self, items: List[str]) -> None: ...
    def Clear(self) -> None: ...

CB_DROPDOWN: int

class ComboBox(Control, ItemContainer):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int,
                 value: str = '',
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 choices: List[str] = [],
                 style: int = 0,
                 validator: Validator = DefaultValidator,
                 name: str = ComboBoxNameStr) -> None: ...

class TextEntry:
    def GetValue(self) -> str: ...
    def SetValue(self, value: str) -> None: ...

TextCtrlNameStr: str

class TextCtrl(Control, TextEntry):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 value: str = '',
                 pos: Point = DefaultPosition,
                 size: Union[Tuple[int, int], Size] = DefaultSize,
                 style: int = 0,
                 validator: Validator = DefaultValidator,
                 name: str = TextCtrlNameStr) -> None: ...

class AnyButton(Control): ...

ButtonNameStr: str

class Button(AnyButton):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 label: str = '',
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 style: int = 0,
                 validator: Validator = DefaultValidator,
                 name: str = ButtonNameStr) -> None: ...

StaticBoxNameStr: str

class StaticBox(Control):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 label: str = '',
                 pos: Point = DefaultPosition,
                 size: Size = DefaultSize,
                 style: int = 0,
                 name: str = StaticBoxNameStr) -> None: ...

class DC: ...

FontFamily = int
FONTFAMILY_MODERN: FontFamily

FontStyle = int
NORMAL: FontStyle

FontWeight = int

FontEncoding = int
FONTENCODING_DEFAULT: FontEncoding

class FontInfo: ...

class NativeFontInfo: ...

class Font:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, font: Font) -> None: ...
    @overload
    def __init__(self, fontInfo: FontInfo) -> None: ...
    @overload
    def __init__(self,
                 pointSize: int,
                 family: FontFamily,
                 style: FontStyle,
                 weight: FontWeight,
                 underline: bool = False,
                 faceName: str = '',
                 encoding: FontEncoding = FONTENCODING_DEFAULT) -> None: ...
    @overload
    def __init__(self,
                 pixelSize: Size,
                 family: FontFamily,
                 style: FontStyle,
                 weight: FontWeight,
                 underline: bool = False,
                 faceName: str = '',
                 encoding: FontEncoding = FONTENCODING_DEFAULT) -> None: ...
    @overload
    def __init__(self, nativeInfoString: str) -> None: ...
    @overload
    def __init__(self, nativeInfo: NativeFontInfo) -> None: ...

class FontEnumerator:
    @staticmethod
    def IsValidFacename(facename: str) -> bool: ...

def GetDisplayPPI() -> Size: ...
