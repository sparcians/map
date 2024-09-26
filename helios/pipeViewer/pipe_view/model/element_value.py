# A convenient wrapper for individual Elements and the [Layout]
# Context-specific value they should display when drawn with a Layout
# Canvas. For all intents and purposes, this could be accomplished with
# [Element, val].
from __future__ import annotations
from bisect import bisect, bisect_left
from typing import Any, Dict, List, Optional, Tuple, Union
from . import content_options as content
from . import highlighting_utils
from .element import Element, FakeElement, PropertyValue
from .schedule_element import ScheduleLineElement


TimedVal = Tuple[Optional[Union[int, str]], Tuple[int, int]]
TimedDict = Dict[int, TimedVal]


class Element_Value:
    __TIMED_VAL_MAX_CAPACTIY = 1200

    # number of periods on either side of edge of window to leave transactions
    __EVICT_MARGIN = 3

    def __init__(self, e: Element, val: str = '') -> None:
        self.__element = e
        self.__val = val
        self.__clock_period = -1
        self.__timed_vals: TimedDict = {}
        self.__timed_vals_uop_uids: Dict[int, Optional[int]] = {}
        self.__update_index = -1
        self.__metas: Optional[Dict[str, Any]] = None

        # Missing location. This differs from __resolved_location in that it
        # implies that the element NEEDS a location to display anyting but has
        # none
        self.__missing_loc = False

        # Cached display data
        self.__resolved_location: Optional[str] = None
        self.__display_t_offset: Optional[int] = None

        # Tick ID of last update
        self.__vis_tick = -1

    # Returns keys stored for this object in the database
    # if nothing stored, returns None
    def GetMetaEntries(self) -> Optional[Dict[str, Any]]:
        return self.__metas

    # Sets the metadata to reference the \a entries argument
    # @param entries Dictionary of meta-data
    def SetMetaEntries(self, entries: Optional[Dict[str, Any]]) -> None:
        assert entries is None or isinstance(entries, dict), \
            f'Attempted to set metadata which was not a dict: {entries}'
        self.__metas = entries

    # Updates the current meta-entries, overwriting only keys in added. This is
    # simply a dictionary update and should only be used if the SetMetaEntries
    # method has already been used this tick
    # @param entries Dictionary of meta-data. If None, has no effect
    def UpdateMetaEntries(self, entries: Optional[Dict[str, Any]]) -> None:
        assert entries is None or isinstance(entries, dict), \
            f'Attempted to set metadata which was not a dict: {entries}'
        if self.__metas is None:
            self.__metas = entries
        elif entries is not None:
            self.__metas.update(entries)

    # Gets the auxilliary metadata properties for this element
    def GetAuxMetadataProperties(self) -> List[str]:
        return self.__element._AUX_METADATA_PROPERTIES

    # Gets the property to use as the metadata key for this element
    def GetMetadataKey(self) -> str:
        return self.__element._METADATA_KEY_PROPERTY

    # Returns the value (str) that should be drawn with a Layout Canvas
    def GetVal(self) -> str:
        return self.__val

    # Sets the value after confirming it is a str
    # @post Marks this element value as NOT missing its location
    def SetVal(self, val: str, up_idx: Optional[int] = None) -> None:
        if not isinstance(val, str):
            raise TypeError('Value must be a string, was a ' + type(val))
        self.__val = val
        if up_idx:
            self.__update_index = up_idx
        self.__missing_loc = False

    # Marks this element value as missing a location
    # @post Updates value to be the content_options 'no trans' string
    def SetMissingLocation(self) -> None:
        self.SetVal(content.OverrideState('no trans'))
        self.__missing_loc = True

    # Is this element value missing location
    def IsMissingLocation(self) -> bool:
        return self.__missing_loc

    # Set value to values keyed by time
    def SetTimedVal(self, time: int, val: TimedVal) -> None:
        if len(self.__timed_vals) > self.__TIMED_VAL_MAX_CAPACTIY:
            q_frame = self.__element.GetQueryFrame(self.__clock_period)
            if q_frame and isinstance(self.__element, ScheduleLineElement):
                margin = self.__EVICT_MARGIN*self.__clock_period
                hc = self.__element.GetTime()
                q_frame = q_frame[0]+hc-margin, q_frame[1]+hc+margin
                # clear everything not in q frame
                new = {}
                new_uop_uids = {}
                keys = self.__GetKeysInRange(q_frame)
                for key in keys:
                    new[key] = self.__timed_vals[key]
                    new_uop_uids[key] = self.__timed_vals_uop_uids[key]
                del self.__timed_vals
                del self.__timed_vals_uop_uids
                self.__timed_vals = new
                self.__timed_vals[time] = val
                self.__timed_vals_uop_uids = new_uop_uids
                self.__timed_vals_uop_uids[time] = \
                    highlighting_utils.GetUopUid(val[0])
            else:
                raise Exception(
                    'Should not be setting timed value on item without '
                    'time-frame.'
                )
        else:
            self.__timed_vals[time] = val
            self.__timed_vals_uop_uids[time] = \
                highlighting_utils.GetUopUid(val[0])

    def SetClockPeriod(self, period: int) -> None:
        self.__clock_period = period

    def GetClockPeriod(self) -> int:
        return self.__clock_period

    # purge cache
    def ClearTimedValues(self) -> None:
        self.__timed_vals.clear()
        self.__timed_vals_uop_uids.clear()

    # Get value at a particular time
    def GetTimedVal(self, time: int) -> Optional[TimedVal]:
        return self.__timed_vals.get(time)

    def GetTimedValues(self) -> TimedDict:
        return self.__timed_vals

    # Get the UID for the uop at the given time
    def GetTimedValUopUid(self, time: int) -> Optional[int]:
        return self.__timed_vals_uop_uids.get(time)

    def __GetKeysInRange(self, interval: Tuple[int, int]) -> List[int]:
        start_tick, end_tick = interval
        # fast method for returning range of data
        times = list(self.__timed_vals.keys())
        times = sorted(times)
        start_ind = bisect_left(times, start_tick)
        end_ind = bisect(times, end_tick)

        if start_ind > 0:
            # Since we are now merging transactions across heartbeat intervals,
            # we need to check if the previous interval overlaps the current
            # query range
            last_int_start, last_int_end = self.__timed_vals[times[start_ind-1]][1]  # noqa: E501
            # Iterate backwards until we no longer overlap
            while (start_tick >= last_int_start and
                   start_tick <= last_int_end and
                   start_ind > 0):
                start_ind -= 1
                last_int_start, last_int_end = self.__timed_vals[times[start_ind-1]][1]  # noqa: E501

        return times[start_ind:end_ind]

    # Gets all values stored on a given time (units: ticks) interval.
    def GetTimeRange(self, interval: Tuple[int, int]) -> List[TimedVal]:
        keys_in_range = self.__GetKeysInRange(interval)
        return [self.__timed_vals[key] for key in keys_in_range]

    # Used to determine if an Element Value has an up-to-date val
    @property
    def updateindex(self) -> int:
        return self.__update_index

    # Returns the Element belonging to this instance
    def GetElement(self) -> Element:
        return self.__element

    # Used so that Element_Values can be compared to Elements
    def GetPIN(self) -> int:
        return self.__element.GetPIN()

    # Set tick ID of last visibility upadte for this element
    # The renderer will query this to determine if the element should be draw
    # this update
    def SetVisibilityTick(self, t: int) -> None:
        self.__vis_tick = t

    # Get tick ID of last visibility upadte for this element
    def GetVisibilityTick(self) -> int:
        return self.__vis_tick

    # Set the location and timing information used to order this object in a
    # QuerySet. This must be cached in case the t_offset or location is changed
    # (e.g. by changing location variables) before it is deleted. This is
    # because deleting the element from the QuerySet requires knowledge of the
    # clock (through location) and t_offset
    # @param t_offset Current value of t_offset property
    # @param location Location string with location-string variables resolved
    def SetLocationAndTimingInformation(self,
                                        t_offset: int,
                                        location: Optional[str]) -> None:
        self.__display_t_offset = t_offset
        self.__resolved_location = location

    # Get the fully-resolved location string (no variables) as set by
    # SetLocationAndTimingInformation. If none, this object did not correspond
    # to a real location.
    def GetDisplayLocationString(self) -> Optional[str]:
        return self.__resolved_location

    # Get the t_offset as set by
    # SetLocationAndTimingInformation
    def GetDisplayTOffset(self) -> Optional[int]:
        return self.__display_t_offset

    # Used so that Element_Values can be compared to Elements or a None object
    # Comparison with other types is not allowed
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, (Element, Element_Value)):
            return False
        return self.GetPIN() == other.GetPIN()

    def __hash__(self) -> int:
        return self.GetPIN()


# For same purpose as fake element, just plays a different role.
class FakeElementValue(Element_Value):
    def __init__(self, element: FakeElement, value: PropertyValue) -> None:
        self.__element = element
        self.__value = str(value)
        self.__clock_period = 1

    def GetVal(self) -> str:
        return self.__value

    def GetElement(self) -> Element:
        return self.__element

    def GetClockPeriod(self) -> int:
        return self.__clock_period

    def SetClockPeriod(self, period: int) -> None:
        self.__clock_period = period
