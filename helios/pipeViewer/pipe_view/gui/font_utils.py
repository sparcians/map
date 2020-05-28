import wx

def ScaleFont(font_size):
    # Assume default DPI is 72
    DEFAULT_DPI = 72
    dpi = wx.ScreenDC().GetPPI()[1] # Point size determines font height, so look at the vertical DPI
    return int(round((font_size * DEFAULT_DPI) / dpi)) # Rounding up seems to give better results

