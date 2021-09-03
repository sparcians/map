from collections import OrderedDict
import wx

class ViewSettingsDialog(wx.Dialog):
    __SETTINGS = OrderedDict((
        ('layout_font_size',
            {
                'label': 'Layout Font Size',
                'type': 'spin',
                'ctrl': None
            }
        ),
        ('hover_font_size',
            {
                'label': 'Hover Font Size',
                'type': 'spin',
                'ctrl': None
            }
        ),
        ('playback_font_size',
            {
                'label': 'Playback Bar Font Size',
                'type': 'spin',
                'ctrl': None
            }
        )
    ))

    def __init__(self, parent, settings):
        wx.Dialog.__init__(self, parent, wx.NewId(), 'View Settings')

        self.__settings_controls = ViewSettingsDialog.__SETTINGS.copy()

        self.__changed_settings = set()
        self.__ctrl_map = {}
        sizer = wx.BoxSizer(wx.VERTICAL)
        setting_sizer = wx.FlexGridSizer(cols=2, gap=(10,0))
        for k, v in self.__settings_controls.items():
            setting_sizer.Add(wx.StaticText(self, wx.NewId(), v['label']), 0, wx.ALIGN_CENTER_VERTICAL)
            new_ctrl = None
            if v['type'] == 'spin':
                new_ctrl = wx.SpinCtrl(self, id=wx.NewId(), value=str(settings[k]))
                new_ctrl.SetRange(1, 999)
                new_ctrl.SetMinSize(new_ctrl.GetSizeFromTextSize(new_ctrl.GetTextExtent('000')))
            self.__ctrl_map[new_ctrl.GetId()] = k
            setting_sizer.Add(new_ctrl, 0, wx.ALIGN_CENTER_VERTICAL)
            self.Bind(wx.EVT_SPINCTRL, self.OnSpinCtrl, new_ctrl)
            v['ctrl'] = new_ctrl

        sizer.Add(setting_sizer, 0, wx.ALIGN_CENTER_HORIZONTAL | wx.ALL, 10)
        sizer.Add(self.CreateButtonSizer(wx.OK | wx.CANCEL), 0, wx.ALIGN_CENTER_HORIZONTAL | wx.ALL, 10)
        sizer.SetSizeHints(self)
        self.SetSizer(sizer)

    def OnSpinCtrl(self, evt):
        self.__changed_settings.add(self.__ctrl_map.get(evt.GetId()))

    def GetSettings(self):
        return {k: self.__settings_controls[k]['ctrl'].GetValue() for k in self.__changed_settings}
