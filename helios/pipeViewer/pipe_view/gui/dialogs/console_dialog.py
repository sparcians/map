

import wx
from wx.py.shell import Shell
from gui.layout_frame import Layout_Frame

## This class displays a python console
class ConsoleDlg(wx.Frame):
    def __init__(self, parent: Layout_Frame) -> None:
        self.__layout_frame = parent
        self.__context = parent.GetContext()
        
        # create GUI
        wx.Frame.__init__(self, parent, -1, 'Python Console', size=(500,300),
                       style=wx.MAXIMIZE_BOX|wx.RESIZE_BORDER|wx.CAPTION|wx.CLOSE_BOX|wx.SYSTEM_MENU)

        menu_bar = wx.MenuBar()
        menu = wx.Menu()
        menu_run_script = menu.Append(wx.NewId(), '&Run Script\tAlt-R', 'Run a script in the console.')
        menu_bar.Append(menu, '&Shell')
        self.SetMenuBar(menu_bar)

        self.__shell = Shell(self)
        
        # set up what user is given to work with
        self.__shell.interp.locals = {'context':self.__context}

        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide()) # Hide instead of closing
        self.Bind(wx.EVT_MENU, self.OnRunScript)

    def OnRunScript(self, evt: wx.MenuEvent) -> None:
        # Loop until user saves or cancels
        dlg = wx.FileDialog(self,
            'Run Python Script',
            wildcard='Python Scripts (*.py)|*.py',
            style=wx.FD_OPEN | wx.FD_CHANGE_DIR)
        dlg.ShowModal()
        ret = dlg.GetReturnCode()
        fp = dlg.GetPath()
        dlg.Destroy()

        if ret == wx.ID_CANCEL:
            return
        self.__shell.write('Running Script... '+fp)
        self.__shell.run('execfile(\'%s\')'%fp)

