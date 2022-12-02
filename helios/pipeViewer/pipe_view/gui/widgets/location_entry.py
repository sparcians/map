from __future__ import annotations
from typing import Any, List, Optional

import wx

from model.location_manager import LocationTree


# An Auto-suggesting Box for Entry of Location Strings
class LocationEntry(wx.ComboBox):
    def __init__(self,
                 parent: wx.Panel,
                 value: str,
                 location_tree: LocationTree,
                 style: int = 0,
                 **kwargs: Any) -> None:
        wx.ComboBox.__init__(self,
                             parent,
                             wx.ID_ANY,
                             value,
                             style=style | wx.CB_DROPDOWN,
                             choices=[],
                             **kwargs)
        self.__location_tree = location_tree
        self.AppendItems(self.GetChoices(value))
        self.Bind(wx.EVT_TEXT, self.EvtText)

    def GetChoices(self, string: str) -> List[str]:
        keys = string.split('.')
        current_level: Optional[LocationTree] = self.__location_tree
        last_level = keys.pop()
        base = ''
        while keys and current_level:
            key = keys.pop(0)
            if base:
                base += '.' + key
            else:
                base = key
            current_level = current_level.get(key)
        if current_level is None:
            return []
        else:
            if not base:
                return list(current_level.keys())
            else:
                return_list = []
                for p_entry in current_level.keys():
                    if p_entry.startswith(last_level):
                        return_list.append(base + '.' + p_entry)
                return return_list

    def EvtText(self, event: wx.CommandEvent) -> None:
        currentText = event.GetString()
        self.Clear()
        self.AppendItems(self.GetChoices(currentText))
