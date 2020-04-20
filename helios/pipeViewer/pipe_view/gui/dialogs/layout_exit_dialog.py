
import sys
import wx
import os

import wx.lib.platebtn as platebtn

## SearchDialog is a window that enables the user to enter a string, conduct a search and
# jump to a location and transaction based on the result. It gets its data from search_handle.py
class LayoutExitDialog(wx.Dialog):
    def __init__(self, frame):
        super(LayoutExitDialog, self).__init__(frame, -1, title='Unsaved Changes to this Layout')

        layout_name = frame.GetContext().GetLayout().GetFilename()
        if layout_name is not None:
            layout_name = os.path.split(layout_name)[-1]
        else:
            layout_name = '<unsaved layout>'

        fnt_filename = wx.Font(13, wx.NORMAL, wx.NORMAL, wx.NORMAL, wx.FONTWEIGHT_BOLD)

        lbl_filename = wx.StaticText(self, -1, layout_name)
        lbl_filename.SetFont(fnt_filename)

        BTN_ICON_SIZE = 16
        bmp_discard = wx.ArtProvider.GetBitmap(wx.ART_DELETE, wx.ART_MESSAGE_BOX, (BTN_ICON_SIZE,BTN_ICON_SIZE))
        bmp_save = wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE, wx.ART_MESSAGE_BOX, (BTN_ICON_SIZE,BTN_ICON_SIZE))
        bmp_return = wx.ArtProvider.GetBitmap(wx.ART_GO_BACK, wx.ART_MESSAGE_BOX, (BTN_ICON_SIZE,BTN_ICON_SIZE))

        STYLE = platebtn.PB_STYLE_SQUARE
        btn_discard = platebtn.PlateButton(self, wx.ID_DELETE, ' Discard Changes ', bmp_discard, style=STYLE)
        btn_discard.SetBackgroundColour((240,80,80))
        btn_save = platebtn.PlateButton(self, wx.ID_SAVE, ' Save ', bmp_save, style=STYLE)
        btn_save.SetBackgroundColour((170,170,170))
        btn_return = platebtn.PlateButton(self, wx.ID_BACKWARD, ' Back to Editing ', bmp_return, style=STYLE)
        btn_return.SetBackgroundColour((170,170,170))

        button_sizer = wx.BoxSizer(wx.HORIZONTAL)
        button_sizer.Add(btn_discard, 0, wx.ALL, 4)
        button_sizer.Add((50,1), 0)
        button_sizer.Add(btn_save, 0, wx.ALL, 4)
        button_sizer.Add(btn_return, 0, wx.ALL, 4)

        message_sizer = wx.BoxSizer(wx.VERTICAL)
        message_sizer.Add((1,10), 0)
        message_sizer.Add(wx.StaticText(self, -1, 'Layout has been modified since last save!'), 0, wx.ALL, 8)
        message_sizer.Add((1,1), 0)
        message_sizer.Add(lbl_filename, 0, wx.ALL, 8)
        message_sizer.Add((1,20), 0)

        bmp = wx.ArtProvider.GetBitmap(wx.ART_WARNING, wx.ART_MESSAGE_BOX, (64,64))
        upper_sizer = wx.BoxSizer(wx.HORIZONTAL)
        upper_sizer.Add(wx.StaticBitmap(self, -1, bmp, (0,0), (bmp.GetWidth(), bmp.GetHeight())), 0, wx.ALL, 10)
        upper_sizer.Add(message_sizer, 1, wx.EXPAND)

        main_sizer = wx.BoxSizer(wx.VERTICAL)
        main_sizer.Add(upper_sizer, 1, wx.EXPAND)
        main_sizer.Add(button_sizer, 0, wx.EXPAND | wx.ALL, 5)

        self.SetSizer(main_sizer)
        self.Layout()
        self.Fit()

        def GenAssigner(val):
            def Assigner(*args):
                self.__result = val
                self.EndModal(val)
            return Assigner

        btn_discard.Bind(wx.EVT_BUTTON, GenAssigner(wx.ID_DELETE))
        btn_save.Bind(wx.EVT_BUTTON, GenAssigner(wx.ID_SAVE))
        btn_return.Bind(wx.EVT_BUTTON, GenAssigner(wx.ID_CANCEL))
        self.Bind(wx.EVT_CLOSE, GenAssigner(wx.ID_CANCEL))

        self.__result = None

    ## Show modal dialog.
    #  @return wx.ID_DELETE to discard changes, wx.ID_SAVE to save & quit, and
    #  wx.ID_CANCEL to return
    def ShowModal(self):
        super(LayoutExitDialog, self).ShowModal()
        return self.__result

    def GetReturnCode(self):
        return self.__result