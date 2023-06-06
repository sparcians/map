# @package location_manager.py
#  @brief Consumes argos location files through LocationManager class

from __future__ import annotations
import functools
import re
import time
from typing import Dict, List, Optional, TextIO, Tuple

# Expression for searching for variables in location strings
#  @note value can be empty string
LOCATION_STRING_VARIABLE_RE = re.compile(r'{(\w+)\s*(?:=\s*(\w*))?}')

LocationTree = Dict[str, 'LocationTree']
LocationType = Tuple[int, str, int]
__LocationDict = Dict[str, LocationType]


# Consumes an Argos location file and provides a means of lookup up location
#  IDs via location strings
#
#  Also allows browsing of availble locations
class LocationManager:

    # Describes the location file
    LOCATION_FILE_EXTENSION = 'location.dat'

    # Expeted version number from file. Otherwise the data cannot be consumed
    VERSION_NUMBER = 1

    # Integer refering to no clock when seen as a clock ID in this location
    #  file
    NO_CLOCK = -1

    # Integer refering to invalid location ID
    INVALID_LOCATION_ID = -1

    # Tuple indicating that a location string was not found in the map when a
    #  location was queried through getLocationInfo
    LOC_NOT_FOUND = (INVALID_LOCATION_ID, '', NO_CLOCK)

    FILE_WAIT_TIMEOUT = 60

    # Constructor
    #  @param prefix Argos transaction database prefix to open.
    #  LOCATION_FILE_EXTENSION will be appended to determine the actual
    #  filename to open
    def __init__(self, prefix: str, update_enabled: bool) -> None:
        # { Location Name : ( LocationID, name, ClockID ) }
        self.__locs: __LocationDict = {}
        self.__id_to_string = {}  # { LocationID : Location Name }
        self.__loc_tree: LocationTree = {}  # Location tree
        self.__regex_cache: Dict[str, str] = {}
        try_count = self.FILE_WAIT_TIMEOUT

        index_file_exists = False

        filename = prefix + self.LOCATION_FILE_EXTENSION
        while update_enabled and not index_file_exists:
            try:
                with open(filename) as f:
                    index_file_exists = True
            except IOError as e:
                if try_count == self.FILE_WAIT_TIMEOUT:
                    print(f"Index file{filename} doesn't exist yet.")
                elif try_count == 0:
                    raise e
                print("Retrying:", self.FILE_WAIT_TIMEOUT - try_count)
                try_count -= 1
                time.sleep(1)

        with open(filename, 'r') as f:
            # Find version information
            while 1:
                first = self.__findNextLine(f)
                assert first is not None
                if first == '':
                    continue

                try:
                    els = first.split(' \t#')
                    ver = int(els[0])
                except ValueError:
                    if update_enabled and (not first and try_count > 0):
                        print("Could not read version string. File may be "
                              "in-progress and not flushed yet. Retrying: "
                              f"{self.FILE_WAIT_TIMEOUT - try_count}")
                        try_count -= 1
                        f.seek(0)
                        time.sleep(1)
                        continue
                    else:
                        raise ValueError('Found an unparsable (non-integer) '
                                         f'version number string: "{first}". '
                                         f'Expected "{self.VERSION_NUMBER}".')

                if ver != self.VERSION_NUMBER:
                    raise ValueError('Found incorrect version number: '
                                     f'"{ver}". Expected '
                                     f'"{self.VERSION_NUMBER}". This reader '
                                     'may need to be updated')
                break

            # Read subsequent location lines
            # <node_uid_int>,<node_location_string>,<clock_uid_int>\n
            while 1:
                ln = self.__findNextLine(f)
                if ln == '':
                    continue
                if ln is None:
                    break

                els = ln.split(',')

                try:
                    uid_str, name, clockid_str = els[:3]
                    uid = int(uid_str)
                    clockid = int(clockid_str)
                except ValueError:
                    raise ValueError(f'Failed to parse line "{ln}"')

                self.__insertLocationInTree(name)
                self.__locs[name] = (uid, name, clockid)
                self.__id_to_string[uid] = name

    # Gets a tree of location strings represented as a dictionary with keys
    #  equal to objects between dots in location strings: Leaf nodes are empty
    #  dictionaries
    #
    #  Example:
    #  @code
    #  { 'top' : { 'core0': { 'fetch': {},
    #                         'alu0': {} },
    #              'core1': {},
    #            },
    #  }
    #  @endcode
    @property
    def location_tree(self) -> LocationTree:
        return self.__loc_tree

    # Gets next line from file which is not a comment.
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

    # Gets a tuple containing location information associated with a location
    #  string
    #  @param locname Location name string to lookup
    #  @param variables dict of variables that can be substituted into the
    #  locname
    #  @return 3-tuple (locationID, location name, clock id) if found. If not
    #  found, returns LOC_NOT_FOUND
    def getLocationInfo(self,
                        locname: str,
                        variables: Dict[str, str],
                        loc_vars_changed: bool = False) -> LocationType:
        if not isinstance(locname, str):
            raise TypeError(f'locname must be a str, is a {type(locname)}')

        resolved_loc = self.replaceLocationVariables(locname,
                                                     variables,
                                                     loc_vars_changed)
        try:
            return self.__locs[resolved_loc]
        except KeyError:
            return self.LOC_NOT_FOUND

    # Gets a tuple containing location information associated with a location
    #  string
    #  @param locname Location name string to lookup
    #  @return 3-tuple (locationID, location name, clock id) if found. If not
    #  found, returns LOC_NOT_FOUND
    def getLocationInfoNoVars(self, locname: str) -> LocationType:
        if not isinstance(locname, str):
            raise TypeError(f'locname must be a str, is a {type(locname)}')
        try:
            return self.__locs[locname]
        except KeyError:
            return self.LOC_NOT_FOUND

    # Gets a location string with variables in it replaced by the given
    #  variables dictionary. Caches results to improve performance for repeated
    #  calls.
    #  @param locname Location string
    #  @param variables dict of variables and values
    #  @param loc_vars_changed True if we need to refresh the entry in the
    #  regex cache
    def replaceLocationVariables(self,
                                 locname: str,
                                 variables: Dict[str, str],
                                 loc_vars_changed: bool = False) -> str:
        if loc_vars_changed:
            self.__regex_cache[locname] = LOCATION_STRING_VARIABLE_RE.sub(
                functools.partial(self.__replaceVariable, variables), locname
            )
        try:
            return self.__regex_cache[locname]
        except KeyError:
            self.__regex_cache[locname] = LOCATION_STRING_VARIABLE_RE.sub(
                functools.partial(self.__replaceVariable, variables), locname
            )
            return self.__regex_cache[locname]

    # Find all location variables in location string
    #  Returns a list of tuples (variable name, value) representing each
    #  variable found
    @staticmethod
    def findLocationVariables(locname: str) -> List[Tuple[str, str]]:
        if not isinstance(locname, str):
            raise TypeError(f'locname must be a str, is a {type(locname)}')

        found = []
        matches = LOCATION_STRING_VARIABLE_RE.findall(locname)
        for m in matches:
            found.append((m[0], m[1]))

        return found

    # Gets location srings in no particular order
    #  @note This is slow because this list is generated on request
    #  @note Order of results is not necessarily consistent
    #  @return List of str instances containing names of all known locations
    def getLocationStrings(self) -> List[str]:
        return [x[1] for x in self.__locs.values()]

    # Gets location strings for the given locid
    #  @param locid Location ID to lookup a string for
    #  @return Location string (str) if found, None otherwise
    def getLocationString(self, locid: int) -> Optional[str]:
        return self.__id_to_string.get(locid, None)

    # Gets a sequence of potential location string completions given the
    #  input \a locname
    #  @param locname Location name string input
    #  @warning This is a very slow method since it iterates the entire list
    #  @todo Use an internal tree of location names (between '.'s) for
    #  generating completions more quickly
    def getLocationStringCompletions(self, locname: str) -> List[str]:
        results = []
        for k, v in self.__locs.items():
            _, n, _ = v
            if locname == n[:len(locname)]:
                results.append(n[len(locname):])

        return results

    # Returns the maximum location ID known to this manager
    def getMaxLocationID(self) -> int:
        if len(self.__id_to_string) == 0:
            return 0
        return max(self.__id_to_string.keys())

    def __len__(self) -> int:
        return len(self.__locs)

    def __str__(self) -> str:
        return f'<LocationManager locs={len(self.__locs)}>'

    def __repr__(self) -> str:
        return self.__str__()

    def __insertLocationInTree(self, loc_name: str) -> None:
        '''
        Parses a location name by '.' and builds a tree from its content
        '''
        parent = self.__loc_tree
        paths = loc_name.split('.')
        for obj in paths:
            if obj not in parent:
                parent[obj] = {}
            parent = parent[obj]

    # Handles replacing a word in a string with
    #  @param variables dict of variable names with values
    #  @param match Match regex result. Contains 2 groups: the variable name
    #  and default value (which is None if no default was supplied)
    def __replaceVariable(self,
                          variables: Dict[str, str],
                          match: re.Match) -> str:
        # Get the replacement form the variables dictionary
        replacement = variables.get(match.group(1), None)
        if replacement is None:
            if match.group(1) is not None:
                # Use the default value associated with with variable in the
                # location string
                replacement = match.group(2)
            else:
                return f'<undefined:{match.group(0)}!>'
        else:
            pass
        return replacement
