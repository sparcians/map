from __future__ import annotations
import os
import re
from typing import Optional
import wx

__DPI: Optional[float] = None


def _GetDPI() -> float:
    global __DPI
    if __DPI is None:
        # The macOS handles scaling for us. Always return the default DPI of 72
        os_desc = wx.GetOsDescription()
        pattern = re.compile('macOS')
        match = re.search(pattern, os_desc)
        if match:
            __DPI = 72
        # Use Tk to get display DPI on Wayland platforms since a disconnected
        # primary display may still show up as present to wx
        elif os.environ.get('WAYLAND_DISPLAY') is not None:
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
    # Initialize to a fallback generic modern font (not by name)
    # just in case we don't find a specific font
    face_name = ''

    # Now look for a specific font. Monospace is found on Ubuntu.
    # Menlo on the Mac
    for name in ['Monospace', 'Menlo']:
        if wx.FontEnumerator.IsValidFacename('name'):
            face_name = name
            break

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
