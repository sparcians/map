import wx

def ScaleFont(font_size):
    # Assume default DPI is 72
    DEFAULT_DPI = 72
    dpi = wx.GetDisplayPPI()[1] # Point size determines font height, so look at the vertical DPI
    return int(round((font_size * DEFAULT_DPI) / dpi)) # Rounding up seems to give better results

def GetMonospaceFont(size):
    face_name = 'Monospace'
    if not wx.FontEnumerator.IsValidFacename(face_name):
        # Pick a fallback generic modern font (not by name)
        face_name = ''
    return wx.Font(ScaleFont(size), wx.FONTFAMILY_MODERN, wx.NORMAL, wx.NORMAL, faceName=face_name)

def GetDefaultFontSize():
    return 11

def GetDefaultControlFontSize():
    return 12

_DEFAULT_FONT = None
def GetDefaultFont():
    global _DEFAULT_FONT
    if _DEFAULT_FONT is None:
        _DEFAULT_FONT = GetMonospaceFont(GetDefaultFontSize())
    return _DEFAULT_FONT
