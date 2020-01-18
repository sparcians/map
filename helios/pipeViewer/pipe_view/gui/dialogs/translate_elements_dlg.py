

import sys
import wx
from functools import partial
from gui.widgets.element_list import ElementList


# # TranslateElementsDlg allows relative or absolute translation of a group of
#  elements based on user-specified coordinates. This is exepected to be used
#  modally
class TranslateElementsDlg(wx.Dialog):

    def __init__(self, parent):
        self.__layout_frame = parent
        self.__context = parent.GetContext()
        self.__sel_mgr = parent.GetCanvas().GetSelectionManager()

        # initialize graphical part
        wx.Dialog.__init__(self, parent,
                          -1,
                          'Translate Elements - {}'.format(self.__layout_frame.ComputeTitle()),
                          size = (-1, -1),
                          style = wx.MAXIMIZE_BOX | wx.RESIZE_BORDER | wx.CAPTION | wx.CLOSE_BOX)

        self.__fnt_numbers = wx.Font(8, wx.FONTFAMILY_MODERN, wx.NORMAL, wx.NORMAL, face = 'Monospace')

        panel = wx.Panel(self)

        lbl_info = wx.StaticText(panel, -1, '(Press ENTER to appy changes, ESC to exit)')
        lbl_info.SetFont(self.__fnt_numbers)
        lbl_info.SetForegroundColour((120, 120, 120))

        lbl_heading = wx.StaticText(panel, -1, '{} selected elements'
                                              .format(len(self.__sel_mgr.GetSelection())))

        box_dims = (60, 40)
        panel_box = wx.Panel(panel, size = box_dims, style = wx.BORDER_SIMPLE)
        panel_box.SetMinSize(box_dims)
        self.__lbl_sel_lt = wx.StaticText(panel, -1, '')
        self.__lbl_sel_lt.SetFont(self.__fnt_numbers)
        self.__lbl_sel_wh = wx.StaticText(panel_box, -1, '') # Inside box
        self.__lbl_sel_wh.SetFont(self.__fnt_numbers)
        self.__lbl_sel_rb = wx.StaticText(panel, -1, '')
        self.__lbl_sel_rb.SetFont(self.__fnt_numbers)

        self.__UpdateDesc()

        lbl_x = wx.StaticText(panel, -1, "X: ", size = (20, 20))
        self.__txt_x = wx.TextCtrl(panel, -1, "0", size = (70, 20))
        self.__chk_rel_x = wx.CheckBox(panel, -1, label = 'relative')
        self.__chk_rel_x.SetValue(True)

        lbl_y = wx.StaticText(panel, -1, "Y: ", size = (20, 20))
        self.__txt_y = wx.TextCtrl(panel, -1, "0", size = (70, 20))
        self.__chk_rel_y = wx.CheckBox(panel, -1, label = 'relative')
        self.__chk_rel_y.SetValue(True)

        self.__btn_exit = wx.Button(panel, -1, 'Close')
        self.__btn_apply = wx.Button(panel, -1, 'Apply')

        panel_box_sizer = wx.BoxSizer(wx.VERTICAL)
        panel_box_sizer.Add((1, 1), 1, wx.EXPAND)
        panel_box_sizer.Add(self.__lbl_sel_wh, 0, wx.ALIGN_CENTER_VERTICAL | wx.ALIGN_CENTER_HORIZONTAL)
        panel_box_sizer.Add((1, 1), 1, wx.EXPAND)
        panel_box.SetSizer(panel_box_sizer)

        position_sizer = wx.BoxSizer(wx.HORIZONTAL)
        position_sizer.Add(self.__lbl_sel_lt, 0, wx.ALIGN_TOP | wx.ALIGN_RIGHT | wx.RIGHT, 4)
        position_sizer.Add(panel_box, 1, wx.EXPAND | wx.TOP | wx.BOTTOM, 10)
        position_sizer.Add(self.__lbl_sel_rb, 0, wx.ALIGN_BOTTOM | wx.ALIGN_RIGHT | wx.LEFT, 4)

        sizer_x = wx.BoxSizer(wx.HORIZONTAL)
        sizer_x.Add(lbl_x, 0, wx.ALL, 4)
        sizer_x.Add(self.__txt_x, 0, wx.ALL, 4)
        sizer_x.Add(self.__chk_rel_x, 0, wx.ALL, 4)

        sizer_y = wx.BoxSizer(wx.HORIZONTAL)
        sizer_y.Add(lbl_y, 0, wx.ALL, 4)
        sizer_y.Add(self.__txt_y, 0, wx.ALL, 4)
        sizer_y.Add(self.__chk_rel_y, 0, wx.ALL, 4)

        self.__sizer_buttons = wx.BoxSizer(wx.HORIZONTAL)
        self.__sizer_buttons.Add(self.__btn_exit, 0, 0, 4)
        self.__sizer_buttons.Add((1, 1), 1, wx.EXPAND)
        self.__sizer_buttons.Add(self.__btn_apply, 0, 0, 4)

        main_sizer = wx.BoxSizer(wx.VERTICAL)
        main_sizer.Add(lbl_heading, 0, wx.ALIGN_CENTER_HORIZONTAL)
        # main_sizer.Add((1,6), 0)
        main_sizer.Add(position_sizer, 0, wx.EXPAND | wx.ALL, 10) # Indent
        main_sizer.Add(wx.StaticLine(panel), 0, wx.EXPAND)
        main_sizer.Add((1, 4), 0)
        main_sizer.Add(sizer_x, 0, wx.ALIGN_LEFT)
        main_sizer.Add(sizer_y, 0, wx.ALIGN_LEFT)
        main_sizer.Add((1, 4), 0, wx.EXPAND)
        main_sizer.Add(wx.StaticLine(panel), 0, wx.EXPAND)
        main_sizer.Add((1, 4), 0)
        main_sizer.Add(lbl_info, 0, wx.ALIGN_CENTER)
        main_sizer.Add((1, 18), 0)
        main_sizer.Add(self.__sizer_buttons, 0, wx.EXPAND)

        panel.SetSizer(main_sizer)

        padding_sizer = wx.BoxSizer(wx.VERTICAL)
        padding_sizer.Add(panel, 1, wx.EXPAND | wx.ALL, 10)
        self.SetSizer(padding_sizer)

        self.Fit()

        self.__txt_x.SetFocus()

        # TODO: bind escape to exit!

        # Could show currently selected elements for excluding some from the xlate operation
        # #self.__results_box = ElementList(panel,
        # #            wx.NewId(),
        # #            parent.GetCanvas(),
        # #            name='listbox',
        # #            properties=[''])
        # #main_sizer.Add(self.__results_box, 1, wx.EXPAND, 5)

        # bind to events
        self.__btn_apply.Bind(wx.EVT_BUTTON, self.OnApply)
        self.__btn_exit.Bind(wx.EVT_BUTTON, self.OnExit)
        panel.Bind(wx.EVT_KEY_UP, self.OnKeyPress)
        # #panel.Bind(wx.EVT_KEY_DOWN, self.OnKeyPress)
        # #panel.Bind(wx.EVT_CHAR, self.OnKeyPress)
        self.__txt_x.Bind(wx.EVT_TEXT, self.OnCoordChange)
        self.__txt_y.Bind(wx.EVT_TEXT, self.OnCoordChange)
        self.__chk_rel_x.Bind(wx.EVT_CHECKBOX, self.OnCoordChange)
        self.__chk_rel_y.Bind(wx.EVT_CHECKBOX, self.OnCoordChange)

    def Show(self):
        raise Exception('This dialog should only be shown modally')

    # # defines how the dialog should pop up
    def ShowModal(self):
        wx.Dialog.ShowModal(self)
        self.Raise()

    # # callback that listens for enter being pressed to initiate search
    def OnKeyPress(self, evt):
        if evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.EndModal(0)
            self.__layout_frame.SetFocus()
        elif evt.GetKeyCode() == wx.WXK_RETURN:
            self.OnApply()
        elif (evt.GetKeyCode() == ord('Z')) and evt.ControlDown() and not evt.ShiftDown() and not evt.AltDown():
            # Catch undo hotkeys for this control
            self.__sel_mgr.Undo()
        elif (evt.GetKeyCode() == ord('Y')) and evt.ControlDown() and not evt.ShiftDown() and not evt.AltDown():
            # Catch redo hotkeys for this control
            self.__sel_mgr.Redo()
        else:
            evt.Skip()

        # In case undo/redo made changes:
        self.__UpdateDesc()

    # # Handle x/y textbox/checkbox change
    def OnCoordChange(self, evt = None):
        dx, dy = self.__ComputeMoveDeltas()

        WHITE = (255, 255, 255)
        PINK = (255, 100, 100)
        self.__txt_x.SetBackgroundColour(PINK if dx is None else WHITE)
        self.__txt_y.SetBackgroundColour(PINK if dy is None else WHITE)

        self.__btn_apply.Enable(dx is not None and dy is not None)

        # Handle enter key for applying
        if evt and hasattr(evt, 'GetKeyCode') and evt.GetKeyCode() == wx.WXK_ENTER \
           and dx is not None and dy is not None:
            self.OnApply()

        if evt is not None:
            evt.Skip()

    def OnExit(self, evt):
        self.EndModal(0)

    # # Move elements as described
    def OnApply(self, evt = None):
        # Move elements to absolute or relative position. Always use the delta
        # argument to Selection_Mgr.Move() because it may compute its absolute
        # position differently than this dialog (i.e. center of selection rather
        # than top-left corner)

        dx, dy = self.__ComputeMoveDeltas()
        # #print 'Move Deltas: ', dx, ',', dy

        # Apply should be ignored for invalid textbox content. Apply button will
        # be disabled and textboxes will be red
        if dx is None or dy is None:
            return

        self.__sel_mgr.BeginCheckpoint('Translate {}({}), {}({})' \
                                       .format(dx, 'rel' if self.__chk_rel_x.GetValue() else 'abs', \
                                               dy, 'rel' if self.__chk_rel_y.GetValue() else 'abs'))
        try:
            self.__sel_mgr.Move(delta = (dx, dy), force_no_snap = True)
        finally:
            self.__sel_mgr.CommitCheckpoint()

        self.__layout_frame.Refresh()

        self.__UpdateDesc() # Update label with new coordinates

    # # Compute Move deltas based on current textbox/checkbox values
    #  @return (dx,dy) indicating arguments to Selection_Mgr.Move. Either value
    #  in this tuple may be None if value data cannot be extracted form the
    #  dialog widgets
    def __ComputeMoveDeltas(self):
        try:
            xstr = self.__txt_x.GetValue()
            # For relative moves, value can be empty to mean no move
            if self.__chk_rel_x.GetValue() and xstr == '':
                x = 0
            else:
                x = int(xstr) # For non-relative motion
        except:
            x = None

        try:
            # For relative moves, value can be empty to mean no move
            ystr = self.__txt_y.GetValue()
            if self.__chk_rel_y.GetValue() and ystr == '':
                y = 0
            else:
                y = int(ystr)
        except:
            y = None

        if x is None or y is None:
            return (None, None)

        bbox = self.__sel_mgr.GetBoundingBox() # l,t,r,b
        if bbox is None:
            return (None, None)

        l, t, r, b = bbox

        # Compute dx based on relative or absolute movement
        if self.__chk_rel_x.GetValue():
            dx = x
        else:
            dx = x - l

        if self.__chk_rel_y.GetValue():
            dy = y
        else:
            dy = y - t

        return (dx, dy)

    def __UpdateDesc(self):
        bbox = self.__sel_mgr.GetBoundingBox() # l,t,r,b
        if bbox is not None:
            l, t, r, b = bbox
            bbox_lt = 'x:{:>6}\ny:{:>6}' \
                      .format(l, t)
            bbox_wh = 'w:{:>6}\nh:{:>6}' \
                      .format(r - l, b - t)
            bbox_rb = 'r:{:>6}\nb:{:>6}' \
                      .format(r, b)
        else:
            bbox_lt = 'N/A'
            bbox_wh = 'N/A'
            bbox_rb = 'N/A'

        self.__lbl_sel_lt.SetLabel(bbox_lt)
        self.__lbl_sel_wh.SetLabel(bbox_wh)
        self.__lbl_sel_rb.SetLabel(bbox_rb)

