from __future__ import annotations
from typing import Any, Dict, List, Tuple, Union, TYPE_CHECKING
import wx
from ..font_utils import GetMonospaceFont

if TYPE_CHECKING:
    from ..layout_canvas import Layout_Canvas
    from ..dialogs.search_dlg import SearchResult

TransactionListBaseEntry = Dict[str, Any]
TransactionListEntry = Union['SearchResult', TransactionListBaseEntry]


# This class is a GUI list control element that shows transactions.
class TransactionList(wx.ListCtrl):
    def __init__(self,
                 parent: wx.Frame,
                 canvas: Layout_Canvas,
                 name: str = '') -> None:
        wx.ListCtrl.__init__(self,
                             parent=parent,
                             id=wx.NewId(),
                             name=name,
                             style=wx.LC_REPORT | wx.SUNKEN_BORDER)
        self.SetFont(GetMonospaceFont(canvas.GetSettings().layout_font_size))

        # used for coloring
        self.__canvas = canvas
        # list of dictionary of properties
        self.__transactions: List[TransactionListEntry] = []
        # properties to show.
        self.__properties: Tuple[str, ...] = ('start',  # must have at least 1
                                              'location',
                                              'annotation')
        # insertion point for elements at end
        self.__current_new_idx = 0
        self.__colorize = True
        self.RefreshAll()

    def GetProperties(self) -> Tuple[str, ...]:
        return tuple(self.__properties)

    def SetProperties(self, properties: Tuple[str, ...]) -> None:
        self.__properties = properties
        self.RefreshAll()

    def Clear(self) -> None:
        self.__transactions = []

    def Colorize(self, colorize: bool) -> None:
        self.__colorize = colorize

    # Destroy all graphical elements and recreate them
    def RefreshAll(self) -> None:
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

    def RefreshTransaction(self, index: int) -> None:
        transaction = self.__transactions[index]
        for col_idx, prop in enumerate(self.__properties):
            self.SetItem(index, col_idx, str(transaction.get(prop)))
        annotation = transaction.get('annotation')
        if self.__colorize and annotation:
            color = self.__canvas.GetAutocolorColor(annotation)
        else:
            color = wx.Colour(255, 255, 255)
        self.SetItemBackgroundColour(index, color)

    def GetTransaction(self, index: int) -> TransactionListEntry:
        return self.__transactions[index]

    def __AddGraphicalTransaction(self,
                                  transaction: TransactionListEntry) -> None:
        self.InsertItem(self.__current_new_idx,
                        str(transaction.get(self.__properties[0])))
        self.RefreshTransaction(self.__current_new_idx)
        self.__current_new_idx += 1

    # Add a new element to bottom of list
    # New item must be dictionary of properties
    def Add(self, transaction_properties: TransactionListEntry) -> int:
        self.__transactions.append(transaction_properties)
        self.__AddGraphicalTransaction(transaction_properties)
        return self.__current_new_idx - 1

    def Remove(self, index: int) -> None:
        del self.__transactions[index]
        self.DeleteItem(index)
        self.__current_new_idx -= 1

    # Attempts to resize the columns based on size
    #  @pre All content should be added before this is called (once)
    def FitColumns(self) -> None:
        for idx, _ in enumerate(self.__properties):
            self.SetColumnWidth(idx, wx.LIST_AUTOSIZE)
        self.Layout()
