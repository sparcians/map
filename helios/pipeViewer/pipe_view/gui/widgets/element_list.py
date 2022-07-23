import wx
from gui.font_utils import GetMonospaceFont

## This class is a GUI list control element that shows elements and allows selection of them.
class ElementList(wx.ListCtrl):
    def __init__(self, parent, id, canvas, name='', style=wx.LC_REPORT|wx.SUNKEN_BORDER, properties=['element']):
        wx.ListCtrl.__init__(self, parent=parent, id=wx.NewId(), name=name, style=style)
        self.SetFont(GetMonospaceFont(canvas.GetSettings().layout_font_size))

        # used for coloring
        self.__canvas = canvas
        # list of dictionary of properties
        self.__elements = []
        # list of element pointers
        self.__element_ptr = []
        # properties to show.
        self.__properties = properties[:] #must have at least 1
        #insertion point for elements at end
        self.__current_new_idx = 0
        self.RefreshAll()

    def GetProperties(self):
        return tuple(self.__properties)

    def SetProperties(self, properties):
        self.__properties = properties[:]
        self.RefreshAll()

    def Clear(self):
        self.__elements = []
        self.__element_ptrs = []

    ## Destroy all graphical elements and recreate them
    def RefreshAll(self):
        self.ClearAll()
        # make header
        width =  int(self.GetClientSize()[0]/len(self.__properties))
        if width < 100:
            width = 100
        for col_idx, column in enumerate(self.__properties):
            self.InsertColumn(col_idx, column)
            self.SetColumnWidth(col_idx, width)
        self.__current_new_idx = 0
        for el in self.__elements:
            self.__AddGraphicalElement(el)

    def RefreshElement(self, index):
        element = self.__elements[index]
        for col_idx, prop in enumerate(self.__properties):
            self.SetItem(index, col_idx, str(element.get(prop)))
        color = (255, 255, 255)
        self.SetItemBackgroundColour(index, color)

    def GetElement(self, index):
        return self.__element_ptrs[index]

    def __AddGraphicalElement(self, element):
        self.InsertItem(self.__current_new_idx, str(element.get(self.__properties[0])))
        self.RefreshElement(self.__current_new_idx)
        self.__current_new_idx+=1

    ## Add a new element to bottom of list. New item must be dictionary of properties.
    def Add(self, element_ptr, element_properties):
        self.__element_ptrs.append(element_ptr)
        self.__elements.append(element_properties)
        self.__AddGraphicalElement(element_properties)
        return self.__current_new_idx-1

    def Remove(self, index):
        del self.__element_ptrs[index]
        del self.__elements[index]
        self.DeleteItem(index)
        self.__current_new_idx-=1

    ## Attempts to resize the columns based on size
    #  @pre All content should be added before this is called (once)
    def FitColumns(self):
        for idx,_ in enumerate(self.__properties):
            self.SetColumnWidth(idx, wx.LIST_AUTOSIZE)
        self.Layout()
