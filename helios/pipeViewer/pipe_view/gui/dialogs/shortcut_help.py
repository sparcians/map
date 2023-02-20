from typing import Dict, Tuple, TypedDict, Union
import wx
import wx.html


class ShortcutHelpEntry(TypedDict, total=False):
    mod: str
    keys: Union[Tuple[str, ...], str]
    desc: str


class ShortcutHelp(wx.Frame):
    @staticmethod
    def is_mac_os() -> bool:
        os = wx.GetOsVersion()[0]
        return os in [wx.OS_MAC_OS, wx.OS_MAC_OSX_DARWIN, wx.OS_MAC]

    __SHIFT_KEY = 'Shift'
    __CTRL_KEY = 'Command' if is_mac_os() else 'CTRL'

    ShortcutHelpDict = Dict[str, Tuple[ShortcutHelpEntry, ...]]
    __SHORTCUT_ITEMS: ShortcutHelpDict = {
        'Global': (
            {'mod': __CTRL_KEY,
             'keys': ('-', 'Mousewheel Down'),
             'desc': 'Zoom out'},
            {'mod': __CTRL_KEY,
             'keys': ('=', 'Mousewheel Up'),
             'desc': 'Zoom in'},
            {'mod': __CTRL_KEY,
             'keys': '0',
             'desc': 'Reset zoom'},
            {'keys': 'Space',
             'desc': 'Step forward by 1 tick'},
            {'keys': 'G',
             'desc': 'Set focus to Jump box'},
            {'mod': __CTRL_KEY,
             'keys': 'G',
             'desc': 'Toggle background grid'},
            {'keys': 'N',
             'desc': 'Jump to next value change for current element'},
            {'mod': __SHIFT_KEY,
             'keys': 'N',
             'desc': 'Jump to previous value change for current element'}
        ),
        'View Mode': (
            {'keys': ('Up Arrow', 'Right Arrow'),
             'desc': 'Step forward by 1 cycle'},
            {'keys': ('Down Arrow', 'Left Arrow'),
             'desc': 'Step backward by 1 cycle'},
            {'keys': 'Page Up',
             'desc': 'Step forward by 10 cycles'},
            {'keys': 'Page Down',
             'desc': 'Step backward by 10 cycles'},
        ),
        'Edit Mode': (
            {'keys': 'Up Arrow',
             'desc': 'Move element up'},
            {'keys': 'Down Arrow',
             'desc': 'Move element down'},
            {'keys': 'Left Arrow',
             'desc': 'Move element left'},
            {'keys': 'Right Arrow',
             'desc': 'Move element right'},
            {'mod': __SHIFT_KEY,
             'keys': 'Arrow Key',
             'desc': 'Move element slower'},
            {'mod': __CTRL_KEY,
             'keys': 'Arrow Key',
             'desc': 'Move element faster'},
            {'mod': __SHIFT_KEY,
             'keys': 'G',
             'desc': 'Snap current element to the grid'}
        )
    }

    @staticmethod
    def __gen_message(shortcut_items: ShortcutHelpDict) -> str:
        msg = '<html>\n<body>\n<h2>Argos Keyboard Shortcuts</h2>\n'

        for k, v in shortcut_items.items():
            msg += f'<h3>{k}</h3>\n<ul>\n'
            for item in v:
                keys = item['keys']
                desc = item['desc']
                mod = item.get('mod')

                if mod is not None:
                    if isinstance(keys, tuple):
                        keys = tuple(f'{mod}</b>+<b>{k}' for k in keys)
                    else:
                        keys = f'{mod}</b>+<b>{keys}'

                if isinstance(keys, tuple):
                    keys = '</b></tt> <i>or</i> <tt><b>'.join(keys)

                msg += f'<li><tt><b>{keys}:</b></tt> {desc}</li>\n'
            msg += '</ul>\n'

        msg += '</body></html>'
        return msg

    __MESSAGE = __gen_message(__SHORTCUT_ITEMS)

    def __init__(self, parent: wx.Window, id: int) -> None:
        wx.Frame.__init__(
            self,
            parent,
            id,
            "Argos Shortcut Information",
            style=(wx.RESIZE_BORDER |
                   wx.CAPTION |
                   wx.CLOSE_BOX |
                   wx.CLIP_CHILDREN)
        )

        self.__message_ctrl = wx.html.HtmlWindow(
            self,
            style=wx.html.HW_SCROLLBAR_AUTO,
            size=(400, 525)
        )
        self.__message_ctrl.SetPage(ShortcutHelp.__MESSAGE)
        self.__close_btn = wx.Button(self, label="Close")
        self.__sizer = wx.BoxSizer(wx.VERTICAL)

        self.Bind(wx.EVT_BUTTON, self.OnCloseButton, self.__close_btn)

        self.__sizer.Add(self.__message_ctrl,
                         proportion=1,
                         flag=wx.ALL | wx.EXPAND,
                         border=5)
        self.__sizer.Add(self.__close_btn,
                         proportion=0,
                         flag=wx.ALL | wx.ALIGN_CENTER,
                         border=5)
        self.SetSizer(self.__sizer)

        self.Fit()
        self.Show(True)

    def OnCloseButton(self, evt: wx.CommandEvent) -> None:
        self.Destroy()
