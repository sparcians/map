from __future__ import annotations
import logging
import weakref
import os
import time
from typing import Any, Dict, List, Optional, Set, Tuple, Union, cast, overload, TYPE_CHECKING
import wx

from .layout import Layout
from .element_set import ElementSet
from .database_handle import DatabaseHandle
from .search_handle import SearchHandle
from .element import Element, FakeElement
from .extension_manager import ExtensionManager
from model.location_manager import LocationManager, LocationType
import model.content_options as content
import model.highlighting_utils as highlighting_utils


if TYPE_CHECKING:
    from gui.layout_frame import Layout_Frame
    from gui.dialogs.search_dlg import SearchResult
    from model.database import Database, TransactionDatabase
    from model.element_value import Element_Value
    from model.group import Group
    from model.schedule_element import ScheduleLineElement, ScheduleElement

class Layout_Context:

    EXTENT_L = 0 # Left
    EXTENT_T = 1 # Top
    EXTENT_R = 2 # Right
    EXTENT_B = 3 # Bottom

    # # Dummy class for showing that extents are invalid
    class InvalidExtents:
        pass

    # # Creates an OrderedDict for the Layout
    #  @param loc_vars location string variables dictionary. This reference is
    #  used (not copied) Created if None
    def __init__(self,
                 layout: Layout,
                 db: Database,
                 hc: Optional[int] = None,
                 loc_vars: Optional[Dict[str, str]] = None) -> None:
        if hc is None:
            hc = 0
        self.__layout = None # Will be updated later. Allows __str__ to succeed
        self.__group: Optional[Group] = None # Sync group
        self.__frame: Optional[weakref.ReferenceType[Layout_Frame]] = None # Parent Layout_Frame (weak reference)
        self.__db = db
        self.__dbhandle = DatabaseHandle(db)
        self.__searchhandle = SearchHandle(self)
        self.__qapi = self.__dbhandle.api
        start = self.__qapi.getFileStart()
        inc_end = self.__qapi.getFileInclusiveEnd()
        if hc < start:
            self.__hc = start
        elif hc > inc_end:
            self.__hc = inc_end
        else:
            self.__hc = hc
        self.__extents: Union[Layout_Context.InvalidExtents, List[int]] = [0, 0, 1, 1]

        self.__loc_variables = loc_vars if loc_vars is not None else {} # Location-string variables
        self.__loc_variables_changed = True if loc_vars is not None else False

        self.__extensions = ExtensionManager()
        filename = layout.GetFilename()
        if filename:
            self.__extensions.AddPath(os.path.dirname(filename))

        self.__elements = ElementSet(self, self.__extensions)
        self.SetHC(self.__hc)

        self.__layout = layout
        self.__layout.LinkLayoutContext(self)
        self.__PullFromLayout()

        # Number of stabbing query results to store at a time
        self.__cache_size = 20

        # get visible clock stuff
        self.__number_elements = 0
        self.__visible_clocks: Optional[Tuple[int, ...]] = None

        # Uop highlighting list
        self.__highlighted_uops: Set[int] = set()
        self.__previously_highlighted_uops: Set[int] = set()
        self.__search_results: Set[int] = set()
        self.__previous_search_results: Set[int] = set()

    # # Returns the SearchHandle held by this context.
    @property
    def searchhandle(self) -> SearchHandle:
        return self.__searchhandle

    # # Returns the DatabaseHandle held by this context.
    @property
    def dbhandle(self) -> DatabaseHandle:
        return self.__dbhandle

    # # Returns the hypercycle this Context is currently centered on
    @property
    def hc(self) -> int:
        return self.__hc

    # # Populate our elements from current layout
    # At the moment, call this only once.
    def __PullFromLayout(self) -> None:
        assert self.__layout is not None
        # Flatten out struture of layout for easy access
        for e in self.__layout.GetElements():
            self.AddElement(e)
            if e.HasChildren():
                children = e.GetChildren()
                for child in children:
                    self.AddElement(child)

    # # Returns the location-string variables dictionary for this layout context
    def GetLocationVariables(self) -> Dict[str, str]:
        return self.__loc_variables

    def GetLocationVariablesChanged(self) -> bool:
        return self.__loc_variables_changed

    # # Updates location variables based on any new element locations created with variables in them
    def UpdateLocationVariables(self) -> None:
        assert self.__layout is not None
        for el in self.__layout.GetElements():
            if el.HasProperty('LocationString'):
                loc_str = cast(str, el.GetProperty('LocationString'))
                loc_vars = LocationManager.findLocationVariables(loc_str)
                for k, v in loc_vars:
                    if k in self.__loc_variables and self.__loc_variables[k] != v:
                        pass # Do not add these to the table since there is a conflict
                        # # @todo Represent these conflicting variable defaults somehow so that they
                        # # can be resolved in the location variables dialog
                    else:
                        self.__loc_variables[k] = v
                        self.__loc_variables_changed = True

    def AckLocationVariablesChanged(self) -> None:
        self.__loc_variables_changed = False

    # # Adds a new element to the OrderedDict
    #  @param e Element to add
    #  @param after_pin PIN of element after which to insert this element
    # @profile
    def AddElement(self, e: Element, after_pins: List[Optional[int]] = [None]) -> Element:
        # #print 'Adding {} after pins {}'.format(e, after_pins)
        self.__elements.AddElement(e, after_pins = after_pins)

        # Update extents. This can only increase
        if isinstance(self.__extents, self.InvalidExtents):
            self.__extents = [0, 0, 1, 1]

        (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
        r = x + w
        b = y + h
        self.__extents[self.EXTENT_R] = max(r, self.__extents[self.EXTENT_R])
        self.__extents[self.EXTENT_B] = max(b, self.__extents[self.EXTENT_B])

        return e

    # # Remove an element from the ElementSet
    def RemoveElement(self, e: Element) -> None:
        self.__elements.RemoveElement(e)

        (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
        r = x + w
        b = y + h

        if isinstance(self.__extents, self.InvalidExtents):
            return # Already invalid, nothing to do here. Will recalc later

        # If this element was at the edge, we need a full recalc.
        # Invalidate extents and recalc when asked
        # Just in case extents were wrong and r was greater, use >= cur extent
        if r >= self.__extents[self.EXTENT_R]:
            self.__extents = self.InvalidExtents()
        elif b >= self.__extents[self.EXTENT_B]:
            self.__extents = self.InvalidExtents()

    # # Move a set of elements to above the highest entry in a list
    #  @above_pin_list List of pins above which to move each element in the
    #  elements list. May contain None at the end to indicate "move to top"
    #  @note elements will retain their own ordering
    def MoveElementsAbovePINs(self, elements: List[Element], above_pin_list: List[Optional[int]]) -> None:
        assert self.__layout is not None
        for e in elements:
            self.__layout.RemoveElement(e)
            # #self.__elements.RemoveElement(e)

        if not above_pin_list:
            above_pin_list = [-1]

        prev_pins_list = above_pin_list[:]
        for e in elements:
            # #self.__elements.AddElement(e, after_pin=prev_pin)
            self.__layout.AddElement(e, follows_pins = prev_pins_list)
            prev_pins_list.append(e.GetPIN())

    # # Move a set of elements to below the lowest entry in a list
    #  @below_pin_list List of pins below which to move each element in the
    #  elements list. May contain -1 at the start to indicate "move to bottom"
    #  @note elements will retain their own ordering
    def MoveElementsBelowPINs(self, elements: List[Element], below_pin_list: List[Optional[int]]) -> None:
        assert self.__layout is not None
        for e in elements:
            self.__layout.RemoveElement(e)
            # #self.__elements.RemoveElement(e)

        if not below_pin_list:
            below_pin_list = [None]

        prev_pins_list = below_pin_list[:]
        for e in elements:
            # #self.__elements.AddElement(e, after_pin=prev_pin)
            self.__layout.AddElement(e, follows_pins = prev_pins_list)
            prev_pins_list.append(e.GetPIN())

    # # If a property for an Element is changed such that it will no longer be
    #  correctly sorted in the ElementSet, this method will figure out where
    #  it goes
    def ReSort(self, e: Element, t_off: int, loc: str) -> None:
        id = self.__dbhandle.database.location_manager.getLocationInfo(loc, self.__loc_variables)[0]
        self.__elements.ReSort(e, t_off, id)

    # # Resort all elements because some locations variable has changed and all elements may be
    #  effected
    def ReSortAll(self) -> None:
        self.__elements.ReSortAll()

    # # An element in the layout has moved
    def ElementMoved(self, e: Element) -> None:
        # Because this can be called many times during a mass-move or resize,
        # simply invalidate the extents
        self.__extents = self.InvalidExtents()

    # # Returns True if element was moved
    def IsElementMoved(self) -> bool:
        return isinstance(self.__extents, self.InvalidExtents)

    # # Used in the event that a property was changed for an element which may
    #  require an updated value to be displayed
    def ReValue(self, e: Element) -> None:
        self.__elements.ReValue(e)

        # NOTE: This would be a good place to update any variables extracted
        # from this element. Note that this might be very slow here since this
        # method can be invoked during mass-updates

    # # Used in the event that many elements were changed (e.g. a location string
    #  variable was updated)
    def ReValueAll(self) -> None:
        self.__elements.ReValueAll()

    # # Updates this context's elements for the curent cycle
    def Update(self) -> None:
        self.__elements.Update(self.__hc)

    # # update that is called every major display update
    def MicroUpdate(self) -> None:
        self.__elements.MicroUpdate()

    # # Force a DB update.
    def DBUpdate(self) -> None:
        self.__elements.DBUpdate()

    # # Force a full update.
    def FullUpdate(self) -> None:
        self.__elements.FullUpdate()

    # # Force a full redraw of all elements without marking them as changed
    def FullRedraw(self) -> None:
        self.__elements.RedrawAll()

    # # Returns the layout which this Context is referrencing
    def GetLayout(self) -> Optional[Layout]:
        return self.__layout

    def GetExtensionManager(self) -> ExtensionManager:
        return self.__extensions

    def __GetLocationInfo(self, element: Element) -> LocationType:
        return self.__db.location_manager.getLocationInfoNoVars(cast(str, element.GetProperty('LocationString')))

    # # Return a set of all clockid's referred to
    def GetVisibleClocks(self) -> Optional[Tuple[int, ...]]:
        elements = self.GetElements()
        if self.__number_elements != len(elements):
            clock_set = set()

            for element in elements:
                if element.NeedsDatabase():
                    info = self.__GetLocationInfo(element)
                    if info[0] != self.__db.location_manager.INVALID_LOCATION_ID:
                        # only add if valid location
                        clock_set.add(info[2])
            clocks = tuple(clock_set)
            self.__visible_clocks = clocks
            self.__number_elements = len(elements)

        return self.__visible_clocks

    def GetLocationId(self, element: Element) -> int:
        return self.__GetLocationInfo(element)[0]

    # # Return a set of all locations referred to
    def GetVisibleLocations(self) -> Set[int]:
        locations = set()
        for element in self.GetElements():
            if element.NeedsDatabase():
                locations.add(self.GetLocationId(element))
        return locations

    # # Returns the All Objects
    def GetElementPairs(self) -> List[Element_Value]:
        return self.__elements.GetPairs()

    # # Returns all pairs suitable for drawing
    def GetDrawPairs(self, bounds: Optional[Tuple[int, int, int, int]]) -> List[Element_Value]:
        return self.__elements.GetDrawPairs(bounds)

    def GetVisibilityTick(self) -> int:
        return self.__elements.GetVisibilityTick()

    def GetElements(self) -> List[Element]:
        return self.__elements.GetElements()

    def GetElementPair(self, e: Element) -> Optional[Element_Value]:
        return self.__elements.GetPair(e)

    def GetElementExtents(self) -> Tuple[int, int, int, int]:
        '''
        Returns the left,right,top,bottom extents of the layout based on what
        elements it contains
        @note Recalculates extents if necessary
        @return (left,top,right,bottom)
        '''
        if isinstance(self.__extents, self.InvalidExtents):
            self.__extents = [0, 0, 1, 1]
            els = self.GetElements()
            for e in els:
                (x, y), (w, h) = cast(Tuple[int, int], e.GetProperty('position')), cast(Tuple[int, int], e.GetProperty('dimensions'))
                self.__extents[self.EXTENT_R] = max(self.__extents[self.EXTENT_R], x + w)
                self.__extents[self.EXTENT_B] = max(self.__extents[self.EXTENT_B], y + h)

        return cast(Tuple[int, int, int, int], tuple(self.__extents))

    # # For testing purposes only
    def __repr__(self) -> str:
        return '<Layout_Context layout={}>'.format(self.__layout)

    # # Jumps context to a specific tick.
    #  @param hc Hypercycle (tick) to jump to. This tick will be constrained
    #  to the endpoints of this database handle's file range
    #  @note Directly refreshes the associated Frame if not attached to a group.
    #  Otherwise, the this context and the associated frame will be refreshed
    #  through the group, when it invokes 'RefreshFrame' on all its contained
    #  layout
    #  contexts
    #  @todo rework this
    #
    #  Performs new queries at the chosen tick and updates element data
    def GoToHC(self, hc: Optional[int] = None, no_broadcast: bool = False) -> None:
        # print "{}: GoToHC called".format(time.time())
        # show busy cursor every call
        assert self.__frame is not None
        frame = self.__frame()
        if frame:
            frame.SetBusy(True)

        if hc is None:
            hc = self.__hc
        hc = self.__ClampHC(hc)
        if self.__group is not None:
            self.__group.MoveTo(hc, self, no_broadcast = no_broadcast)
        else:
            self.SetHC(hc, no_broadcast = no_broadcast)
            self.RefreshFrame()
        # print "{}: Refresh done".format(time.time())

        # set cursor back
        if frame:
            frame.SetBusy(False)

    # # Sets the current tick and updates.
    #  This does not notify groups and is an internal method
    #  @param hc New hypercycle (tick)
    #  @note Does not refresh. Refresh must be called separately (or use GoToHC
    #  which Refreshes or notififes a group which indirectly refreshes).
    def SetHC(self, hc: int, no_broadcast: bool = False) -> None:
        self.__hc = hc
        if not no_broadcast:
            self.__elements.HandleCycleChangedEvent()
        self.Update()

    # # Refresh this context (and its associated frame)
    def RefreshFrame(self) -> None:
        assert self.__frame, \
                   'A Layout_Context should always have a frame before attempting a RefreshFrame call'
        self.__elements.MetaUpdate(self.__hc)
        frame = self.__frame()
        if frame:
            frame.Refresh()

    def GetHC(self) -> int:
        '''
        Returns the current hypercycle (tick) for this layout context
        '''
        return self.__hc

    def SetGroup(self, group: Group) -> None:
        assert self.__group is None, \
               '(for now) SetGroup cannot be called on a LayoutContext after it already has a group'
        assert group is not None, \
               'SetGroup parameter group must not be None'
        logging.getLogger('LayoutContext').debug('Context {} adding to group {}'.format(self, group))
        self.__group = group
        self.__group.AddContext(self)

    def LeaveGroup(self) -> None:
        assert self.__group is not None, 'LeaveGroup cannot be called on a LayoutContext before it has joined a group'
        logging.getLogger('LayoutContext').debug('Context {} leaving group {}'.format(self, self.__group))
        self.__group.RemoveContext(self)

    def GetGroup(self) -> Optional[Group]:
        return self.__group

    def SetFrame(self, frame: Layout_Frame) -> None:
        assert self.__frame is None, \
               'SetFrame cannot be called on a LayoutContext after it already has a frame'
        assert frame is not None, \
               'SetFrame parameter frame must not be None'
        logging.getLogger('LayoutContext').debug('Context {} associated with frame {}'.format(self, frame))
        self.__frame = weakref.ref(frame)

    # # Returns the frame associated with this context. If the associated frame
    #  was destroyed (or no Frame associated), returns None
    def GetFrame(self) -> Optional[Layout_Frame]:
        if self.__frame is None:
            return None
        return self.__frame() # May be None

    # Clamp the HC to the file extents
    def __ClampHC(self, hc: int) -> int:
        hc = max(hc, self.__qapi.getFileStart())
        hc = min(hc, self.__qapi.getFileInclusiveEnd()) # End is normally exclusive
        return hc

    # # Returns a list of all Elements beneath the given point
    #  @param pt Point to test for collision with elements
    #  @param include_subelements Should subelements be searched (e.c. schedule line within a
    #  schedule)
    #  @param include_nondrawables Should selectable elements be returned even if they aren't
    #  drawable? Depth ordering might be lost when including non drawables
    #  Subelements are fake elements generated by elements on a collision
    def DetectCollision(self,
                        pt: Union[Tuple[int, int], wx.Point],
                        include_subelements: bool = False,
                        include_nondrawables: bool = False) -> List[Element_Value]:
        mx, my = pt
        res: List[Element_Value] = []
        # Search draw pairs instead of all element pairs because they are
        #  (1) visible
        #  (2) sorted by depth
        # ##for e in self.GetElementPairs():

        # Get bounds for quad-tree query
        # #bounds = None
        assert self.__frame is not None
        frame = self.__frame()
        if frame:
            bounds = frame.GetCanvas().GetBounds()
        else:
            bounds = None

        # Query by draw pairs to get depth order correct
        if include_nondrawables:
            pairs = self.GetElementPairs()
        else:
            pairs = self.GetDrawPairs(bounds = bounds)
            vis_tick = self.__elements.GetVisibilityTick() # After GetDrawPairs
        for e in pairs:
            if not include_nondrawables and bounds is not None and e.GetVisibilityTick() != vis_tick:
                continue # Skip: this is off-screen

            element = e.GetElement()
            x, y = cast(Tuple[int, int], element.GetProperty('position'))
            w, h = cast(Tuple[int, int], element.GetProperty('dimensions'))
            if x <= mx <= (x + w) and y <= my <= (y + h):
                if include_subelements:
                    et = element.GetProperty('type')
                    if et == 'schedule':
                        element = cast('ScheduleElement', element)
                        sl = element.DetectCollision((mx, my))
                        if sl and sl.GetProperty('type') == 'schedule_line':
                            sl = cast('ScheduleLineElement', sl)
                            # Hierarchical point containment test assumes that schedule
                            # objects contain schedule lines
                            mx, my = pt
                            c_x, c_y = cast(Tuple[int, int], sl.GetProperty('position'))
                            loc_x = mx - c_x
                            loc_y = my - c_y

                            # Go inside this scheule line
                            sub_object = sl.DetectCollision((loc_x, loc_y), e)
                            if sub_object:
                                res.append(sub_object)
                    elif et == 'schedule_line':
                        element = cast('ScheduleLineElement', element)
                        mx, my = pt
                        c_x, c_y = cast(Tuple[int, int], element.GetProperty('position'))
                        loc_x = mx - c_x
                        loc_y = my - c_y

                        # Go inside this element
                        sub_object = element.DetectCollision((loc_x, loc_y), e)
                        if sub_object:
                            res.append(sub_object)
                    else:
                        res.append(e)
                else:
                    # just attach element
                    res.append(e)

        return res

    def GetLocationPeriod(self, location_string: str) -> int:
        '''
        a function for getting the period of the clock at a certain time
        '''
        clock = self.dbhandle.database.location_manager.getLocationInfo(location_string,
                                                                        self.GetLocationVariables())[2]
        return self.dbhandle.database.clock_manager.getClockDomain(clock).tick_period

    def GetTransactionFields(self, time: int, location_string: str, fields: List[str]) -> Dict[str, Any]:
        '''
        performs random-access query at time and place and returns requested attributes in dictionary
        @return Dictionary of results {field:value}
        '''
        # No results if time is outside of the currently loaded window.
        # Cannot do a reasonably fast query and there is no way tha the data is currently visible to
        # the user anyway
        dbapi = self.dbhandle.database.api
        if time < dbapi.getWindowLeft() or time >= dbapi.getWindowRight():
            return {}

        results = {}

        def callback(t: int, tapi: TransactionDatabase) -> None:
            assert t == time, f'bad tick {t}'
            loc_mgr = self.dbhandle.database.location_manager
            location = loc_mgr.getLocationInfo(location_string,
                                               self.GetLocationVariables())[0]
            trans_proxy = tapi.getTransactionProxy(location)
            if trans_proxy:
                for field in fields:
                    if field == 'time':
                        value = t
                    else:
                        fake_element = FakeElement()
                        fake_element.SetProperty('LocationString', location_string)
                        value = content.ProcessContent(field,
                                                       trans_proxy,
                                                       fake_element,
                                                       self.dbhandle,
                                                       t,
                                                       self.GetLocationVariables())
                    results[field] = value

        self.dbhandle.query(time, time, callback, mod_tracking = False)
        return results

    def SearchResultHash(self, start: int, location: int) -> int:
        return hash(f'{start}:{location}')

    def AddSearchResult(self, search_entry: SearchResult) -> None:
        self.__search_results.add(self.SearchResultHash(search_entry['start'], search_entry['location']))

    def ClearSearchResults(self) -> None:
        self.__previous_search_results.update(self.__search_results)
        self.__search_results.clear()

    @overload
    def IsSearchResult(self, start: Optional[int]) -> bool: ...

    @overload
    def IsSearchResult(self, start: int, location: int) -> bool: ...

    def IsSearchResult(self, start: Optional[int], location: Optional[int] = None) -> bool:
        if location is None:
            # start is actually the hash
            return start in self.__search_results
        else:
            assert start is not None
            return self.IsSearchResult(self.SearchResultHash(start, location))

    @overload
    def WasSearchResult(self, start: Optional[int]) -> bool: ...

    @overload
    def WasSearchResult(self, start: int, location: int) -> bool: ...

    def WasSearchResult(self, start: Optional[int], location: Optional[int] = None) -> bool:
        if location is None:
            # start is actually the hash
            return start in self.__search_results
        else:
            assert start is not None
            return self.WasSearchResult(self.SearchResultHash(start, location))

    def HighlightUop(self, uid: Optional[Union[int, str]]) -> None:
        '''
        Highlight the uop with the given annotation string
        '''
        if isinstance(uid, str):
            self.HighlightUop(highlighting_utils.GetUopUid(uid))
        elif uid is not None:
            self.__highlighted_uops.add(uid)

    def UnhighlightUop(self, uid: Optional[Union[int, str]]) -> None:
        '''
        Unhighlight the uop with the given annotation string
        '''
        if isinstance(uid, str):
            self.UnhighlightUop(highlighting_utils.GetUopUid(uid))
        elif uid is not None:
            if uid in self.__highlighted_uops:
                self.__highlighted_uops.remove(uid)
                self.__previously_highlighted_uops.add(uid)

    # # Check if a uop has been highlighted (by UID)
    def IsUopUidHighlighted(self, uop_uid: Optional[int]) -> bool:
        return uop_uid in self.__highlighted_uops

    # # Check if a uop has been unhighlighted (by UID), but not yet redrawn
    def WasUopUidHighlighted(self, uop_uid: Optional[int]) -> bool:
        return uop_uid in self.__previously_highlighted_uops

    # # Check if a uop has been highlighted (by annotation string)
    def IsUopHighlighted(self, uid: Optional[Union[int, str]]) -> bool:
        if isinstance(uid, str):
            return self.IsUopHighlighted(highlighting_utils.GetUopUid(uid))
        return uid in self.__highlighted_uops

    # # Check if a uop has been unhighlighted (by annotation string), but not yet redrawn
    def WasUopHighlighted(self, uid: Optional[Union[int, str]]) -> bool:
        if isinstance(uid, str):
            return self.WasUopHighlighted(highlighting_utils.GetUopUid(uid))
        return uid in self.__previously_highlighted_uops

    # # Redraw elements that have changed their highlighting state
    def RedrawHighlightedElements(self) -> None:
        self.__elements.RedrawHighlighted()
        self.__previously_highlighted_uops.clear()
        self.__previous_search_results.clear()
