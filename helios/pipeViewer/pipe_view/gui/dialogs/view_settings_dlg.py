from __future__ import annotations
from collections import OrderedDict
from typing import Dict, Optional, Set, TypedDict, TYPE_CHECKING
import wx

from model.settings import ArgosSettings
if TYPE_CHECKING:
    from gui.layout_frame import Layout_Frame


class ViewSettingsDialog(wx.Dialog):
    class ViewSettingType(TypedDict):
        label: str
        type: str
        ctrl: Optional[wx.SpinCtrl]

    __SETTINGS: OrderedDict[str, ViewSettingType] = OrderedDict((
        (
            'layout_font_size',
            {
                'label': 'Layout Font Size',
                'type': 'spin',
                'ctrl': None
            }
        ),
        (
            'hover_font_size',
            {
                'label': 'Hover Font Size',
                'type': 'spin',
                'ctrl': None
            }
        ),
        (
            'playback_font_size',
            {
                'label': 'Playback Bar Font Size',
                'type': 'spin',
                'ctrl': None
            }
        )
    ))

    def __init__(self, parent: Layout_Frame, settings: ArgosSettings):
        wx.Dialog.__init__(self, parent, wx.NewId(), 'View Settings')

        self.__settings_controls = ViewSettingsDialog.__SETTINGS.copy()

        self.__changed_settings: Set[str] = set()
        self.__ctrl_map = {}
        sizer = wx.BoxSizer(wx.VERTICAL)
        setting_sizer = wx.FlexGridSizer(cols=2, gap=(10, 0))
        for k, v in self.__settings_controls.items():
            setting_sizer.Add(wx.StaticText(self,
                                            wx.NewId(),
                                            v['label']),
                              0,
                              wx.ALIGN_CENTER_VERTICAL)
            new_ctrl = None
            if v['type'] == 'spin':
                new_ctrl = wx.SpinCtrl(self,
                                       id=wx.NewId(),
                                       value=str(settings[k]))
                new_ctrl.SetRange(1, 999)
                new_ctrl.SetMinSize(
                    new_ctrl.GetSizeFromTextSize(new_ctrl.GetTextExtent('000'))
                )
            assert new_ctrl is not None
            self.__ctrl_map[new_ctrl.GetId()] = k
            setting_sizer.Add(new_ctrl, 0, wx.ALIGN_CENTER_VERTICAL)
            self.Bind(wx.EVT_SPINCTRL, self.OnSpinCtrl, new_ctrl)
            v['ctrl'] = new_ctrl

        sizer.Add(setting_sizer, 0, wx.ALIGN_CENTER_HORIZONTAL | wx.ALL, 10)
        sizer.Add(self.CreateButtonSizer(wx.OK | wx.CANCEL),
                  0,
                  wx.ALIGN_CENTER_HORIZONTAL | wx.ALL,
                  10)
        sizer.SetSizeHints(self)
        self.SetSizer(sizer)

    def OnSpinCtrl(self, evt: wx.SpinEvent) -> None:
        self.__changed_settings.add(self.__ctrl_map[evt.GetId()])

    def GetSettings(self) -> Dict[str, int]:
        def get_value(ctrl: Optional[wx.SpinCtrl]) -> int:
            assert ctrl is not None
            return ctrl.GetValue()
        return {
            k: get_value(self.__settings_controls[k]['ctrl'])
            for k in self.__changed_settings
        }
