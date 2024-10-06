# @package select_db_dbg.py
#  @brief Dialog for selecting a database file

from __future__ import annotations
import os
from typing import Optional
import wx
import wx.lib.scrolledpanel as scrolledpanel


# Dialog for selecting an Argos database.
#
#  Use ShowModal to display the dialog and then use GetPrefix to see selected
#  filename
class SelectDatabaseDlg(wx.Dialog):

    # File name and extension for simulation info files
    #
    #  This can be appended to a prefix to get simulation information
    INFO_FILE_EXTENSION = '.info'

    # Initialized the dialog
    #  @param init_prefix Value of prefix to show in the box by default.
    #  Must be a str or None
    def __init__(self, init_prefix: Optional[str] = None) -> None:
        wx.Dialog.__init__(self,
                           None,
                           title='Select an Argos transaction database',
                           size=(800, 380))

        if not isinstance(init_prefix, str) and init_prefix is not None:
            raise TypeError(
                f'init_prefix must be a str or None, is a {type(init_prefix)}'
            )

        self.__prefix: Optional[str] = None  # Updated in CheckSelectionState

        if init_prefix is not None:
            filepath = init_prefix + self.INFO_FILE_EXTENSION
        else:
            filepath = os.getcwd()

        # Controls
        info = wx.StaticText(
            self,
            label=f'Specify a {self.INFO_FILE_EXTENSION} file from an argos '
                  'transaction database'
        )
        info.Wrap(self.GetSize()[0] - 20)

        self.__file_txt = wx.TextCtrl(self, size=(160, -1), value=filepath)
        self.__orig_txt_colour = self.__file_txt.GetBackgroundColour()
        file_btn = wx.Button(self, id=wx.ID_FIND)

        quit_btn = wx.Button(self, id=wx.ID_EXIT)
        self.__ok_btn = wx.Button(self, id=wx.ID_OK)

        file_info_box = wx.StaticBox(self, label='Simulation Info')
        self.__scroll_win = scrolledpanel.ScrolledPanel(self)
        self.__scroll_win.SetupScrolling()
        self.__file_info = wx.StaticText(self.__scroll_win, label='')

        # Bindings

        quit_btn.Bind(wx.EVT_BUTTON, self.__OnClose)
        self.__ok_btn.Bind(wx.EVT_BUTTON, self.__OnOk)
        file_btn.Bind(wx.EVT_BUTTON, self.__OnFindFile)
        self.__file_txt.Bind(wx.EVT_TEXT, self.__OnChangeFilename)

        # Layout
        sbs = wx.StaticBoxSizer(file_info_box, wx.HORIZONTAL)
        sbs.Add(self.__scroll_win, 1, wx.EXPAND | wx.ALL, 5)

        sws = wx.BoxSizer(wx.VERTICAL)
        sws.Add(self.__file_info, 0, wx.EXPAND)
        self.__scroll_win.SetSizer(sws)
        sws.Fit(self.__scroll_win)

        open_row = wx.BoxSizer(wx.HORIZONTAL)
        open_row.Add(self.__file_txt, 1, wx.EXPAND)
        open_row.Add((10, 1), 0, wx.EXPAND)
        open_row.Add(file_btn, 0, wx.EXPAND)

        buttons_row = wx.BoxSizer(wx.HORIZONTAL)
        buttons_row.Add(quit_btn, 0, wx.ALIGN_LEFT | wx.ALIGN_BOTTOM)
        buttons_row.Add((1, 1), 1, wx.EXPAND)
        buttons_row.Add(self.__ok_btn, 0, wx.ALIGN_BOTTOM)

        sz = wx.BoxSizer(wx.VERTICAL)
        sz.Add(info, 0, wx.EXPAND)
        sz.Add((1, 15), 0, wx.EXPAND)
        sz.Add(open_row, 0, wx.EXPAND)
        sz.Add((1, 25), 0, wx.EXPAND)
        sz.Add(sbs, 1, wx.EXPAND)
        sz.Add((1, 25), 0, wx.EXPAND)
        sz.Add(buttons_row, 0, wx.EXPAND)

        border = wx.BoxSizer(wx.VERTICAL)
        border.Add(sz, 1, wx.EXPAND | wx.ALL, 10)
        self.SetSizer(border)

        self.SetAutoLayout(True)

        self.__CheckSelectionState()

    def Show(self, show: bool = True) -> bool:
        raise NotImplementedError(
            'Cannot Show() this dialog. Use ShowModal instead'
        )

    # Gets the prefix selected by the dialog
    #  @return The prefix selected while the dialog was shown. Is a string if
    #  found and None if no database was chosen
    #
    #  This should be checked after invoking ShowModal() on this object
    def GetPrefix(self) -> Optional[str]:
        return self.__prefix

    # Handler for Close button
    def __OnClose(self, evt: wx.CommandEvent) -> None:
        self.__prefix = None
        self.EndModal(wx.CANCEL)

    # Handler for Ok button
    def __OnOk(self, evt: wx.CommandEvent) -> None:
        # self.__filename already set before this button was enabled
        self.EndModal(wx.OK)

    # Handler for Find button
    def __OnFindFile(self, evt: wx.CommandEvent) -> None:
        ext = self.INFO_FILE_EXTENSION
        dlg = wx.FileDialog(
            self,
            "Select Argos database simulation.info file",
            defaultFile=self.__file_txt.GetValue(),
            wildcard=f'Argos Simulation info files (*{ext})|*{ext}'
        )
        dlg.ShowModal()

        fp = dlg.GetPath()

        if fp is not None and fp != '':
            self.__file_txt.SetValue(fp)

        self.__CheckSelectionState()

    # Handler for Changing the filename in file_txt
    def __OnChangeFilename(self, evt: wx.CommandEvent) -> None:
        self.__CheckSelectionState()

    # Checks on the value in the self.__file_txt box to see if it points to a
    # valid simulation
    #
    # Updates self.__prefix
    # Updates or clears self.__file_info and en/disables self.__ok_btn
    # depending on whether selection points to a valid file. Also changes
    # colors of box
    def __CheckSelectionState(self) -> None:
        filepath = self.__file_txt.GetValue()

        simulation_file_full_extention = 'simulation'+self.INFO_FILE_EXTENSION
        suffix_pos = filepath.find(simulation_file_full_extention)
        if suffix_pos != len(filepath) - len(simulation_file_full_extention):
            valid = False
        elif not os.path.exists(filepath):
            valid = False
        else:
            try:
                summary = ''
                with open(filepath, 'r') as f:
                    while True:
                        ln = f.readline()
                        if ln == '':
                            break
                        summary += ln

                self.__file_info.SetLabel(summary)

            except IOError:
                valid = False
            else:
                valid = True

        self.__ok_btn.Enable(valid)

        if valid:
            self.__prefix = filepath[:suffix_pos]
            self.__file_txt.SetBackgroundColour(wx.Colour(235, 255, 235))
        else:
            self.__prefix = None
            self.__file_info.SetLabel('')
            self.__file_txt.SetBackgroundColour(wx.Colour(255, 220, 220))

        self.__scroll_win.FitInside()
        self.__scroll_win.Layout()
