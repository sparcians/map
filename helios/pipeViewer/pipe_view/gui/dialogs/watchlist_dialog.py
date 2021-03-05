

import wx
from gui.widgets.transaction_list import TransactionList
from functools import partial

ID_WATCH_DELETE = wx.NewId()

## This class displays a list of transactions the user chooses to persistently show
class WatchListDlg(wx.Frame):
    __BASE_PROPERTIES = ('start', 'loc', 'annotation', 't_offset', 'period')
    __NON_QUERIED_PROPERTIES = ('t_offset', 'period')
    def __init__(self, parent):
        self.__layout_frame = parent
        self.__context = parent.GetContext()
        # holds a list of list indices that need updating every cycle
        self.__relative_indices = [] 

        # create GUI
        wx.Frame.__init__(self, parent, -1, 'Watch List', size=(500,800),
                       style=wx.MAXIMIZE_BOX|wx.RESIZE_BORDER|wx.CAPTION|wx.CLOSE_BOX|wx.SYSTEM_MENU)
        self.__list = TransactionList(self,
                             wx.NewId(),
                             parent.GetCanvas(),
                             name='listbox')
        self.__list.SetProperties(self.__BASE_PROPERTIES)
        
        self.__list.Bind(wx.EVT_LIST_ITEM_RIGHT_CLICK, self.__OnItemMenu)
  
        self.Bind(wx.EVT_CLOSE, lambda evt: self.Hide()) # Hide instead of closing

    def __OnItemMenu(self, evt):
        menu = wx.Menu()
        delete = menu.Append(ID_WATCH_DELETE, "Delete")
        # calculate current mouse position in relative coordinates
        mouse_pos = self.ScreenToClient(wx.GetMousePosition())
        
        self.Bind(wx.EVT_MENU, partial(self.__OnDeleteItem, index=evt.GetIndex()))
        self.PopupMenu(menu, mouse_pos)

    def __OnDeleteItem(self, evt, index):
        self.__list.Remove(index)
        index_of_removal = len(self.__relative_indices)
        if index in self.__relative_indices:
            index_of_removal = self.__relative_indices.index(index)
            self.__relative_indices.remove(index)
        
        for i in range(len(self.__relative_indices)):
            if i >= index_of_removal: # shift items after index
                self.__relative_indices[i]-=1

    def SetFields(self, fields):
        fields_new = fields[:]
        fields_new.extend(self.__BASE_PROPERTIES)
        self.__list.SetProperties(fields_new)

    def Add(self, loc, t_offset, relative=False):
        q_fields = list(self.__list.GetProperties())
        period = self.__context.GetLocationPeriod(loc)
        start = t_offset*period + self.__context.hc
        for p in self.__NON_QUERIED_PROPERTIES:
            q_fields.remove(p) # remove since this is not a valid db query
        info = self.__context.GetTransactionFields(start,
                                                   loc,
                                                   q_fields)
        info['t_offset'] = t_offset
        info['period'] = period
        idx = self.__list.Add(info)
        if relative:
            self.__relative_indices.append(idx)

    def Show(self):
        wx.Frame.Show(self)
        self.Raise()

    ## called every hc change. Updates relative entries
    def Update(self, hc):
        for relative_index in self.__relative_indices:
            transaction = self.__list.GetTransaction(relative_index)
            q_fields = list(self.__list.GetProperties())
            for p in self.__NON_QUERIED_PROPERTIES:
                q_fields.remove(p)
            t_offset = transaction.get('t_offset')
            period = transaction.get('period')
            updated = self.__context.GetTransactionFields(hc + t_offset*period,
                                                            transaction.get('loc'),
                                                            q_fields)
            # copy over
            for key in updated.keys():
                transaction[key] = updated[key]
            
            self.__list.RefreshTransaction(relative_index)

