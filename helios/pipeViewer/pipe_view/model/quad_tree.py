

class QuadNode:
    '''
    Node used by QuadTree
    '''

    def __init__(self, bounds, contents, elements, parent = None, parent_index = None):
        self.bounds = bounds
        self.contents = contents # list of QuadNodes
        self.elements = elements
        self.parent_index = parent_index
        self.parent = parent

    def __CalculateChildNodeBounds(self):
        min_x, min_y, max_x, max_y = self.bounds
        midpoint_x = (max_x + min_x) / 2.0
        midpoint_y = (max_y + min_y) / 2.0
        return (min_x, min_y, max_x, max_y, midpoint_x, midpoint_y)

    def BisectCreateNodes(self):
        if not self.contents:
            min_x, min_y, max_x, max_y, midpoint_x, midpoint_y = self.__CalculateChildNodeBounds()
            self.contents = [QuadNode((min_x, min_y, midpoint_x, midpoint_y), [], [], self, 0),
                             QuadNode((midpoint_x, min_y, max_x, midpoint_y), [], [], self, 1),
                             QuadNode((min_x, midpoint_y, midpoint_x, max_y), [], [], self, 2),
                             QuadNode((midpoint_x, midpoint_y, max_x, max_y), [], [], self, 3)]

    def UpdateBounds(self, new_bounds):
        self.bounds = new_bounds
        if self.contents:
            min_x, min_y, max_x, max_y, midpoint_x, midpoint_y = self.__CalculateChildNodeBounds()
            self.contents[0].UpdateBounds((min_x, min_y, midpoint_x, midpoint_y))
            self.contents[1].UpdateBounds((midpoint_x, min_y, max_x, midpoint_y))
            self.contents[2].UpdateBounds((min_x, midpoint_y, midpoint_x, max_y))
            self.contents[3].UpdateBounds((midpoint_x, midpoint_y, max_x, max_y))

    def SetVisibilityTickOnAllChildElements(self, vis_tick):
        if self.elements: # leaves only
            for x in self.elements:
                x.SetVisibilityTick(vis_tick)
        else:
            for cnode in self.contents:
                cnode.SetVisibilityTickOnAllChildElements(vis_tick)

    def GetAllChildElements(self): # contains duplicates
        l = []
        self.__GetAllChildren(self, l)
        return l

    def __GetAllChildren(self, node, output_list):
        if node.elements and not node.contents: # leaves only
            output_list.extend(node.elements)
        else:
            for cnode in node.contents:
                self.__GetAllChildren(cnode, output_list)

    def GetAllBoxes(self, node = None):
        if node == None:
            node = self
        if not node.contents:
            bounds = [node.bounds]
        else:
            bounds = []
        for content in node.contents:
            bounds.extend(self.GetAllBoxes(content))
        return bounds

    # # Debug printout for this node and nodes below it.
    def PrintRecursive(self, node = None, level = 0):
        if node == None:
            node = self
        space = '      ' * level
        print('%sNode %s:' % (space, node))
        print('%s  Bounds: %s' % (space, node.bounds))
        print('%s  Elements: %s' % (space, str(node.elements)))
        print('%s  Contents:' % (space))
        for content in node.contents:
            self.PrintRecursive(content, level + 1)


# # A small QuadTree-based container for pairs.
class QuadTree:
    # when more than this number of objects are dirty, the bsp rebuilds
    FULL_REBUILD_THRESHOLD = 30
    WIDTH_LIMIT = 100

    def __init__(self):
        self.__objects = []
        self.__tree = None

        # Dictionary keyed by objects which stores
        # all nodes object is in
        # e.g. self.__lookup_table[myObj] = [QuadNode_instance1, QuadNode_instance2]
        self.__lookup_table = {}
        self.__dirty_objects = set()
        self.__removes = []
        self.Build()

    # # Add function.
    def AddObject(self, obj, push_update = False):
        self.__objects.append(obj)
        if push_update:
            self.AddSingleToTree(obj)
        else:
            self.__dirty_objects.add(obj)

    def AddObjects(self, objs):
        self.__objects.extend(objs)
        self.__dirty_objects.update(objs)

    def RemoveObject(self, obj):
        self.__dirty_objects.difference_update([obj])
        self.__objects.remove(obj)
        self.RemoveSingleFromTree(obj)

    def RemoveSingleFromTree(self, obj):
        lookup_entry = self.__lookup_table.get(obj)

        while lookup_entry:
            node_owner = lookup_entry.pop()
            node_owner.elements.remove(obj)

    def Update(self):
        if len(self.__dirty_objects) >= self.FULL_REBUILD_THRESHOLD:
            self.Build()
        else:
            for obj in self.__dirty_objects:
                self.AddSingleToTree(obj, no_remove = True)
        self.__dirty_objects.clear()

    def AddSingleToTree(self, obj, no_remove = False):
        lookup_entry = self.__lookup_table.get(obj)
        if lookup_entry:
            return # already there.
        if not no_remove:
            self.__dirty_objects.difference_update([obj])
        bounds = obj.GetElement().GetBounds()
        if bounds[0] < self.__tree.bounds[0] or \
            bounds[1] < self.__tree.bounds[1] or \
            bounds[2] > self.__tree.bounds[2] or \
            bounds[3] > self.__tree.bounds[3]:
            # our new object is outside bounds. Need to recalculate.
            self.Build(no_clear = True)
        else:
            # limited recalc
            self.Build(update = [obj]) # not a full rebuild

    def UpdateBounds(self):
        bounds = self.CalculateBounds()
        self.__tree.UpdateBounds(bounds)

    def CalculateBounds(self):
        lowest_x = 1000000
        highest_x = 0
        lowest_y = 1000000
        highest_y = 0
        for obj in self.__objects:
            bounds = obj.GetElement().GetBounds()
            if bounds[0] < lowest_x:
                lowest_x = bounds[0]
            elif bounds[2] > highest_x:
                highest_x = bounds[2]
            if bounds[1] < lowest_y:
                lowest_y = bounds[1]
            elif bounds[3] > highest_y:
                highest_y = bounds[3]
        return (lowest_x, lowest_y, highest_x, highest_y)

    # # places objects in tree buckets
    # pattern for indices used
    # 0 1
    # 2 3
    # if an update list is specified, then only objects in there are updated
    def Build(self, update = [], no_clear = False):
        if update:
            self.__tree.elements.extend(update)
        else:
            if not no_clear:
                self.__dirty_objects.clear() # cleans all
            min_x, min_y, max_x, max_y = self.CalculateBounds()
            self.__tree = QuadNode((min_x, min_y, max_x, max_y), contents = [], elements = self.__objects[:])
        current_nodes = [self.__tree]
        while current_nodes:
            # filter the objects down through tree
            node = current_nodes.pop(0)
            if node.elements and (node.bounds[2] - node.bounds[0]) / 2.0 >= self.WIDTH_LIMIT:
                # will only create if needed
                node.BisectCreateNodes()
                while node.elements:
                    obj = node.elements.pop()
                    # accessing variable rather than dict (e.g. obj.pos) saves very little time
                    bounds = obj.GetElement().GetBounds()
                    midpoint_x = node.contents[0].bounds[2]
                    midpoint_y = node.contents[0].bounds[3]
                    if bounds[0] <= midpoint_x:
                        if bounds[1] <= midpoint_y:
                            node.contents[0].elements.append(obj)
                        if bounds[3] > midpoint_y:
                            node.contents[2].elements.append(obj)
                    if bounds[2] > midpoint_x:
                        if bounds[1] <= midpoint_y:
                            node.contents[1].elements.append(obj)
                        if bounds[3] > midpoint_y:
                            node.contents[3].elements.append(obj)
                current_nodes.extend(node.contents)
            elif node.elements:
                # print route
                if not update:
                    # go from leaves to root and build quick index map (path to element)
                    # could probably be combined with generator
                    for el in node.elements:
                        table_entry = self.__lookup_table.get(el)
                        if not table_entry is None:
                            if not node in table_entry:
                                table_entry.append(node)
                            # else:
                            #    print 'overlap'
                        else:
                            self.__lookup_table[el] = [node]
                else:
                    for el in node.elements:
                        if el in update:
                            table_entry = self.__lookup_table.get(el)
                            if not table_entry is None:
                                if not node in table_entry:
                                    table_entry.append(node)
                                # else:
                                #    print 'overlap'
                            else:
                                self.__lookup_table[el] = [node]
        # self.__tree.PrintRecursive()
        # print self.__lookup_table
        return self.__tree

    def GetObjects(self, given_bounds):
        s = set()
        # if not built yet
        if not self.__tree:
            return s
        # recursive queryng of objects inside given_bounds
        self.__GetObjects(self.__tree, given_bounds, s)
        return s

    def __GetObjects(self, node, given_bounds, output_set): # left, upper, right, lower
        if not node.contents: # leaf
            output_set.update(node.elements)
        else:
            for i in (0, 1, 2, 3):
                cnode = node.contents[i]
                # in bounds
                x_intersects = (given_bounds[0] <= cnode.bounds[2]) and (given_bounds[2] >= cnode.bounds[0])
                y_intersects = (given_bounds[1] <= cnode.bounds[3]) and (given_bounds[3] >= cnode.bounds[1])
                if (x_intersects and y_intersects):
                    if (given_bounds[0] <= cnode.bounds[0] and \
                            given_bounds[1] <= cnode.bounds[1] and \
                            given_bounds[2] > cnode.bounds[2] and \
                            given_bounds[3] > cnode.bounds[3]):
                        # inside--take all objects
                        child_elements = cnode.GetAllChildElements()
                        output_set.update(child_elements)
                    else:
                        self.__GetObjects(cnode, given_bounds, output_set)

    def SetVisibilityTick(self, given_bounds, vis_tick):
        if self.__tree:
            self.__SetVisibilityTick(self.__tree, given_bounds, vis_tick)

    def __SetVisibilityTick(self, node, given_bounds, vis_tick):
        if not node.contents: # leaf
            for x in node.elements:
                x.SetVisibilityTick(vis_tick)
        else:
            for i in (0, 1, 2, 3):
                cnode = node.contents[i]
                # in bounds
                x_intersects = (given_bounds[0] <= cnode.bounds[2]) and (given_bounds[2] >= cnode.bounds[0])
                y_intersects = (given_bounds[1] <= cnode.bounds[3]) and (given_bounds[3] >= cnode.bounds[1])
                if (x_intersects and y_intersects):
                    if (given_bounds[0] <= cnode.bounds[0] and \
                            given_bounds[1] <= cnode.bounds[1] and \
                            given_bounds[2] > cnode.bounds[2] and \
                            given_bounds[3] > cnode.bounds[3]):
                        # Update all objets because the node is completely contained within the
                        # given bounds
                        cnode.SetVisibilityTickOnAllChildElements(vis_tick)
                    else:
                        self.__SetVisibilityTick(cnode, given_bounds, vis_tick)

    def GetAllObjects(self):
        return self.__objects

    def RefreshObject(self, obj):
        self.RemoveSingleFromTree(obj)
        self.AddSingleToTree(obj)

