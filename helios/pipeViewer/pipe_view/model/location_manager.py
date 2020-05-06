# # @package location_manager.py
#  @brief Consumes argos location files through LocationManager class

import functools
import re
import sys
import time

# # Expression for searching for variables in location strings
#  @note value can be empty string
LOCATION_STRING_VARIABLE_RE = re.compile('{(\w+)\s*(?:=\s*(\w*))?}')


# # Consumes an Argos location file and provides a means of lookup up location
#  IDs via location strings
#
#  Also allows browsing of availble locations
class LocationManager(object):

    # # Describes the location file
    LOCATION_FILE_EXTENSION = 'location.dat'

    # # Expeted version number from file. Otherwise the data cannot be consumed
    VERSION_NUMBER = 1

    # # Integer refering to no clock when seen as a clock ID in this location
    #  file
    NO_CLOCK = -1

    # # Integer refering to invalid location ID
    INVALID_LOCATION_ID = -1

    # # Tuple indicating that a location string was not found in the map when a
    #  location was queried through getLocationInfo
    LOC_NOT_FOUND = (INVALID_LOCATION_ID, '', NO_CLOCK)

    FILE_WAIT_TIMEOUT = 60

    # # Constructor
    #  @param prefix Argos transaction database prefix to open.
    #  LOCATION_FILE_EXTENSION will be appended to determine the actual
    #  filename to open
    def __init__(self, prefix, update_enabled):

        self.__locs = {} # { Location Name : ( LocationID, name, ClockID ) }
        self.__id_to_string = {} # { LocationID : Location Name }
        self.__loc_tree = {} # Location tree
        self.__regex_cache = {}
        try_count = self.FILE_WAIT_TIMEOUT

        index_file_exists = False

        while update_enabled and not index_file_exists:
            try:
                with open(prefix + self.LOCATION_FILE_EXTENSION) as f:
                    index_file_exists = True
            except IOError as e:
                if try_count == self.FILE_WAIT_TIMEOUT:
                    print("Index file", prefix + self.LOCATION_FILE_EXTENSION, "doesn't exist yet.")
                elif try_count == 0:
                    raise e
                print("Retrying:", self.FILE_WAIT_TIMEOUT - try_count)
                try_count -= 1
                time.sleep(1)

        with open(prefix + self.LOCATION_FILE_EXTENSION, 'r') as f:
            # Find version information
            while 1:
                first = self.__findNextLine(f)
                if first == '':
                    continue

                try:
                    els = first.split(' \t#')
                    ver = int(els[0])
                except:
                    if update_enabled and (not first and try_count > 0):
                        print("Could not read version string. File may be in-progress and not flushed yet. Retrying:", self.FILE_WAIT_TIMEOUT - try_count)
                        try_count -= 1
                        f.seek(0)
                        time.sleep(1)
                        continue
                    else:
                        raise ValueError('Found an unparsable (non-integer) version number string: "{0}". Expected "{1}".' \
                                     .format(first, self.VERSION_NUMBER))

                if ver != self.VERSION_NUMBER:
                    raise ValueError('Found incorrect version number: "{0}". Expected "{1}". This reader may need to be updated' \
                                     .format(ver, self.VERSION_NUMBER))
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
                    uid, name, clockid = els[:3]
                except:
                    raise ValueError('Failed to parse line "{0}"'.format(ln))

                uid = int(uid)
                clockid = int(clockid)

                self.__insertLocationInTree(name)
                self.__locs[name] = (uid, name, clockid)
                self.__id_to_string[uid] = name

    # # Gets a tree of location strings represented as a dictionary with keys equal to
    #  objects between dots in location strings: Leaf nodes are empty dictionaries
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
    def location_tree(self):
        return self.__loc_tree

    # # Gets next line from file which is not a comment.
    #  Strips comments on the line and whitespace from each end
    #  @param f File to read next line from
    #  @return '' if line is empty or comment, None if EOF is reached
    def __findNextLine(self, f):
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

    # # Gets a tuple containing location information associated with a location
    #  string
    #  @param locname Location name string to lookup
    #  @param variables dict of variables that can be substituted into the locname
    #  @return 3-tuple (locationID, location name, clock id) if found. If not
    #  found, returns LOC_NOT_FOUND
    def getLocationInfo(self, locname, variables, loc_vars_changed = False):
        if not isinstance(locname, str):
            raise TypeError('locname must be a str, is a {0}'.format(type(locname)))

        resolved_loc = self.replaceLocationVariables(locname, variables, loc_vars_changed)
        try:
            return self.__locs[resolved_loc]
        except:
            return self.LOC_NOT_FOUND

    # # Gets a tuple containing location information associated with a location
    #  string
    #  @param locname Location name string to lookup
    #  @return 3-tuple (locationID, location name, clock id) if found. If not
    #  found, returns LOC_NOT_FOUND
    def getLocationInfoNoVars(self, locname):
        if not isinstance(locname, str):
            raise TypeError('locname must be a str, is a {0}'.format(type(locname)))
        try:
            return self.__locs[locname]
        except:
            return self.LOC_NOT_FOUND

    # # Gets a location string with variables in it replaced by the given
    #  variables dictionary. Caches results to improve performance for repeated calls.
    #  @param locname Location string
    #  @param variables dict of variables and values
    #  @param loc_vars_changed True if we need to refresh the entry in the regex cache
    def replaceLocationVariables(self, locname, variables, loc_vars_changed = False):
        if loc_vars_changed:
            self.__regex_cache[locname] = LOCATION_STRING_VARIABLE_RE.sub(functools.partial(self.__replaceVariable, variables), locname)
        try:
            return self.__regex_cache[locname]
        except:
            self.__regex_cache[locname] = LOCATION_STRING_VARIABLE_RE.sub(functools.partial(self.__replaceVariable, variables), locname)
            return self.__regex_cache[locname]

    # # Find all location variables in location string
    #  Returns a list of tuples (variable name, value) representing each variable found
    @staticmethod
    def findLocationVariables(locname):
        if not isinstance(locname, str):
            raise TypeError('locname must be a str, is a {0}'.format(type(locname)))

        found = []
        matches = LOCATION_STRING_VARIABLE_RE.findall(locname)
        for m in matches:
            found.append((m[0], m[1]))

        return found

    # # Gets location srings in no particular order
    #  @note This is slow because this list is generated on request
    #  @note Order of results is not necessarily consistent
    #  @return List of str instances containing names of all known locations
    def getLocationStrings(self):
        return [x[1] for x in list(self.__locs.values())]

    # # Gets location strings for the given locid
    #  @param locid Location ID to lookup a string for
    #  @return Location string (str) if found, None otherwise
    def getLocationString(self, locid):
        return self.__id_to_string.get(locid, None)

    # # Gets a sequence of potential location string completions given the
    #  input \a locname
    #  @param locname Location name string input
    #  @warning This is a very slow method since it iterates the entire list
    #  @todo Use an internal tree of location names (between '.'s) for
    #  generating completions more quickly
    def getLocationStringCompletions(self, locname):
        results = []
        for k, v in self.__locs.items():
            _, n, _ = v
            if locname == n[:len(locname)]:
                results.append(n[len(locname):])

        return results

    # # Returns the maximum location ID known to this manager
    def getMaxLocationID(self):
        if len(self.__id_to_string) == 0:
            return 0
        return max(self.__id_to_string.keys())

    def __len__(self):
        return len(self.__locs)

    def __str__(self):
        return '<LocationManager locs={0}>'.format(len(self.__locs))

    def __repr__(self):
        return self.__str__()

    def __insertLocationInTree(self, loc_name):
        '''
        Parses a location name by '.' and builds a tree from its content
        '''
        parent = self.__loc_tree
        paths = loc_name.split('.')
        for obj in paths:
            if parent.get(obj) is None:
                parent[obj] = {}
            parent = parent[obj]

    # # Handles replacing a word in a string with
    #  @param variables dict of variable names with values
    #  @param match Match regex result. Contains 2 groups: the variable name and default value
    #  (which is None if no default was supplied)
    def __replaceVariable(self, variables, match):
        # Get the replacement form the variables dictionary
        replacement = variables.get(match.group(1), None)
        if replacement is None:
            if match.group(1) is not None:
                # Use the default value associated with with variable in the location string
                replacement = match.group(2)
                # #print 'Used default {} for {}. Variables were {}'.format(match.group(2), match.group(1), variables)
            else:
                return '<undefined:{}!>'.format(match.group(0))
        else:
            pass # #print 'Used variable for {}'.format(match.group(1))
        return replacement

