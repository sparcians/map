
from .element_value import Element_Value
from .query_set import QuerySet
from .quad_tree import QuadTree

## ElementSet stores all elements in a LayoutContext and bins them by time appropriately
# Purely drawable objects are only put in a draw list.
# Objects that request data from the database are put in a QuerySet,
# Objects with both are placed in both.
class ElementSet:
    def __init__(self, layout_context, extensions):
        self.__draw_set = []
        self.__meta_set = []
        self.__query_set = QuerySet(layout_context)
        #used to quickly find pairs from elements
        self.__elements_to_pairs = {}
        self.__layout_context = layout_context
        self.__extensions = extensions
        # tree used for display calls
        self.__tree = QuadTree()
        self.__vis_tick = 0

    ## Adds a new element to the set
    #  @param e Element to add
    #  @parm after_pins ordered PINs of elements after which to insert this element
    def AddElement(self, e, after_pins=[None]):
        pair = Element_Value(e)
        self.__elements_to_pairs[e] = pair
        if e.NeedsDatabase():
            self.__query_set.AddPair(pair)
        if e.IsDrawable():
            # in the case of a pure drawable object (no query), Value_Pair val is empty
            #self.__tree.AddObject(pair)
            #self.__draw_set.append(pair)

            self.__InsertDrawableAfterPIN(pair, after_pins=after_pins)
        if e.UsesMetadata():
            self.__meta_set.append(pair)
        if e.HasProperty('on_init'):
            on_init = e.GetProperty('on_init')
            if on_init and on_init != 'None':
                func = self.__extensions.GetFunction(on_init)
                if func:
                    func(pair, self.__layout_context, 0)
                else:
                    print('Warning: unable to call \"%s\"'%e.GetProperty('on_init'))
                    print(func)


    ## Removes an element from the set
    def RemoveElement(self, e):
        pair = self.__elements_to_pairs[e]
        if e.NeedsDatabase():
            self.__query_set.DeletePair(pair)
        if e.IsDrawable():
            self.__draw_set.remove(pair)
            self.__tree.RemoveObject(pair)
        if e.UsesMetadata():
            self.__meta_set.remove(pair)
        del self.__elements_to_pairs[e]


    ## Resorts an element. Mostly for query-type objects.
    def ReSort(self, e, t_offs, id):
        #q-frame objects need no resorting. make a bogus call to GetQueryFrame
        if e.NeedsDatabase() and not e.GetQueryFrame(1):
            pair = self.__elements_to_pairs[e]
            self.__query_set.ReSort(pair, t_offs, id)
        # Don't currently do anything for pure drawables.

    ## Resort all elements which depend on database queries
    def ReSortAll(self):
        for e in list(self.__elements_to_pairs.keys()):
            if e.NeedsDatabase() and not e.GetQueryFrame(1):
                loc = e.GetProperty("LocationString")
                t_off = e.GetProperty("t_offset")
                id = self.__layout_context.dbhandle.database.location_manager.getLocationInfo(loc, self.__layout_context.GetLocationVariables())[0]
                self.ReSort(e, t_off, id)

    ## Update value
    def ReValue(self, e):
        if e.NeedsDatabase():
            pair = self.__elements_to_pairs[e]
            self.__query_set.ReValue(pair)

    ## Used in the event that many elements were changed (e.g. a location string
    #  variable was updated)
    def ReValueAll(self):
        for pair in self.__elements_to_pairs.values():
            self.__query_set.ReValue(pair)

    def RefreshPair(self, pair):
        self.__tree.RefreshObject(pair)

    ## Called every major render update. This makes sure objects are updated when they are moved.
    def MicroUpdate(self):
        self.__tree.Update()
        if self.__layout_context.IsElementMoved():
            frame = self.__layout_context.GetFrame()
            if frame:
                elements = frame.GetCanvas().GetSelectionManager().GetSelection()
                for el in elements:
                    self.RefreshPair(self.__elements_to_pairs[el])

    def HandleCycleChangedEvent(self):
        # call custom updates
        # keyed by function, stores number of times function accessed in Update
        indices = {}
        
        for element in self.GetElements():
            if element.HasProperty('on_cycle_changed'):
                 on_cycle_changed = element.GetProperty('on_cycle_changed')
                 if on_cycle_changed and on_cycle_changed != 'None':
                     func = self.__extensions.GetFunction(on_cycle_changed)
                     if func:
                         if indices.get(func) == None:
                             indices[func] = 0
                         #print "Calling function for {}".format(element.GetProperty('LocationString'))
                         func(self.__elements_to_pairs[element], self.__layout_context, indices[func])
                         indices[func] += 1
                     else:
                         print('Warning: unable to call \"%s\"'%element.GetProperty('on_cycle_changed'))

    ## Update what needs to be updated.
    #  @param tick Tick at which update is happening (Helps in maintaining
    #  metadata between layout windows)
    def Update(self, tick):

        # Update element content before updating meta-data
        # Could go into a slower-updating loop
        self.__query_set.Update()

        # call custom updates
        # keyed by function, stores number of times function accessed in Update
        indices = {}

        # Purge metadata at the start of a new tick
        if self.__layout_context.dbhandle.database.GetMetadataTick() != tick:
            self.__layout_context.dbhandle.database.PurgeMetadata()
            self.__layout_context.dbhandle.database.SetMetadataTick(tick)

        for element in self.GetElements():
            if element.HasProperty('on_update'):
                 on_update = element.GetProperty('on_update')
                 if on_update and on_update != 'None':
                     func = self.__extensions.GetFunction(on_update)
                     if func:
                         if indices.get(func) == None:
                             indices[func] = 0
                         func(self.__elements_to_pairs[element], self.__layout_context, indices[func])
                         indices[func] += 1
                     else:
                         print('Warning: unable to call \"%s\"'%element.GetProperty('on_update'))

    ## Updates meta-data for elements in this element set
    #  @note Call immediately before drawing
    #  @param tick Tick at which update/redraw is happening
    def MetaUpdate(self, tick):
        assert self.__layout_context.dbhandle.database.GetMetadataTick() == tick, \
               'MetaUpdate called where meta data was stale'

        # update pairs with current metadata (happens anyway with pointers?)
        db = self.__layout_context.dbhandle.database
        for pair in self.__meta_set:
            pair.SetMetaEntries(db.GetMetadata(pair.GetElement().GetProperty(pair.GetMetadataKey())))
            for aux_prop in pair.GetAuxMetadataProperties():
                pair.UpdateMetaEntries(db.GetMetadata(pair.GetElement().GetProperty(aux_prop)))


    ## Always refreshes all objects and clears local buffers
    def FullUpdate(self):
        for element in self.GetElements():
            element.SetChanged()
        self.Update(self.__layout_context.GetHC())

    ## Force a redraw of all elements that have changed their highlighting state.
    def RedrawHighlighted(self):
        for element, pair in self.__elements_to_pairs.items():
            if element.GetType() == 'schedule_line_ruler':
                continue
            for key in pair.GetTimedValues().keys():
                redraw_set = False
                if element.HasProperty('LocationString'):
                    location = element.GetProperty('LocationString')
                    if (self.__layout_context.IsSearchResult(key, location) or self.__layout_context.WasSearchResult(key, location)):
                        element.SetNeedsRedraw()
                        redraw_set = True

                if not redraw_set:
                    uop_uid = pair.GetTimedValUopUid(key)
                    if uop_uid is not None:
                        if self.__layout_context.IsUopUidHighlighted(uop_uid) or self.__layout_context.WasUopUidHighlighted(uop_uid):
                            element.SetNeedsRedraw()

    ## Force a full redraw of the elements.
    def RedrawAll(self):
        for element in self.GetElements():
            element.SetNeedsRedraw()

    ## Force a DB update.
    def DBUpdate(self):
        self.RedrawAll()
        self.Update(self.__layout_context.GetHC())

    ## Get depth-sorted pairs with the intention of drawing them while updating each ElementValue's
    #  visibility tick for filtering by renders/selectors
    #  @return All drawables pairs sorted by draw depth (back first). Caller must filter
    #  out-of-bounds elements using ElementValue.GetVisibilityTick()
    def GetDrawPairs(self, bounds=None):
        if bounds:
            # Use quadtree
            ##self.__tree.GetObjects(bounds)
            self.__vis_tick += 1
            if self.__vis_tick > 10000:
                self.__vis_tick = 0
                # TODO: invalidate vis tick on all elements!
            self.__tree.SetVisibilityTick(bounds, self.__vis_tick)
        else:
            pass # Do not update visibility tick. Caller doesn't care

        # Use ordered draw set. Caller must filter elements based on GetVisibilityTick() comparison
        # with ElementValue.GetVisibilityTick
        return self.__draw_set

    def GetVisibilityTick(self):
        return self.__vis_tick

    ##Get Element_Value pairs from set.
    def GetPairs(self, bounds=None):
        return list(self.__elements_to_pairs.values())

    def GetPair(self, e):
        return self.__elements_to_pairs.get(e)

    ## Get Elements from set.
    def GetElements(self):
        return list(self.__elements_to_pairs.keys())

    ## Insert a drawable after the given PIN
    def __InsertDrawableAfterPIN(self, pair, after_pins=[None]):
        ##print 'Inserting to draw context pair {} after PINs {}'.format(pair, after_pins)

        self.__tree.AddObject(pair) # No pin ordering support yet

        # Care only about element since the draw list will contain all elements we care about
        after_pin = after_pins[-1]

        if after_pin == None:
            self.__draw_set.append(pair)
            ##print '  Appending to draw'
        elif after_pin == -1:
            self.__draw_set.insert(0, pair)
            ##print '  Inserting in draw @ 0'
        else:
            for idx,op in enumerate(self.__draw_set):
                if op.GetElement().GetPIN() == after_pin:
                    self.__draw_set.insert(idx+1, pair)
                    ##print '  Inserting in draw at {}/{} (highest PIN={})' \
                    ##      .format(idx+1,
                    ##              len(self.__draw_set),
                    ##              max([x.GetElement().GetPIN() for x in self.__draw_set]))
                    ##assert pair in self.__draw_set, 'Pair not actually inserted?'
                    break
                pass
            else:
                self.__draw_set.append(pair)
                ##print '  Appending to draw (2)'
