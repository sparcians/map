
from __future__ import annotations
import copy
import logging
import sys
import time
from typing import Dict, List, Optional, Tuple, cast, TYPE_CHECKING

from . import content_options as content

from logging import info, debug, warn, error

if TYPE_CHECKING:
    from model.element_value import Element_Value
    from model.layout_context import Layout_Context
    from model.schedule_element import ScheduleLineElement
    from model.database import TransactionDatabase

    TOffDict = Dict[int, Dict[int, List[Element_Value]]]

class ContinuedTransaction:
    def __init__(self, interval: Tuple[int, int], processed_val: str, last: bool):
        self.interval = interval
        self.processed_val = processed_val
        self.last = last

    def unwrap(self) -> Tuple[Tuple[int, int], str, bool]:
        return self.interval, self.processed_val, self.last

# #Formerly known as Ordered_Dict
class QuerySet:
    # For sorting elements with no clock
    __DEFAULT_T_OFF = 0

    # # Get the Ordered Dict initialized. Note: in order to force-populate the
    #  Ordered Dict upon initialization, both optional parameters must be provided
    def __init__(self, layout_context: Layout_Context):
        self.__layout_context = layout_context
        self.__handle = self.__layout_context.dbhandle

        # keeps track of the ranges we've already queried
        self.old_hc = 0
        # stores elements that need ranges of data
        self.__range_pairs: List[Element_Value] = []
        self.__continued_transactions: Dict[int, ContinuedTransaction] = {}

        # A count of the number of times this Layout Context has requested to
        # move to a different HC. NOTE: not an index of the current HC, nor
        # number of stabbing queries performed
        self.__stab_index = 0
        # This will be a series of nested dictionaries, with t_offsets (in
        # HC's) and loc_id's as their respective keys. The inmost values
        # will be lists of Element Values
        self.__t_off_sorted: TOffDict = {}

    # # Adds a pair to the query set and stashes in correct location
    # @profile
    def AddPair(self, pair: Element_Value) -> None:
        e = pair.GetElement()
        # Recompute t_off in terms of plain HC's
        ####clock = self.GetClock(pair)
        lmgr = self.__layout_context.dbhandle.database.location_manager
        loc_str = e.GetProperty('LocationString')
        variables = self.__layout_context.GetLocationVariables()
        loc, _, clock = lmgr.getLocationInfo(loc_str, vars)
        t_off_property = cast(int, e.GetProperty('t_offset', period = pair.GetClockPeriod()))

        # Warn about invalid locations for content types which DO require transactions
        if loc == lmgr.INVALID_LOCATION_ID and e.GetProperty('Content') not in content.NO_TRANSACTIONS_REQUIRED:
            print('Warning: No collected location matching "{}" (using variables:{})' \
                                 .format(loc_str, variables), file = sys.stderr)

        if clock == self.__handle.database.location_manager.NO_CLOCK:
            # Makes the assumption that there will always be something else at
            # t_offset of 0. If not, then this could stand to be optimized
            t_off = self.__DEFAULT_T_OFF
            period = -1
        else:
            period = self.__handle.database.clock_manager.getClockDomain(clock).tick_period

            t_off = period * t_off_property
            pair.SetClockPeriod(period)

        #import pdb; pdb.set_trace()

        if e.GetQueryFrame(period):
            self.__range_pairs.append(pair)
        else:
            if self.GetID(pair) == -1 and e.GetProperty('Content') not in content.NO_DATABASE_REQUIRED:
                pair.SetMissingLocation()
            else:
                pair.SetVal('')

            if t_off in self.__t_off_sorted:
                self.__t_off_sorted[t_off] = self.__AddAtLoc(pair, self.__t_off_sorted[t_off])
            else:
                self.__t_off_sorted[t_off] = self.__AddAtLoc(pair)

        # Update this pair to indicate that it was added with this this t_off and location
        # This will be recalled when deleting this pair
        pair.SetLocationAndTimingInformation(t_off_property, lmgr.getLocationString(loc))

    # # Helper method to AddPair()
    # @profile
    def __AddAtLoc(self, pair: Element_Value, sub_dict: Optional[Dict[int, List[Element_Value]]] = None) -> Dict[int, List[Element_Value]]:
        if sub_dict:
            if self.GetID(pair) in sub_dict:
                if pair not in sub_dict[self.GetID(pair)]:
                    sub_dict[self.GetID(pair)].append(pair)
            else:
                sub_dict[self.GetID(pair)] = [pair]
            return sub_dict
        else:
            return {self.GetID(pair):[pair]}

    # # Used for re-sorting an Element's location within t_off_sorted{},
    #  before the Element's Properties have actually changed
    def __ForceAddSingleQueryPair(self, pair: Element_Value, t_off_in: int, id: int) -> None:
        e = pair.GetElement()

        # Recompute t_off in terms of plain HC's
        ####clock = self.GetClock(pair)
        lmgr = self.__layout_context.dbhandle.database.location_manager
        loc_str = e.GetProperty('LocationString')
        loc, _, clock = lmgr.getLocationInfo(loc_str, self.__layout_context.GetLocationVariables())

        if clock == self.__handle.database.location_manager.NO_CLOCK:
            # Makes the assumption that there will always be something else at
            # t_offset of 0. If not, then this could stand to be optimized
            t_off = self.__DEFAULT_T_OFF
        else:
            period = self.__handle.database.clock_manager.getClockDomain(clock).tick_period
            t_off = period * t_off_in
            pair.SetClockPeriod(period)

        if id == -1 and e.GetProperty('Content') not in content.NO_DATABASE_REQUIRED:
            pair.SetMissingLocation()
        else:
            pair.SetVal('')

        if t_off in self.__t_off_sorted:
            self.__t_off_sorted[t_off] = self.__ForceAddAtLoc(pair, id, self.__t_off_sorted[t_off])
        else:
            self.__t_off_sorted[t_off] = self.__ForceAddAtLoc(pair, id)

        # Update this pair to indicate that it was added with this this t_off and location
        # This will be recalled when deleting this pair
        pair.SetLocationAndTimingInformation(t_off_in, lmgr.getLocationString(loc))

    # # Helper method to __ForceAddSingleQueryPair()
    def __ForceAddAtLoc(self, pair: Element_Value, id: int, sub_dict: Optional[Dict[int, List[Element_Value]]] = None) -> Dict[int, List[Element_Value]]:
        if sub_dict:
            if id in sub_dict:
                if pair not in sub_dict[id]:
                    sub_dict[id].append(pair)
            else:
                sub_dict[id] = [pair]
            return sub_dict
        else:
            return {id:[pair]}

    # # Removes the Element-Value associated with the provided Element from
    #  both draw_order and the t_off_sorted, without leaving lose ends.
    #  @param pair Element_Value pair. The element in this pair must not have
    #  had its location or t_offset changed since it was added, otherwise it will
    #  not be found in the expecected __t_offset_sorted bucket.
    def DeletePair(self, pair: Element_Value) -> None:
        # In the case of Resorting an Element in t_off_sorted, draw order
        # delete range pair_entry if we have one
        e = pair.GetElement()

        # Get the properties related to rendering/sorting at the time this pair was added
        prev_locstr = pair.GetDisplayLocationString() # get the fully-resolved (no variables) location string
        prev_t_off = pair.GetDisplayTOffset()

        if e.GetQueryFrame(pair.GetClockPeriod()):
            for r_pair in self.__range_pairs:
                if r_pair == pair:
                    self.__range_pairs.remove(pair)
                    break
        else:
            # Recompute t_off in terms of plain HC's
            # #clock = self.GetClock(pair)
            lmgr = self.__layout_context.dbhandle.database.location_manager
            if prev_locstr is not None:
                loc, _, clock = lmgr.getLocationInfo(prev_locstr, {})
            else:
                loc = lmgr.INVALID_LOCATION_ID
                clock = lmgr.NO_CLOCK
            ####t_off = e.GetProperty('t_offset')

            if clock == lmgr.NO_CLOCK:
                # Makes the assumption that there will always be something else at
                # t_offset of 0. If not, then this could stand to be optimized
                t_off = self.__DEFAULT_T_OFF
            else:
                t_off = self.__handle.database.clock_manager.getClockDomain(clock).tick_period * prev_t_off

            # Note that we could ignore missing t_offs here, but then we might
            # have stale links in another t_off bucket. This guarantees that the
            # the proper pair was removed by requiring it to be in the expected
            # bucket
            temp = self.__t_off_sorted[t_off].get(loc)
            if not temp:
                return
            for p in temp:
                if p == e:
                    temp.remove(p)
            if len(self.__t_off_sorted[t_off][loc]) == 0:
                del self.__t_off_sorted[t_off][loc]
            if len(self.__t_off_sorted[t_off]) == 0:
                del self.__t_off_sorted[t_off]

    def CheckLocationVariablesChanged(self) -> bool:
        loc_vars_status = self.__layout_context.GetLocationVariablesChanged()
        if loc_vars_status:
           self.__layout_context.AckLocationVariablesChanged()
        return loc_vars_status

    # # Returns the internal ID which maps to the given Element's Location
    #  String, per the Location Manager
    def GetID(self, pair: Element_Value) -> int:
        el = pair.GetElement()
        if not el.LocationHasVars():
            return self.__layout_context.dbhandle.database.location_manager.getLocationInfoNoVars(el.GetProperty('LocationString'))[0]
        else:
            return self.__layout_context.dbhandle.database.location_manager.getLocationInfo(el.GetProperty('LocationString'), self.__layout_context.GetLocationVariables(), self.CheckLocationVariablesChanged())[0]

    # # Returns the clock ID which maps to the given' Element's location
    #  string, per the Location Manager
    def GetClock(self, pair: Element_Value) -> int:
        el = pair.GetElement()
        if el.LocationHasVars():
            return self.__layout_context.dbhandle.database.location_manager.getLocationInfo(el.GetProperty('LocationString'), self.__layout_context.GetLocationVariables(), self.CheckLocationVariablesChanged())[2]
        else:
            return self.__layout_context.dbhandle.database.location_manager.getLocationInfoNoVars(el.GetProperty('LocationString'))[2]

    # # When an element has it's LocationString (therefore LocationID) or
    #  it's t_offset changed, it needs to be resorted in the
    #  dictionary. This method is called, and executes, BEFORE the new
    #  property is assigned to the Element
    def ReSort(self, pair: Element_Value, t_off: int, id: int) -> None:
        self.DeletePair(pair)
        self.__ForceAddSingleQueryPair(pair, t_off, id)

    # # Update the val of an Element Value when the Element's 'Content'
    #  property is changed
    def ReValue(self, pair: Element_Value) -> None:
        e = pair.GetElement()
        if e.GetQueryFrame(pair.GetClockPeriod()):
            pair.ClearTimedValues()
            self.__layout_context.GoToHC(self.__layout_context.hc)
        else:
            if e.GetProperty('Content') in content.NO_TRANSACTIONS_REQUIRED:
                # Recompute t_off in terms of plain HC's
                clock = self.GetClock(pair)
                t_off = cast(int, e.GetProperty('t_offset'))
                if clock == self.__handle.database.location_manager.NO_CLOCK:
                    # Makes the assumption that there will always be something else at
                    # t_offset of 0. If not, then this could stand to be optimized
                    t_off = self.__DEFAULT_T_OFF
                else:
                    t_off = self.__handle.database.clock_manager.getClockDomain(clock).tick_period * t_off
                temp = self.__t_off_sorted[t_off][self.GetID(pair)]
                for pair_tmp in temp:
                    if pair_tmp == e:
                        pair.SetVal(content.ProcessContent(cast(str, e.GetProperty('Content')),
                                                           None,
                                                           e,
                                                           self.__handle,
                                                           self.__layout_context.hc,
                                                           self.__layout_context.GetLocationVariables()),
                                    self.__stab_index,
                                    )
                        return
            else:
                self.__layout_context.GoToHC(self.__layout_context.hc)

    # @profile
    def Update(self) -> None:
        '''
        This is where all Element Values get re-synchronized to the current hc
        not sure if everything here is final, or best implemented
        '''
        self.__stab_index = self.__stab_index + 1

        # Cached variable lookups
        no_trans = content.NO_TRANSACTIONS_REQUIRED
        stab_index = self.__stab_index
        handle = self.__handle
        hc = self.__layout_context.hc
        loc_vars = self.__layout_context.GetLocationVariables()

        ordered_ticks = [hc + toff for toff in self.__t_off_sorted]
        # Clear all continued transactions so that we don't accidentally draw garbage
        self.__continued_transactions.clear()
        # add intermediate values to make sure  Line-type  elements have what they need
        bottom_of_pair = 100000000000000000
        top_of_pair = -100000000000
        for pair in self.__range_pairs:
            e = pair.GetElement()
            assert isinstance(e, ScheduleLineElement)
            e.SetTime(hc) # Always set time because it is used for drawing the schedule group
            period = pair.GetClockPeriod()
            if period == -1:
                # unset/invalid
                continue
            qframe = e.GetQueryFrame(period)
            # #print 'QUERY FRAME @ hc={} FOR {} = {}. Period = {}'.format(hc, e, qframe, period)
            curr_time = qframe[0] + hc
            end_time = qframe[1] + hc
            curr_time = curr_time - curr_time % period
            end_time = end_time - end_time % period
            while curr_time <= end_time:
                timed_val = pair.GetTimedVal(curr_time)
                if not timed_val or not timed_val[0]:
                    ordered_ticks.append(curr_time)
                    if curr_time > top_of_pair:
                        top_of_pair = curr_time
                    if curr_time < bottom_of_pair:
                        bottom_of_pair = curr_time

                curr_time += period
        if len(ordered_ticks) == 0:
            return # Nothing to update

        ordered_ticks = sorted(set(ordered_ticks))

        next_tick_idx = [0]
        total_callbacks = [0]
        total_useful_callbacks = [0]
        total_updates = [0]

        # @profile
        def callback(t: int, tapi: TransactionDatabase) -> None:
            total_callbacks[0] += 1
            next_tick = next_tick_idx[0]
            if len(ordered_ticks) == 0:
                return
            next_t = ordered_ticks[next_tick]

            # Show tick info
            # #print 'On t=', t, ' @idx ', next_tick
            # #print'  next t=', next_t

            if t < next_t:
                # print ' ignored callback at t={}. t < next_t ({})'.format(t, next_t)
                return # Ignore this t because there is no entry in ordered_ticks
#            #in future, do this for all clocks.
#            if t % period != 0:
#                return
            next_tick_idx[0] += 1
            total_useful_callbacks[0] += 1

            # get data for range_pairs
            updated = 0
            GetID = self.GetID
            for range_pair_idx, range_pair in enumerate(self.__range_pairs):
                period = range_pair.GetClockPeriod()
                if period == -1:
                    # unset/invalid
                    continue
                e = range_pair.GetElement()
                frame = e.GetQueryFrame(period)
                assert frame is not None
                tick_start = frame[0] + self.__layout_context.hc
                tick_end = frame[1] + self.__layout_context.hc
                if tick_start <= t <= tick_end:
                    timed_val = range_pair.GetTimedVal(t)
                    if not timed_val or not timed_val[0]:
                        updated += 1
                        loc_id = GetID(range_pair)
                        content_type = cast(str, e.GetProperty('Content'))

                        # Update element content based on transaction
                        # If there is no data for this tick, this will return None
                        trans_proxy = self.__layout_context.dbhandle.api.getTransactionProxy(loc_id)

                        if trans_proxy is not None and trans_proxy.isValid():
                            if range_pair_idx in self.__continued_transactions:
                                old_interval, _, last = self.__continued_transactions[range_pair_idx].unwrap()
                                if last and t >= old_interval[1]:
                                    del self.__continued_transactions[range_pair_idx]
                            if range_pair_idx in self.__continued_transactions:
                                old_interval, old_processed_val, last = self.__continued_transactions[range_pair_idx].unwrap()
                                old_left = old_interval[0]
                                new_interval = (old_left, trans_proxy.getRight())
                                # Fix for ARGOS-158/ARGOS-164
                                # There's a corner case where a heartbeat occurs in the middle of a clock period. We would ordinarily skip over it, and
                                # consequently miss the last part of a continued transaction. If a continued transaction ends before the next clock period begins,
                                # we add it to the ordered_ticks list so that we can catch the next part of it.
                                if new_interval[1] < new_interval[0] + period:
                                    ordered_ticks.insert(next_tick_idx[0], new_interval[1])
                                self.__range_pairs[range_pair_idx].SetTimedVal(old_left, (old_processed_val, new_interval))
                                if not trans_proxy.isContinued():
                                    self.__continued_transactions[range_pair_idx].interval = new_interval
                                    self.__continued_transactions[range_pair_idx].last = True

                            else:
                                processed_val = content.ProcessContent(content_type,
                                                                       trans_proxy,
                                                                       e,
                                                                       handle,
                                                                       hc,
                                                                       loc_vars)
                                interval = (trans_proxy.getLeft(), trans_proxy.getRight())
                                if trans_proxy.isContinued():
                                    self.__continued_transactions[range_pair_idx] = ContinuedTransaction(interval, copy.copy(processed_val), False)
                                    # Fix for ARGOS-158/ARGOS-164
                                    # There's a corner case where a heartbeat occurs in the middle of a clock period. We would ordinarily skip over it, and
                                    # consequently miss the last part of a continued transaction. If a continued transaction ends before the next clock period begins,
                                    # we add it to the ordered_ticks list so that we can catch the next part of it.
                                    if interval[1] < interval[0] + period:
                                        ordered_ticks.insert(next_tick_idx[0], interval[1])
                                else:
                                    range_pair.SetTimedVal(interval[0], (processed_val, interval))
                                    if trans_proxy.getLeft() != t:
                                        if t % period == 0:
                                            original_start = trans_proxy.getLeft()
                                            range_pair.SetTimedVal(t, (original_start, (t, t))) # placeholder
                                            if not range_pair.GetTimedVal(original_start):
                                                info('Unable to make full query.')

                        else:
                            if t % period == 0:
                                range_pair.SetTimedVal(t, (None, (t, t))) # placeholder

            # Query at this time and update all elements for which a transaction
            # exists.
            # assert t-hc in self.__t_off_sorted, 'bad tick {0}'.format(t)
            if t - hc in self.__t_off_sorted:
                ids = self.__t_off_sorted[t - hc] # #.keys()
                # #print 'IDs @ {0} = {1}'.format(t, ids)

                # Dump all locations in a row with locaiton transacitons IDs coded into ascii
                # #for locid in xrange(0, self.__layout_context.dbhandle.database.location_manager.getMaxLocationID()):
                # #    trans_proxy = self.__layout_context.dbhandle.api.getTransactionProxy(locid)
                # #    if trans_proxy is not None and trans_proxy.isValid():
                # #        sys.stdout.write('{:1s}'.format(chr((0x21 + trans_proxy.getTransactionID())
                # #                                            % (ord('~') - ord('!'))
                # #                                            )))
                # #    else:
                # #        sys.stdout.write('_')
                # #print ''

                for loc_id, els in self.__t_off_sorted[t - hc].items():
                    for pair in els:
                        e = pair.GetElement()

                        content_type = cast(str, e.GetProperty('Content'))
                        if content_type in no_trans:
                            # Update this element, which is not dependent on a transaction
                            pair.SetVal(content.ProcessContent(content_type,
                                                               None,
                                                               e,
                                                               handle,
                                                               hc,
                                                               loc_vars),
                                        stab_index)
                        else:
                            # Update element content based on transaction
                            # If there is no data for this tick, this will return None
                            trans_proxy = self.__layout_context.dbhandle.api.getTransactionProxy(loc_id)
                            if loc_id == -1:
                                pair.SetMissingLocation()
                            elif trans_proxy is not None and trans_proxy.isValid():
                                pair.SetVal(content.ProcessContent(content_type,
                                                                   trans_proxy,
                                                                   e,
                                                                   handle,
                                                                   hc,
                                                                   loc_vars),
                                                                   stab_index)
                            else:
                                # There is no transaction here. It might be a fake query response or
                                # a genuine empty transaction.
                                # If previously, there was no transaction at this location,
                                # assume this is still the case. If an element changes locations
                                # and then points to a location that is valid but has no transaction
                                # it is the responsibility of AddElement-related methods to clear
                                # the 'no location' value so that it doesn't persist
                                if pair.GetVal() is not content.OverrideState('no loc'):
                                    pair.SetVal(content.OverrideState('no trans'))

                        updated += 1
                total_updates[0] += updated

        logging.debug('Querying from {} to {}'.format(ordered_ticks[0], ordered_ticks[-1]))
        t_start = time.monotonic()
        try:
            self.__layout_context.dbhandle.query(ordered_ticks[0], ordered_ticks[-1], callback, True)
            logging.debug("Done with db query")
        except Exception as ex:
            logging.debug('Exception while querying!: {}'.format(ex))
            raise
        finally:
            logging.debug('{0}s: Query+Update for {1} elements. {2} callbacks ({3} useful)' \
                          .format(time.monotonic() - t_start, total_updates[0], total_callbacks[0], total_useful_callbacks[0]))
            logging.debug('  {}'.format(self.__layout_context.dbhandle.api))
            node_states = self.__layout_context.dbhandle.api.getNodeStates().decode('utf-8').split('\n')
            for ns in node_states:
                logging.debug('  {}'.format(ns))

        logging.debug('Done')

        # print 'Node 0 dump:\n'
        # print self.__layout_context.dbhandle.api.getNodeDump(0, 890, 905, 40);

    # # For debug purposes
    def __repr__(self) -> str:
        return self.__str__()

    def __str__(self) -> str:
        return '<Ordered_Dict>'.format()

    def GetElementDump(self) -> str:
        res = ''
        for t_off in self.__t_off_sorted:
            res += str(t_off) + '\t'
            for loc in self.__t_off_sorted[t_off]:
                res += str(loc) + '\t'
                for e in self.__t_off_sorted[t_off][loc]:
                    res += repr(e) + ', '
                res += '\n\t'
            res += '\n'
        return res
