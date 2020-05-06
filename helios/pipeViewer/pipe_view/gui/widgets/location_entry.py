import wx

## An Auto-suggesting Box for Entry of Location Strings
class LocationEntry(wx.ComboBox):
    def __init__(self, parent, value, location_tree, style=0, **par):
        wx.ComboBox.__init__(self, parent, wx.ID_ANY, value,
                        style=style|wx.CB_DROPDOWN, choices=[], **par)
        self.__location_tree = location_tree
        self.AppendItems(self.GetChoices(value))
        self.Bind(wx.EVT_TEXT, self.EvtText)

    def GetChoices(self, string):
        keys = string.split('.')
        current_level = self.__location_tree
        last_level = keys.pop()
        base = ''
        while keys and current_level:
            key = keys.pop(0)
            if base:
                base+='.'+key
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
                        return_list.append(base+'.'+p_entry)
                return return_list

    def EvtText(self, event):
        currentText = event.GetString()
        self.Clear()
        self.AppendItems(self.GetChoices(currentText))
