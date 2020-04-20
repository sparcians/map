import wx


# # This class is a GUI list control element that shows transactions.
class TransactionList(wx.ListCtrl):

    def __init__(self, parent, id, canvas, name = '', style = wx.LC_REPORT | wx.SUNKEN_BORDER):
        wx.ListCtrl.__init__(self, parent = parent, id = wx.NewId(), name = name, style = style)
        self.SetFont(wx.Font(8, wx.FONTFAMILY_MODERN, wx.FONTSTYLE_NORMAL, wx.FONTWEIGHT_NORMAL, faceName = 'Monospace'))

        # used for coloring
        self.__canvas = canvas
        # list of dictionary of properties
        self.__transactions = []
        # properties to show.
        self.__properties = ['start', 'location', 'annotation'] # must have at least 1
        # insertion point for elements at end
        self.__current_new_idx = 0
        self.__colorize = True
        self.RefreshAll()

    def GetProperties(self):
        return tuple(self.__properties)

    def SetProperties(self, properties):
        self.__properties = properties[:]
        self.RefreshAll()

    def Clear(self):
        self.__transactions = []

    def Colorize(self, colorize):
        self.__colorize = colorize

    # # Destroy all graphical elements and recreate them
    def RefreshAll(self):
        self.ClearAll()
        # make header
        width = int(self.GetClientSize()[0] / len(self.__properties))
        if width < 100:
            width = 100
        for col_idx, column in enumerate(self.__properties):
            self.InsertColumn(col_idx, column)
            self.SetColumnWidth(col_idx, width)
        self.__current_new_idx = 0
        for transaction in self.__transactions:
            self.__AddGraphicalTransaction(transaction)

    def RefreshTransaction(self, index):
        transaction = self.__transactions[index]
        for col_idx, prop in enumerate(self.__properties):
            self.SetItem(index, col_idx, str(transaction.get(prop)))
        annotation = transaction.get('annotation')
        if self.__colorize and annotation:
            color = self.__canvas.GetAutocolorColor(annotation)
        else:
            color = (255, 255, 255)
        self.SetItemBackgroundColour(index, color)

    def GetTransaction(self, index):
        return self.__transactions[index]

    def __AddGraphicalTransaction(self, transaction):
        self.InsertItem(self.__current_new_idx, str(transaction.get(self.__properties[0])))
        self.RefreshTransaction(self.__current_new_idx)
        self.__current_new_idx += 1

    # # Add a new element to bottom of list. New item must be dictionary of properties.
    def Add(self, transaction_properties):
        self.__transactions.append(transaction_properties)
        self.__AddGraphicalTransaction(transaction_properties)
        return self.__current_new_idx - 1

    def Remove(self, index):
        del self.__transactions[index]
        self.DeleteItem(index)
        self.__current_new_idx -= 1

    # # Attempts to resize the columns based on size
    #  @pre All content should be added before this is called (once)
    def FitColumns(self):
        for idx, _ in enumerate(self.__properties):
            self.SetColumnWidth(idx, wx.LIST_AUTOSIZE)
        self.Layout()
