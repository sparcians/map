# # @package database.py
#  @brief Consumes argos database files based on prefix

import os
import sys
import logging
from logging import warn, debug, info, error

from .location_manager import LocationManager
from .clock_manager import ClockManager

# Import Argos transaction database module from SPARTA
__MODULE_ENV_VAR_NAME = 'TRANSACTIONDB_MODULE_DIR'
env_var = os.environ.get(__MODULE_ENV_VAR_NAME)
if env_var == None:
    # Try to find the transaction module in the Helios release dir
    added_path = os.path.dirname(__file__) + "/../../../../release/helios/pipeViewer/transactiondb/lib"
    added_path = os.path.abspath(added_path)
    if not os.path.isdir(added_path):
        error('Argos cannot find the transactiondb directory: {0}'.format(added_path))
        sys.exit(1)
else:
    added_path = os.environ.get(__MODULE_ENV_VAR_NAME, os.getcwd())

sys.path.insert(0, added_path) # Add temporary search path
try:
    import transactiondb

except ImportError as e:
    error('Argos failed to import module: "transactiondb"')
    error(f'The search paths (sys.path) were: {", ".join(sys.path)}')
    error(f'Please export the environment variable {__MODULE_ENV_VAR_NAME} to ' \
          'contain the absolute path of the directory wherein the SPARTA ' \
          'transactiondb module can be found. Currently is "{added_path}"')
    error('Exception: {0}'.format(e))
    sys.exit(1)
finally:
    sys.path.remove(added_path) # Remove temporary path

# Inform users which transactiondb interface they are getting
info('Using transactiondb at "{}"'.format(transactiondb.__file__))


# # Consumes an Argos database and creates a location manager, a clock manager,
#  and a transaction database API
#
#  Also allows browsing of database static structure as well as creating handles
#  to database transaction info.
#
#  The static database structure exposed through this class and its children is
#  can be shared between many DatabaseHandle instances
class Database(object):

    # # Constructor
    #  @param prefix Argos transaction database prefix to open.
    #  Transaction database file extensions will be appended to determine the
    #  actual filenames to open
    def __init__(self, prefix, update_enabled):

        self.__filename = prefix
        self.__loc_mgr = LocationManager(prefix, update_enabled)
        self.__clk_mgr = ClockManager(prefix)
        # Number of stabbing query results to store at a time
        self.__cache_size = 20
        self.__cache = {}
        self.__hc_queue = []

        # stores extra data shared between objects across layout contexts using the same database
        self.__metadata = {}
        self.__metadata_tick = None

        # Note that this will need to move if multiple layout contexts access
        # the same database sporadically
        logging.getLogger('Database').debug('Database {} about to open query API'.format(self))
        self.__dbapi = transactiondb.TransactionDatabase(self.filename,
                                                          1 + self.location_manager.getMaxLocationID(), update_enabled)
        logging.getLogger('Database').debug('Database opened with node length {}, heartbeat size {}' \
                                            .format(self.__dbapi.getNodeLength(), self.__dbapi.getChunkSize()))

    # # Gets the database implementation module
    @property
    def dbmodule(self):
        return transactiondb

    # # Query API (TransactionDatabase)
    @property
    def api(self):
        return self.__dbapi

    # # ClockManager object containing all clocks in this database
    @property
    def clock_manager(self):
        return self.__clk_mgr

    # # LocationManager object containing all locations in this database
    @property
    def location_manager(self):
        return self.__loc_mgr

    #### Dictionary containing mappings of HC's to ISL Query results
    # #@property
    # #def cache(self):
    # #    return self.__cache.keys()
    # #
    #### Add a query result set to the cache, removing outdated content as necessary
    # #def Cache(self, hc, results):
    # #    self.__cache[hc] = results
    # #    self.__hc_queue.append(hc)
    # #    if len(self.__hc_queue) >= self.__cache_size:
    # #        expired = self.__hc_queue.pop(0)
    # #        comatose = self.__cache[expired]
    # #        del self.__cache[expired]
    # #        comatose._destroy()
    # #
    #### Returns results of a previously cached query
    # #def Fetch(self, hc):
    # #    return self.__cache[hc]

    # # Filename (prefix) of the database referenced by this object
    @property
    def filename(self):
        return self.__filename

    def __str__(self):
        return '<Database "{0}">'.format(self.filename)

    def __repr__(self):
        return self.__str__()

    # # Returns the tick at which the current metadata was written.
    #  This is used to determine whether to keep or purge the metadata during
    #  an element_set Update
    def GetMetadataTick(self):
        return self.__metadata_tick

    # # Sets the metadata tick. See GetMetadataTick
    def SetMetadataTick(self, tick):
        self.__metadata_tick = tick

    # # Returns values stored under a string 'name'
    # One current use: coloring and generated info on graph nodes
    def GetMetadata(self, objname):
        return self.__metadata.get(objname)

    def DumpMetadata(self):
        return self.__metadata

    def PurgeMetadata(self):
        self.__metadata.clear()

    # # Add or update specified key-value pair(s)
    def AddMetadata(self, objname, data):
        old_data = self.__metadata.get(objname)
        if old_data:
            old_data.update(data)
        else:
            self.__metadata[objname] = data
