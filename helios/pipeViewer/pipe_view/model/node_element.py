
from .element import Element
from . import element_propsvalid as valid

import wx
import math

## An element that acts as a graph node
class NodeElement(Element):
    _NODE_PROPERTIES = {'name'            : ('', valid.validateString),
                        'connections_in'  : ([], valid.validateList),
                        'connections_out' : ([], valid.validateList),
                        'data'            : ('', valid.validateString),
                        'halfaddr'        : ('', valid.validateString),
                        'show_meta_links' : (False, valid.validateBool),}
    _ALL_PROPERTIES = Element._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_NODE_PROPERTIES)
    
    # Name of the property to use as a metadata key
    _METADATA_KEY_PROPERTY = 'name'
    
    # Additional metadata properties that should be associated with this element type
    _AUX_METADATA_PROPERTIES = list(Element._AUX_METADATA_PROPERTIES)
    _AUX_METADATA_PROPERTIES.append('halfaddr')

    @staticmethod
    def GetType():
        return 'node'

    ## returns a list of property names (keys) to exclude from
    # element property dialog
    @staticmethod
    def GetReadOnlyProperties():
        return ['halfaddr']

    @staticmethod
    def IsDrawable():
        return True

    @staticmethod
    def UsesMetadata():
        return True

    @staticmethod
    def GetDrawRoutine():
        return NodeElement.DrawRoutine

    def __init__(self, *args, **kwargs):
        Element.__init__(self, *args, **kwargs)
        # doublely linked
        self.__connections_in = []
        self.__connections_out = []
        self.__ComputeHA(self._properties['name'])

    def SetProperty(self, key, val):
        Element.SetProperty(self, key, val)
        if key == 'name':
            self.__ComputeHA(val)

    def BuildConnectionsFromProperty(self, connections_dict):
        for name in self.GetProperty('connections_in'):
            self.__connections_in.append(connections_dict[name])
        for name in self.GetProperty('connections_out'):
            self.__connections_out.append(connections_dict[name])

    ## add nodes that points to this node
    def AddConnectionOut(self, node_element):
        self.__connections_out.append(node_element)
        name = node_element.GetProperty('name')
        l = self.GetProperty('connections_out')
        if not l:
            self.SetProperty('connections_out', [name])
        else:
            l.append(name)

    ## add nodes that this node points to
    def AddConnectionIn(self, node_element):
        self.__connections_in.append(node_element)
        name = node_element.GetProperty('name')
        l = self.GetProperty('connections_in')
        if not l:
            self.SetProperty('connections_in', [name])
        else:
            l.append(name)

    ## Returns a bounding box around this shape
    # x1, y1, x2, y2
    def GetBounds(self):
        min_x, min_y = self.GetProperty('position')
        max_x, max_y = self.GetProperty('dimensions')
        max_x+=min_x
        max_y+=min_y

        # connections_out
        for child in self.__connections_out:
            child_x, child_y = child.GetProperty('position')
            if child_x < min_x:
                min_x = child_x
            elif child_x > max_x:
                max_x = child_x
            if child_y < min_y:
                min_y = child_y
            elif child_y > max_y:
                max_y = child_y

        # connections_in
        for child in self.__connections_in:
            child_x, child_y = child.GetProperty('position')
            if child_x < min_x:
                min_x = child_x
            elif child_x > max_x:
                max_x = child_x
            if child_y < min_y:
                min_y = child_y
            elif child_y > max_y:
                max_y = child_y

        return min_x, min_y, max_x, max_y

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    tick):
        (c_x,c_y),(c_w,c_h) = self.GetProperty('position'),self.GetProperty('dimensions')
        xoff, yoff = canvas.GetRenderOffsets()
        (c_x,c_y) = (c_x-xoff, c_y-yoff)

        # attempt to set color based on meta values
        meta_entry = pair.GetMetaEntries()
        if meta_entry and meta_entry.get('color'):
            color = meta_entry.get('color')
            # set pen to meta-derived color
            pen_color = max(color[0]*0.7, 0), max(color[1]*0.7, 0), max(color[2]*0.7, 0)
        else:
            pen_color = (0, 0, 0)
            color = (255, 255, 255)

        # Override pen color
        if meta_entry and 'ubtb_current' in meta_entry:
            border_color = (0, 255, 0)
            border_weight = 3
        elif meta_entry and 'ubtb_next' in meta_entry:
            border_color = (255, 0, 0)
            border_weight = 3
        elif meta_entry and 'border' in meta_entry:
            tmp = meta_entry.get('border')
            border_color = tmp[:3]
            border_weight = tmp[3]
        else:
            border_color = self.GetProperty('color')
            border_weight = 1

        dc.SetPen(wx.ThePenList.FindOrCreatePen((pen_color), 1, wx.SOLID))

        for connection in self.__connections_out:
            (con_x,con_y),(con_w,con_h) = connection.GetProperty('position'),connection.GetProperty('dimensions')
            (con_x,con_y) = (con_x-xoff, con_y-yoff)
            # draw line or arc
            if connection == self:
                # set brush transparent so arc is not filled in
                dc.SetBrush(wx.TheBrushList.FindOrCreateBrush((255, 255, 255), wx.TRANSPARENT))
                dc.DrawArc(c_x+40, c_y, c_x, c_y, c_x+20, c_y)
                start_x = c_x
                start_y = c_y
                end_x = con_x
                end_y = con_y
            else:
                start_x,start_y,end_x,end_y = self.__computeLine(c_x, c_y, c_w, c_h, con_x, con_y, con_w, con_h)
                dc.DrawLine(start_x, start_y, end_x, end_y)

            if canvas.GetScale() > 0.5:
                dy = end_y-start_y
                dx = end_x-start_x
                # draw arrowhead
                self.__drawArrowhead(dc, dx, dy, end_x, end_y)

        name = self.GetProperty('name')

        # Hack to show actual graph links overlayed on the graph. This is not perfect because link
        # addrs can be aliased and must match up with the node names by a guess at actual addr from
        # the given half-addr
        show_ubtb_links = self.GetProperty('show_meta_links')
        if show_ubtb_links and meta_entry:
            ARROW_SPACING = 4 # Spacing between each parallel arrow drawn
            tlinks = meta_entry.get('ubtb_tlinks', None)
            nlinks = meta_entry.get('ubtb_nlinks', None)
            graph_nodes = self.GetLayout().GetGraphNodes()
            if tlinks:
                for tlink in tlinks:
                    tln = graph_nodes.get('{:4x}'.format(tlink))
                    if tln is None:
                        tln = graph_nodes.get('{:4x}'.format(tlink-2))
                    if tln is not None and tln is not self:
                        dc.SetPen(wx.ThePenList.FindOrCreatePen((255,100,100), 1, wx.SOLID))
                        (con_x,con_y),(con_w,con_h) = tln.GetProperty('position'),tln.GetProperty('dimensions')
                        (con_x,con_y) = (con_x-xoff, con_y-yoff)
                        dx,dy = con_x-c_x,con_y-c_y
                        mag = max(0.00001,(dx*dx + dy*dy)**0.5)
                        dxu,dyu = dx/mag,dy/mag
                        xadj = -dyu * ARROW_SPACING
                        yadj = dxu * ARROW_SPACING
                        start_x,start_y,end_x,end_y = self.__computeLine(c_x, c_y, c_w, c_h, con_x, con_y, con_w, con_h, xadj, yadj)
                        dc.DrawLine(start_x, start_y, end_x, end_y)
                        dx,dy = end_x-start_x, end_y-start_y
                        self.__drawArrowhead(dc, dx, dy, end_x, end_y)

            if nlinks:
                for nlink in nlinks:
                    nln = graph_nodes.get('{:x}'.format(nlink))
                    if nln is None and nln is not self:
                        nln = graph_nodes.get('{:x}'.format(nlink-2))
                    if nln is not None and nln is not self:

                        # If two nodes point to eachother, the N link of the node needs to be spaced out
                        # so they don't overlap
                        if meta_entry.get('ubtb_tlink_inc') is True:
                            adj = ARROW_SPACING * 2
                        else:
                            adj = ARROW_SPACING

                        dc.SetPen(wx.ThePenList.FindOrCreatePen((100,100,255), 1, wx.SOLID))
                        (con_x,con_y),(con_w,con_h) = nln.GetProperty('position'),nln.GetProperty('dimensions')
                        (con_x,con_y) = (con_x-xoff, con_y-yoff)
                        dx,dy = con_x-c_x,con_y-c_y
                        mag = max(0.00001,(dx*dx + dy*dy)**0.5)
                        dxu,dyu = dx/mag,dy/mag
                        xadj = dyu * adj
                        yadj = -dxu * adj
                        start_x,start_y,end_x,end_y = self.__computeLine(c_x, c_y, c_w, c_h, con_x, con_y, con_w, con_h, xadj, yadj)
                        dc.DrawLine(start_x, start_y, end_x, end_y)
                        dx,dy = end_x-start_x, end_y-start_y
                        self.__drawArrowhead(dc, dx, dy, end_x, end_y)

        # set pen to property-derived color
        pen = wx.ThePenList.FindOrCreatePen(border_color, border_weight, wx.SOLID)
        dc.SetPen(pen)

        SEED_OFFSETS = (-4,-4,8+10,8)
        # If this is a graph seed, draw some extra information
        if meta_entry and meta_entry.get('is_seed', False) is True:
            # Light blue-background box surrounding seed node
            dc.SetBrush(wx.TheBrushList.FindOrCreateBrush((190, 190, 255), wx.wx.SOLID))
            dc.DrawRectangle(c_x+SEED_OFFSETS[0], c_y+SEED_OFFSETS[1], c_w++SEED_OFFSETS[2], c_h+SEED_OFFSETS[3])
            #dc.DrawRectangle(c_x+c_w, c_y, 8, c_h)
            dc.DrawText("s", c_x+c_w+2, c_y)

        if meta_entry and meta_entry.get('ubtb_camhit', False):
            # Blue doted box around SEED
            pen = wx.ThePenList.FindOrCreatePen((0,0,255), 2, wx.DOT)
            pen.SetDashes([0, 0, 0, 1])
            prevpen = dc.GetPen()
            dc.SetPen(pen)
            brush = wx.TheBrushList.FindOrCreateBrush((0,0,0), wx.TRANSPARENT)
            dc.SetBrush(brush)
            dc.DrawRectangle(c_x+SEED_OFFSETS[0], c_y+SEED_OFFSETS[1], c_w++SEED_OFFSETS[2], c_h+SEED_OFFSETS[3])
            dc.SetPen(prevpen)

        if meta_entry:
            uras_infos = meta_entry.get('ubtb_ras', None)
            if uras_infos:
                dc.SetBrush(wx.TheBrushList.FindOrCreateBrush((255, 190, 190), wx.wx.SOLID))

                ## Start above box. Draw ellipses with target addresses stacked on top of each other
                ## moving down the canvas. For density, each overlaps with <offset> pixels of each
                ## covered ellipse visible above the next
                #width = 55
                #height = c_h+4
                #offset = 4
                #y = c_y - 2 - (offset * (len(uras_infos)-1))
                #for entry_info in uras_infos:
                #    pc,gi,tgt = entry_info
                #    dc.DrawEllipse(c_x - width - 2, y, width, height)
                #    #content = 'ras'
                #    content = tgt
                #    dc.DrawText(content, c_x-width+1, y + 2)
                #    y += offset

                # Draw single ellipse with number of entries for this CALL
                width = 20
                height = c_h+2
                dc.DrawEllipse(c_x - width - 2, c_y-1, width, height)
                content = str(len(uras_infos))
                dc.DrawText(content, c_x-width+2, c_y)

        # set brush to meta-derived color
        data = self.GetProperty('data')
        if 'ANT ' in data: # Always Never Taken
            brush = wx.TheBrushList.FindOrCreateBrush((200,200,200), wx.CROSSDIAG_HATCH)
        else:
            brush = wx.TheBrushList.FindOrCreateBrush(color, wx.SOLID)
        dc.SetBrush(brush)
        dc.DrawRectangle(c_x, c_y, c_w, c_h)

        graph_index = meta_entry.get('ubtb_gi', None) if meta_entry else None
        if canvas.GetScale() > 0.5:
            s = name
            if graph_index is not None:
                s += ' g:' + graph_index
            dc.DrawText(s, c_x, c_y)
        elif graph_index is not None:
            dc.DrawText('g:' + graph_index, c_x, c_y)


        # Node is current AND next. Draw next as dotted box
        if meta_entry and 'ubtb_next' in meta_entry and 'ubtb_current' in meta_entry:
            pen = wx.ThePenList.FindOrCreatePen((255,0,0), 2, wx.DOT)
            pen.SetDashes([0, 0, 0, 1])
            dc.SetPen(pen)
            brush = wx.TheBrushList.FindOrCreateBrush((0,0,0), wx.TRANSPARENT)
            dc.SetBrush(brush)
            dc.DrawRectangle(c_x-2, c_y-2, c_w+4, c_h+4)

        ## debug drawing of bounds
        #dc.SetPen(wx.Pen((255, 0, 0), 1))
        #dc.SetBrush(wx.Brush((255, 255, 255), style=wx.TRANSPARENT))
        #bounds = self.GetBounds()
        #dc.DrawRectangle(bounds[0]-xoff, bounds[1]-yoff, bounds[2]-bounds[0], bounds[3]-bounds[1])
        self.UnsetNeedsRedraw()

    def __ComputeHA(self, name):
        try:
            addr = int(name.strip(), 16)
        except:
            self._properties['halfaddr'] = None
        else:
            # Look for '32' in the second line of the data to determine if this
            # is 32-bit instruction. If so, compute the half-address as addr+2
            data = self._properties['data']
            line2_pos = data.find('\n')
            if line2_pos == -1:
                line2 = data
            else:
                line2 = data[line2_pos]
            if '32' in line2:
                self._properties['halfaddr'] = '{:x}'.format(addr+2)
            else:
                self._properties['halfaddr'] = '{:x}'.format(addr)

    def __drawArrowhead(self, dc, dx, dy, x, y):
        theta = math.atan2(dy, dx) - 3.14
        ARROWHEAD_ANG = 0.25
        dc.DrawLine(x+math.cos(theta-ARROWHEAD_ANG)*15, y + math.sin(theta-ARROWHEAD_ANG)*15, x, y)
        dc.DrawLine(x+math.cos(theta+ARROWHEAD_ANG)*15, y + math.sin(theta+ARROWHEAD_ANG)*15, x, y)

    ## Computes the endpoints of a line between two nodes attempting to run the
    #  line from center-to-center. The start point may be within the start node
    #  @return start_x,start_y,end_x,end_y
    @staticmethod
    def __computeLine(c_x, c_y, c_w, c_h, con_x, con_y, con_w, con_h, xadj=0, yadj=0):
        start_x = c_x + c_w/2.0 + xadj
        start_y = c_y + c_h/2.0 + yadj
        if con_w == 0 or con_h == 0:
            # Use con_x/y as they are. No need to caculate angles
            pass
        else:
            # Attempt to point arrow to the middle

            # Middle of connected node
            cm_x = con_x + con_w/2.0 + xadj
            cm_y = con_y + con_h/2.0 + yadj

            # Slope of line to middle
            dx = cm_x - start_x
            dy = cm_y - start_y

            # Edge cases for slope
            if dx == 0:
                con_x = cm_x
            if dy == 0:
                con_y = cm_y

            # Determine which connection node's edge the slope intersects with
            if dx != 0 or dy != 0:
                if dx == 0:
                    # Vertical line
                    if con_y > start_y:
                        con_y = con_y # Hits upper edge
                    else:
                        con_y = con_y + con_h # Hits lower edge
                elif dy == 0:
                    # Horizontal line
                    if con_x > start_x:
                        con_x = con_x # Hits left edge
                    else:
                        con_x = con_x + con_w # Hits right edge
                else:
                    # Diagonal line
                    slope = dy/float(dx)
                    con_ratio = con_h/float(con_w)
                    if start_x < cm_x:
                        # Line from left of middle
                        if slope > con_ratio:
                            # Bottom edge (line steeper than box ratio)
                            if start_y > con_y:
                                con_y = con_y + con_h
                            con_x = cm_x - ((con_h/2.0) / slope) # Adjust x from middle by half-height * (1/slope)
                        elif slope < -con_ratio:
                            # Top edge (line steeper than box ratio)
                            if start_y > con_y:
                                con_y = con_y + con_h
                            con_x = cm_x + ((con_h/2.0) / slope) # Adjust x from middle by half-height * (1/slope)
                        else:
                            # Left Side
                            con_x = con_x
                            con_y = cm_y - ((con_w/2.0) * slope) # Adjust y from middle by half-width * slope
                    else:
                        # From right of middle
                        if slope > con_ratio:
                            # Bottom edge (line steeper than box ratio)
                            if start_y > con_y:
                                con_y = con_y + con_h
                            con_x = cm_x + ((con_h/2.0) / slope) # Adjust x from middle by half-height * (1/slope)
                        elif slope < -con_ratio:
                            # Top edge (line steeper than box ratio)
                            if start_y > con_y:
                                con_y = con_y + con_h
                            con_x = cm_x - ((con_h/2.0) / slope) # Adjust x from middle by half-height * (1/slope)
                        else:
                            # Right Side
                            con_x = con_x + con_w
                            con_y = cm_y + ((con_w/2.0) * slope) # Adjust y from middle by half-width * slope

        return start_x,start_y,con_x,con_y

NodeElement._ALL_PROPERTIES['type'] = (NodeElement.GetType(), valid.validateString)

