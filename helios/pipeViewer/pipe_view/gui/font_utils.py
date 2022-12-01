from __future__ import annotations
from tkinter import Tk
from typing import Optional
import wx

__DPI: Optional[float] = None

def _GetDPI() -> float:
    global __DPI
    if __DPI is None:
        root = Tk()
        root.withdraw()
        __DPI = root.winfo_fpixels('1i')
    return __DPI

def ScaleFont(font_size: int) -> int:
    DEFAULT_DPI = 72
    dpi = _GetDPI()
    assert dpi > 0
    return int(round((font_size * DEFAULT_DPI) / dpi)) # Rounding up seems to give better results

def GetMonospaceFont(size: int) -> wx.Font:
    face_name = 'Monospace'
    if not wx.FontEnumerator.IsValidFacename(face_name):
        # Pick a fallback generic modern font (not by name)
        face_name = ''
    return wx.Font(ScaleFont(size), wx.FONTFAMILY_MODERN, wx.NORMAL, wx.NORMAL, faceName=face_name)

def GetDefaultFontSize() -> int:
    return 11

def GetDefaultControlFontSize() -> int:
    return 12

_DEFAULT_FONT = None
def GetDefaultFont() -> wx.Font:
    global _DEFAULT_FONT
    if _DEFAULT_FONT is None:
        _DEFAULT_FONT = GetMonospaceFont(GetDefaultFontSize())
    return _DEFAULT_FONT
