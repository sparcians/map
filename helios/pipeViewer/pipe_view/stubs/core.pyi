from typing import Dict, Optional, Tuple, Union
import wx

from gui.layout_canvas import Layout_Canvas
from model.element import Element
from model.extension_manager import ExtensionManager

EXPR_NAMESPACE: dict
debug: function
error: function
info: function

class Renderer:
    def __init__(self) -> None: ...
    def drawElements(self, dc: wx.DC, canvas: Layout_Canvas, tick: int) -> None: ...
    def drawInfoRectangle(self,
                          tick: int,
                          element: Element,
                          dc: wx.DC,
                          canvas: Layout_Canvas,
                          rect: Union[wx.Rect, Tuple[int, int, int, int]],
                          annotation: Optional[Union[int, str]],
                          missing_needed_loc: bool,
                          content_type: str,
                          auto_color: Tuple[str, str], # type, basis
                          clip_x: Tuple[int, int], # (start, width)
                          schedule_settings: Optional[Tuple[int, int]] = None,
                          short_format: Optional[str] = None) -> None: ...
    def parseAnnotationAndGetColor(self,
                                   string_to_display: str,
                                   content_type: str,
                                   field_type: Optional[str] = None,
                                   field_string: Optional[str] = None) -> Tuple[str, wx.Brush]: ...
    def setBrushes(self, reason_brushes: Dict[int, wx.Brush], background_brushes: Dict[int, wx.Brush]) -> None: ...
    def setExtensions(self, extensions: ExtensionManager) -> None: ...
    def setFontFromDC(self, dc: wx.DC) -> None: ...

def extract_value(s: str, key: str, separators: str = '=:', skip_chars: int = 0, not_found: str = '') -> str: ...
def get_argos_version() -> int: ...
