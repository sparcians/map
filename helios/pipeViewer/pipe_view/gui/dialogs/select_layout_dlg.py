

## @package select_layout_dbg.py
#  @brief Dialog for selecting a layout file

import sys
import os
import wx
import wx.lib.scrolledpanel as scrolledpanel

from model.layout import Layout

## Dialog for selecting an Argos layout file.
#
#  Use ShowModal to display the dialog and then use GetPrefix to see selected
#  filename
class SelectLayoutDlg(wx.Dialog):

    ## Initialized the dialog
    #  @param init_prefix Value of the filename to show in the alf file textbox by default
    #  Must be a str or None.
    def __init__(self, init_prefix=None):
        wx.Dialog.__init__(self,
                           None,
                           title='Select an Argos layout file',
                           size=(800,-1))

        if not isinstance(init_prefix, str) and init_prefix is not None:
            raise TypeError('init_prefix must be a str or None, is a {0}'.format(type(init_prefix)))

        self.__filename = None # Updated in CheckSelectionState

        if init_prefix is not None:
            filepath = init_prefix
        else:
            filepath = os.getcwd()

        # Controls
        info = wx.StaticText(self,
                             label='The layout controls how information in the ' \
                             'transaction database is displayed on the screen. ' \
                             'Once loaded, layouts can be modified. A "shared" layout ' \
                             'can be can be modified by several view frames simultaneously\n' \
                             .format(Layout.LAYOUT_FILE_EXTENSION))
        info.Wrap(self.GetSize()[0]-5)


        sbox_src = wx.StaticBox(self, label='Layout File')
        self.__rad_blank = wx.RadioButton(self, -1, 'Create a blank Layout')
        self.__rad_load = wx.RadioButton(self, -1, 'Specify an Argos layout file ({0})' \
                                         .format(Layout.LAYOUT_FILE_EXTENSION))

        self.__panel_sel_file = wx.Panel(self) # To be enabled/disabled
        self.__file_txt = wx.TextCtrl(self.__panel_sel_file, size=(160,-1), value=filepath)
        self.__orig_txt_colour = self.__file_txt.GetBackgroundColour()
        file_btn = wx.Button(self.__panel_sel_file, id=wx.ID_FIND)

        sbox_options = wx.StaticBox(self, label='Layout Options')
        self.__chk_shared = wx.CheckBox(self, label='Shared Layout (NOT YET IMPLEMENTED)')
        self.__chk_shared.Disable() # Until implemented

        quit_btn = wx.Button(self, id=wx.ID_EXIT)
        self.__ok_btn = wx.Button(self, id=wx.ID_OK)

        # Bindings

        quit_btn.Bind(wx.EVT_BUTTON, self.__OnClose)
        self.__ok_btn.Bind(wx.EVT_BUTTON, self.__OnOk)
        file_btn.Bind(wx.EVT_BUTTON, self.__OnFindFile)
        self.__file_txt.Bind(wx.EVT_TEXT, self.__OnChangeFilename)
        self.Bind(wx.EVT_RADIOBUTTON, self.__OnChangeFilename) # Update if user changes radio selection

        # Layout

        INDENT = 35
        open_row = wx.BoxSizer(wx.HORIZONTAL)
        open_row.Add((INDENT,1), 0)
        open_row.Add(self.__file_txt, 1, wx.EXPAND)
        open_row.Add((10,1), 0, wx.EXPAND)
        open_row.Add(file_btn, 0, wx.EXPAND)
        self.__panel_sel_file.SetSizer(open_row)

        sbs_src_border = wx.BoxSizer(wx.VERTICAL)
        sbs_src_border.Add(self.__rad_blank, 0, wx.EXPAND | wx.ALIGN_BOTTOM)
        sbs_src_border.Add((1,25), 0, wx.EXPAND)
        sbs_src_border.Add(self.__rad_load, 0, wx.EXPAND | wx.ALIGN_BOTTOM)
        sbs_src_border.Add((1,15), 0, wx.EXPAND)
        sbs_src_border.Add(self.__panel_sel_file, 0, wx.EXPAND)

        sbs_src = wx.StaticBoxSizer(sbox_src, wx.VERTICAL)
        sbs_src.Add(sbs_src_border, 1, wx.EXPAND | wx.ALL, 10)

        sbs_opts_border = wx.BoxSizer(wx.VERTICAL)
        sbs_opts_border.Add(self.__chk_shared, 0, wx.EXPAND | wx.ALIGN_BOTTOM)

        sbs_opts = wx.StaticBoxSizer(sbox_options, wx.VERTICAL)
        sbs_opts.Add(sbs_opts_border, 1, wx.EXPAND | wx.ALL, 10)
        
        buttons_row = wx.BoxSizer(wx.HORIZONTAL)
        buttons_row.Add(quit_btn, 0, wx.ALIGN_LEFT | wx.ALIGN_BOTTOM)
        buttons_row.Add((1,1), 1, wx.EXPAND)
        buttons_row.Add(self.__ok_btn, 0, wx.ALIGN_RIGHT | wx.ALIGN_BOTTOM)
        ok_best_size = self.__ok_btn.GetBestSize()
        quit_best_size = quit_btn.GetBestSize()
        min_width = max(ok_best_size.GetWidth(), quit_best_size.GetWidth())
        min_height = 60
        min_size = wx.Size(min_width, min_height)
        buttons_row.SetMinSize(min_size)

        sz = wx.BoxSizer(wx.VERTICAL)
        sz.Add(info, 0, wx.EXPAND)
        sz.Add(sbs_src, 0, wx.EXPAND)
        sz.Add((1,15), 0, wx.EXPAND)
        sz.Add(sbs_opts, 0, wx.EXPAND)
        sz.Add((1,15), 0, wx.EXPAND)
        sz.Add(buttons_row, 0, wx.EXPAND)

        border = wx.BoxSizer(wx.VERTICAL)
        border.Add(sz, 1, wx.EXPAND | wx.ALL, 10)
        self.SetSizer(border)

        self.Fit()
        self.SetAutoLayout(1)
        
        self.__CheckSelectionState()

    def Show(self):
        raise NotImplementedError('Cannot Show() this dialog. Use ShowModal instead')

    ## Gets the Argos layout filename selected by the dialog
    #  @return The filename selected while the dialog was shown. Is a string if
    #  found and None if no database was chosen
    #
    #  This should be checked after invoking ShowModal() on this object
    def GetFilename(self):
        return self.__filename

    ## Handler for Close button
    def __OnClose(self, evt):
        self.__filename = None
        self.EndModal(wx.CANCEL)

    ## Handler for Ok button
    def __OnOk(self, evt):
        # self.__filename already set before this button was enabled
        self.EndModal(wx.OK)

    ## Handler for Find button
    def __OnFindFile(self, evt):
        dlg = wx.FileDialog(self, "Select Argos layout file",
                            defaultFile=self.__file_txt.GetValue(),
                            wildcard='Argos layout files (*{0})|*{0}' \
                            .format(Layout.LAYOUT_FILE_EXTENSION))
        dlg.ShowModal()

        fp = dlg.GetPath()
        if fp is not None and fp != '':
            self.__file_txt.SetValue(fp)

        self.__CheckSelectionState()

    ## Handler for Changing the filename in file_txt
    def __OnChangeFilename(self, evt):
        self.__CheckSelectionState()

    ## Checks on the value in the self.__file_txt box to see if it points to a
    #  valid simulation
    #
    #  Updates self.__filename
    #  Updates or clears self.__file_info and en/disables self.__ok_btn depending
    #  on whether selection points to a valid file. Also changes colors of box
    def __CheckSelectionState(self):
        if self.__rad_blank.GetValue():
            self.__panel_sel_file.Disable()
            filepath = None
            valid = True
            reason = ''
            self.__file_txt.SetBackgroundColour(self.__orig_txt_colour)
        else: # self.__rad_load.GetValue()
            self.__panel_sel_file.Enable()
            filepath = self.__file_txt.GetValue()
            suffix_pos = filepath.find(Layout.LAYOUT_FILE_EXTENSION)
            if suffix_pos != len(filepath) - len(Layout.LAYOUT_FILE_EXTENSION):
                valid = False
                reason = 'Filename does not contain suffix "{0}"'.format(Layout.LAYOUT_FILE_EXTENSION)
            elif not os.path.exists(filepath):
                valid = False
                reason = 'File does not exist'
            else:
                # Assume the file is valid
                valid = True
                reason = ''

            if valid:
                self.__file_txt.SetBackgroundColour(wx.Colour(235, 255, 235))
            else:
                self.__file_txt.SetBackgroundColour(wx.Colour(255, 220, 220))
            
        self.__ok_btn.Enable(valid)
        
        if valid:
            self.__filename = filepath
        else:
            self.__filename = None
