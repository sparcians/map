from __future__ import annotations
from .element_value import Element_Value
from .query_set import QuerySet
from .quad_tree import QuadTree

from typing import Callable, Dict, List, Optional, Tuple, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from model.element import Element
    from model.extension_manager import ExtensionManager
    from model.layout_context import Layout_Context


# ElementSet stores all elements in a LayoutContext and bins them by time
# appropriately
# Purely drawable objects are only put in a draw list.
# Objects that request data from the database are put in a QuerySet,
# Objects with both are placed in both.
class ElementSet:
    def __init__(self,
                 layout_context: Layout_Context,
                 extensions: ExtensionManager) -> None:
        self.__draw_set: List[Element_Value] = []
        self.__meta_set: List[Element_Value] = []
        self.__query_set = QuerySet(layout_context)
        # used to quickly find pairs from elements
        self.__elements_to_pairs: Dict[Element, Element_Value] = {}
        self.__layout_context = layout_context
        self.__extensions = extensions
        # tree used for display calls
        self.__tree = QuadTree()
        self.__vis_tick = 0

    # Adds a new element to the set
    # @param e Element to add
    # @param after_pins ordered PINs of elements after which to insert this
    #        element
    def AddElement(self,
                   e: Element,
                   after_pins: List[Optional[int]] = [None]) -> None:
        pair = Element_Value(e)
        self.__elements_to_pairs[e] = pair
        if e.NeedsDatabase():
            self.__query_set.AddPair(pair)
        if e.IsDrawable():
            self.__InsertDrawableAfterPIN(pair, after_pins=after_pins)
        if e.UsesMetadata():
            self.__meta_set.append(pair)
        if e.HasProperty('on_init'):
            on_init = cast(str, e.GetProperty('on_init'))
            if on_init and on_init != 'None':
                func = self.__extensions.GetFunction(on_init)
                if func:
                    func(pair, self.__layout_context, 0)
                else:
                    print('Warning: unable to call \"%s\"' % on_init)
                    print(func)

    # Removes an element from the set
    def RemoveElement(self, e: Element) -> None:
        pair = self.__elements_to_pairs[e]
        if e.NeedsDatabase():
            self.__query_set.DeletePair(pair)
        if e.IsDrawable():
            self.__draw_set.remove(pair)
            self.__tree.RemoveObject(pair)
        if e.UsesMetadata():
            self.__meta_set.remove(pair)
        del self.__elements_to_pairs[e]

    # Resorts an element. Mostly for query-type objects.
    def ReSort(self, e: Element, t_offs: int, id: int) -> None:
        # q-frame objects need no resorting. make a bogus call to GetQueryFrame
        if e.NeedsDatabase() and not e.GetQueryFrame(1):
            pair = self.__elements_to_pairs[e]
            self.__query_set.ReSort(pair, t_offs, id)
        # Don't currently do anything for pure drawables.

    # Resort all elements which depend on database queries
    def ReSortAll(self) -> None:
        for e in list(self.__elements_to_pairs.keys()):
            if e.NeedsDatabase() and not e.GetQueryFrame(1):
                loc = cast(str, e.GetProperty("LocationString"))
                t_off = cast(int, e.GetProperty("t_offset"))
                loc_mgr = self.__layout_context.dbhandle.database.location_manager  # noqa: E501
                id = loc_mgr.getLocationInfo(loc, self.__layout_context.GetLocationVariables())[0]  # noqa: E501
                self.ReSort(e, t_off, id)

    # Update value
    def ReValue(self, e: Element) -> None:
        if e.NeedsDatabase():
            pair = self.__elements_to_pairs[e]
            self.__query_set.ReValue(pair)

    # Used in the event that many elements were changed (e.g. a location string
    # variable was updated)
    def ReValueAll(self) -> None:
        for pair in self.__elements_to_pairs.values():
            self.__query_set.ReValue(pair)

    def RefreshPair(self, pair: Element_Value) -> None:
        self.__tree.RefreshObject(pair)

    # Called every major render update. This makes sure objects are updated
    # when they are moved.
    def MicroUpdate(self) -> None:
        self.__tree.Update()
        if self.__layout_context.IsElementMoved():
            frame = self.__layout_context.GetFrame()
            if frame:
                for el in frame.GetCanvas().GetSelectionManager().GetSelection():  # noqa: E501
                    self.RefreshPair(self.__elements_to_pairs[el])

    def HandleCycleChangedEvent(self) -> None:
        # call custom updates
        # keyed by function, stores number of times function accessed in Update
        indices: Dict[Callable, int] = {}

        for element in self.GetElements():
            if element.HasProperty('on_cycle_changed'):
                on_cycle_changed = \
                    cast(str, element.GetProperty('on_cycle_changed'))
                if on_cycle_changed and on_cycle_changed != 'None':
                    func = self.__extensions.GetFunction(on_cycle_changed)
                    if func:
                        if indices.get(func) is None:
                            indices[func] = 0
                        func(self.__elements_to_pairs[element],
                             self.__layout_context, indices[func])
                        indices[func] += 1
                    else:
                        print(f'Warning: unable to call \"{on_cycle_changed}\"')  # noqa: E501

    # Update what needs to be updated.
    # @param tick Tick at which update is happening (Helps in maintaining
    # metadata between layout windows)
    def Update(self, tick: int) -> None:

        # Update element content before updating meta-data
        # Could go into a slower-updating loop
        self.__query_set.Update()

        # call custom updates
        # keyed by function, stores number of times function accessed in Update
        indices: Dict[Callable, int] = {}

        # Purge metadata at the start of a new tick
        if self.__layout_context.dbhandle.database.GetMetadataTick() != tick:
            self.__layout_context.dbhandle.database.PurgeMetadata()
            self.__layout_context.dbhandle.database.SetMetadataTick(tick)

        for element in self.GetElements():
            if element.HasProperty('on_update'):
                on_update = cast(str, element.GetProperty('on_update'))
                if on_update and on_update != 'None':
                    func = self.__extensions.GetFunction(on_update)
                    if func:
                        if indices.get(func) is None:
                            indices[func] = 0
                        func(self.__elements_to_pairs[element],
                             self.__layout_context, indices[func])
                        indices[func] += 1
                    else:
                        print('Warning: unable to call \"%s\"' % on_update)

    # Updates meta-data for elements in this element set
    # @note Call immediately before drawing
    # @param tick Tick at which update/redraw is happening
    def MetaUpdate(self, tick: int) -> None:
        db = self.__layout_context.dbhandle.database
        assert db.GetMetadataTick() == tick, \
            'MetaUpdate called where meta data was stale'

        # update pairs with current metadata (happens anyway with pointers?)
        for pair in self.__meta_set:
            el = pair.GetElement()
            pair.SetMetaEntries(
                db.GetMetadata(cast(str,
                                    el.GetProperty(pair.GetMetadataKey())))
            )
            for aux_prop in pair.GetAuxMetadataProperties():
                pair.UpdateMetaEntries(
                    db.GetMetadata(cast(str, el.GetProperty(aux_prop)))
                )

    # Always refreshes all objects and clears local buffers
    def FullUpdate(self) -> None:
        for element in self.GetElements():
            element.SetChanged()
        self.Update(self.__layout_context.GetHC())

    # Force a redraw of all elements that have changed their highlighting state
    def RedrawHighlighted(self) -> None:
        for element, pair in self.__elements_to_pairs.items():
            if element.GetType() == 'schedule_line_ruler':
                continue
            for key in pair.GetTimedValues().keys():
                redraw_set = False
                if element.HasProperty('LocationString'):
                    loc_id = self.__layout_context.GetLocationId(element)
                    if (self.__layout_context.IsSearchResult(key, loc_id) or
                       self.__layout_context.WasSearchResult(key, loc_id)):
                        element.SetNeedsRedraw()
                        redraw_set = True

                if not redraw_set:
                    uop_uid = pair.GetTimedValUopUid(key)
                    if uop_uid is not None:
                        if (self.__layout_context.IsUopUidHighlighted(uop_uid) or  # noqa: E501
                           self.__layout_context.WasUopUidHighlighted(uop_uid)):  # noqa: E501
                            element.SetNeedsRedraw()

    # Force a full redraw of the elements.
    def RedrawAll(self) -> None:
        for element in self.GetElements():
            element.SetNeedsRedraw()

    # Force a DB update.
    def DBUpdate(self) -> None:
        self.RedrawAll()
        self.Update(self.__layout_context.GetHC())

    # Get depth-sorted pairs with the intention of drawing them while updating
    # each ElementValue's visibility tick for filtering by renders/selectors
    # @return All drawables pairs sorted by draw depth (back first). Caller
    # must filter out-of-bounds elements using ElementValue.GetVisibilityTick()
    def GetDrawPairs(
        self,
        bounds: Optional[Tuple[int, int, int, int]] = None
    ) -> List[Element_Value]:
        if bounds is not None:
            # Use quadtree
            self.__vis_tick += 1
            if self.__vis_tick > 10000:
                self.__vis_tick = 0
                # TODO: invalidate vis tick on all elements!
            self.__tree.SetVisibilityTick(bounds, self.__vis_tick)
        else:
            pass  # Do not update visibility tick. Caller doesn't care

        # Use ordered draw set. Caller must filter elements based on
        # GetVisibilityTick() comparison with ElementValue.GetVisibilityTick
        return self.__draw_set

    def GetVisibilityTick(self) -> int:
        return self.__vis_tick

    # Get Element_Value pairs from set.
    def GetPairs(
        self,
        bounds: Optional[Tuple[int, int]] = None
    ) -> List[Element_Value]:
        return list(self.__elements_to_pairs.values())

    def GetPair(self, e: Element) -> Optional[Element_Value]:
        return self.__elements_to_pairs.get(e)

    # Get Elements from set.
    def GetElements(self) -> List[Element]:
        return list(self.__elements_to_pairs.keys())

    # Insert a drawable after the given PIN
    def __InsertDrawableAfterPIN(
        self,
        pair: Element_Value,
        after_pins: List[Optional[int]] = [None]
    ) -> None:
        self.__tree.AddObject(pair)  # No pin ordering support yet

        # Care only about element since the draw list will contain all elements
        # we care about
        after_pin = after_pins[-1]

        if after_pin is None:
            self.__draw_set.append(pair)
        elif after_pin == -1:
            self.__draw_set.insert(0, pair)
        else:
            for idx, op in enumerate(self.__draw_set):
                if op.GetElement().GetPIN() == after_pin:
                    self.__draw_set.insert(idx+1, pair)
                    break
                pass
            else:
                self.__draw_set.append(pair)

    def UpdateElementBounds(self) -> None:
        self.__tree.UpdateBounds()
