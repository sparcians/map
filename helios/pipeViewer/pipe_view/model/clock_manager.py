

## @package clock_manager.py
#  @brief Consumes argos clock files through ClockManager class


import sys
import math
from typing import Dict, List, Optional, TextIO, Tuple

## Consumes an Argos clock file and provides a means of lookup up clock info
#  via clock IDs
#
#  Also allows browsing of availble clocks
class ClockManager:

    ## Clock domain owned by a ClockManager
    class ClockDomain:

        def __init__(self,
                     clk_id: int,
                     clk_name: str,
                     hc_tick_period: int,
                     hc_ratio_num: int,
                     hc_ratio_denom: int) -> None:
            self.__clk_id = clk_id
            self.__clk_name = clk_name
            self.__hc_tick_period = hc_tick_period
            self.__hc_ratio_num = hc_ratio_num
            self.__hc_ratio_denom = hc_ratio_denom

        def __str__(self) -> str:
            return '<ClockDomain id={0} name="{1}" per={2}, ratio={3}/{4}>' \
                   .format(self.__clk_id, self.__clk_name, self.__hc_tick_period,
                           self.__hc_ratio_num, self.__hc_ratio_denom)

        def __repr__(self) -> str:
            return self.__str__()

        @property
        def ID(self) -> int:
            return self.__clk_id

        @property
        def name(self) -> str:
            return self.__clk_name

        ## Period of this clock in hypercycle ticks
        @property
        def tick_period(self) -> int:
            return self.__hc_tick_period

        ## Convert a cycle in terms of this clock domain into a hypercycle tick
        #  count
        #  @note The input to this should never be used as a delta and the
        #  output should not be used as a relative cycle because inaccuracies
        #  can accumulate.
        #  @param local_cycle Cycle number on the domain described by this clock
        #  @return integer number of hypercycle ticks elapsed up to
        #  \a local_cycle. This is ABSOLUTE cycles. Relative cycles are computed
        #  differently
        def LocalToHypercycle(self, local_cycle: int) -> int:
            assert local_cycle is not None
            # Absolute cycle counts begin at 0.
            return int(math.floor((local_cycle) * self.__hc_tick_period))

        ## Convert a cycle in terms of hypercycle ticks into number of cycles on
        #  this clock domain.
        #  @note The input to this should never be used as a delta and the
        #  output should not be used as a relative cycle because inaccuracies
        #  can accumulate.
        #  @param hc Hypercycle tick count.
        #  @return integer number of cycles on this clock domain
        def HypercycleToLocal(self, hc: int) -> int:
            assert hc is not None
            # Absolute cycle counts begin at 0.
            return int(math.floor(hc / self.__hc_tick_period))

        ## With a given hypercycle, compute a hypercycle that represents the
        #  next local cycle.
        #  @param hc Hypercycle to use for computation
        #  @return hypercycle representing (local cycle) where local_cycle
        #  is computed by the \a hc argument.
        #
        #  Implemented as LocalToHypercycle(HypercycleToLocal(hc))
        def NextLocalCycle(self, hc: int) -> int:
            assert hc is not None
            return self.LocalToHypercycle(self.HypercycleToLocal(hc))


    ## Describes the clock file
    CLOCK_FILE_EXTENSION = 'clock.dat'

    ## Expeted version number from file. Otherwise the data cannot be consumed
    VERSION_NUMBER = 1

    ## ID associated with built-in hypercycle clock
    HYPERCYCLE_CLOCK_ID = 0

    ## Name associated with built-in hypercycle clock
    HYPERCYCLE_CLOCK_NAME = 'ticks'

    ## Constructor
    #  @param prefix Argos transaction database prefix to open.
    #  CLOCK_FILE_EXTENSION will be appended to determine the actual
    #  filename to open
    #  @note Prevents duplicate names
    def __init__(self, prefix: str) -> None:

        self.__clocks: Dict[int, ClockManager.ClockDomain] = {} # { ClockID : ClockDomain }
        self.__clock_names: Dict[str, int] = {} # { Clock name : ClockID }
        self.__clock_list: List[ClockManager.ClockDomain] = [] # [ClockDomain0, ClockDomain1, ... ]
        self.__hc_frequency = 0 # Frequency of hypercycle ticks in actual Hz

        with open(prefix + self.CLOCK_FILE_EXTENSION, 'r') as f:

            # Find version information
            while 1:
                first = self.__findNextLine(f)
                assert first is not None
                if first == '':
                    continue

                try:
                    els = first.split(' \t#')
                    ver = int(els[0])
                except:
                    raise ValueError('Found an unparsable (non-integer) version number string: "{0}". Expected "{1}".' \
                                     .format(first, self.VERSION_NUMBER))

                if ver != self.VERSION_NUMBER:
                    raise ValueError('Found incorrect version number: "{0}". Expected "{1}". This reader may need to be updated' \
                                     .format(ver, self.VERSION_NUMBER))
                break

            # Find hypercycle tick frequency information
            # <hc_tick_freq>
            while 1:
                first = self.__findNextLine(f)
                assert first is not None
                if first == '':
                    continue

                try:
                    self.__hc_frequency = int(first)
                except:
                    raise ValueError('Found an unparsable (non-integer) frequency string: "{0}".' \
                                     .format(first))

                self.__addClockDomain(self.HYPERCYCLE_CLOCK_ID, self.HYPERCYCLE_CLOCK_NAME, 1, 1, 1)
                break

            # Read subsequent location lines
            # <clock_uid_int>,<clock_name>,<period_in_hc_ticks>,<clock_ratio_numerator>,<clock_ration_denominator>\n
            while 1:
                ln = self.__findNextLine(f)
                if ln == '':
                    continue
                if ln is None:
                    break

                els = ln.split(',')

                try:
                    uid_str, name, period_str, rat_num_str, rat_denom_str = els[:5]
                except:
                    raise ValueError('Failed to parse line "{0}"'.format(ln))

                uid = int(uid_str)
                period = int(period_str)
                rat_num = int(rat_num_str)
                rat_denom = int(rat_denom_str)

                self.__addClockDomain(uid, name, period, rat_num, rat_denom)

    ## Checks if a ClockDomain object with the given name exists.
    #  @param clock_name Name of clock to lookup.
    #  @return Whether clock name is present
    def doesClockNameExist(self, clock_name: str) -> bool:
        if not isinstance(clock_name, str):
            raise TypeError('clock_name must be a string, is a {0}'.format(type(clock_name)))
        return clock_name in self.__clock_names

    ## Gets a ClockDomain object with the given name.
    #  @param clock_name Name of clock to lookup.
    #  @throw KeyError if name is not found in this manager.
    #  @return ClockDomain object
    def getClockDomainByName(self, clock_name: str) -> ClockManager.ClockDomain:
        if not isinstance(clock_name, str):
            raise TypeError('clock_name must be a string, is a {0}'.format(type(clock_name)))
        return self.getClockDomain(self.__clock_names[clock_name]) # Throw KeyError if not found

    ## Gets a ClockDomain object associated with the given clock ID.
    #  @param clock_id ID of clock to lookup.
    #  @throw KeyError if ID is not found in this manager.
    #  @return ClockDomain object
    def getClockDomain(self, clock_id: int) -> ClockManager.ClockDomain:
        if not isinstance(clock_id, int):
            raise TypeError('clock_id must be an integer, is a {0}'.format(type(clock_id)))
        return self.__clocks[clock_id] # Throw KeyError if not found

    ## Gets sequence of all clock domains in no particular order
    #  @note This is slow because this list is generated on request
    #  @note Order of results is guaranteed to be consistent
    #  @return List of ClockDomain instances representing all known
    #  clock domains
    def getClocks(self) -> Tuple[ClockManager.ClockDomain, ...]:
        return tuple(self.__clock_list) # Convert to tuple to prevent modification to internal list

    ## Of all clocks, which is closest to our current time
    def getClosestClock(self, hc: int, clocks: List[int], forward: bool = True) -> ClockManager.ClockDomain:
        if not len(clocks):
            return self.__clock_list[0]
        closest_clock = self.__clocks[clocks[0]]
        if not forward:
            hc-=1
        min_divergence = hc % closest_clock.tick_period
        if forward:
            min_divergence = closest_clock.tick_period-min_divergence
        for clk_id in clocks:
            if clk_id == -1:
                continue
            clk = self.__clocks[clk_id]
            delta = hc % clk.tick_period
            if forward:
                delta = clk.tick_period-delta
            if delta < min_divergence:
                min_divergence = delta
                closest_clock = clk
        return closest_clock

    ## Adds a clock domain with given parameters.
    #  @note Refer to ClockDomain class for parameter semantics
    #  @throw If any clock domain arguments are unacceptable for ClockDomain or
    #  if \a clkname has already been added to this manager
    def __addClockDomain(self, clkid: int, clkname: str, hc_period: int, rat_num: int, rat_denom: int) -> None:
        if clkname in self.__clock_names:
            raise ValueError('clkname "{0}" is already present in this clock manager. It cannot be re-added' \
                             .format(clkname))
        cd = self.ClockDomain(clkid, clkname, hc_period, rat_num, rat_denom)
        self.__clock_names[clkname] = clkid
        self.__clocks[clkid] = cd
        self.__clock_list.append(cd)

    ## Gets next line from file which is not a comment.
    #  Strips comments on the line and whitespace from each end
    #  @param f File to read next line from
    #  @return '' if line is empty or comment, None if EOF is reached
    def __findNextLine(self, f: TextIO) -> Optional[str]:
        ln = f.readline().strip()
        if ln == '':
            return None

        if ln.find('#') == 0:
            return ''

        pos = ln.find('#')
        if pos >= 0:
            ln = ln[:pos]

            ln = ln.strip()

        return ln

    def __len__(self) -> int:
        return len(self.__clocks)

    def __str__(self) -> str:
        return '<ClockManager clocks={0}>'.format(len(self.__clocks))

    def __repr__(self) -> str:
        return self.__str__()

