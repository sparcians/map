from __future__ import annotations
import os
from typing import Optional
import wx

__DPI: Optional[float] = None


def _GetDPI() -> float:
    global __DPI
    if __DPI is None:
        # Use Tk to get display DPI on Wayland platforms since a disconnected
        # primary display may still show up as present to wx
        if os.environ.get('WAYLAND_DISPLAY') is not None:
            from tkinter import Tk
            root = Tk()
            root.withdraw()
            __DPI = root.winfo_fpixels('1i')
        else:
            __DPI = wx.GetDisplayPPI()[1]
    return __DPI


def ScaleFont(font_size: int) -> int:
    DEFAULT_DPI = 72
    dpi = _GetDPI()
    assert dpi > 0
    # Rounding up seems to give better results
    return int(round((font_size * DEFAULT_DPI) / dpi))


def GetMonospaceFont(size: int) -> wx.Font:
    face_name = 'Monospace'
    if not wx.FontEnumerator.IsValidFacename(face_name):
        # Pick a fallback generic modern font (not by name)
        face_name = ''
    return wx.Font(ScaleFont(size),
                   wx.FONTFAMILY_MODERN,
                   wx.NORMAL,
                   wx.NORMAL,
                   faceName=face_name)


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
