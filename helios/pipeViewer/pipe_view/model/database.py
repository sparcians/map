# @package database.py
#  @brief Consumes argos database files based on prefix

from __future__ import annotations
import logging
from logging import info, error
from types import ModuleType
from typing import Any, Dict, Optional

from .location_manager import LocationManager
from .clock_manager import ClockManager

try:
    from .. import transactiondb
except ImportError as e:
    error('Argos failed to import module: "transactiondb"')
    raise e

# Inform users which transactiondb interface they are getting
info('Using transactiondb at "%s"', transactiondb.__file__)

Transaction = transactiondb.Transaction
TransactionDatabase = transactiondb.TransactionDatabase


# Consumes an Argos database and creates a location manager, a clock manager,
#  and a transaction database API
#
#  Also allows browsing of database static structure as well as creating
#  handles to database transaction info.
#
#  The static database structure exposed through this class and its children is
#  can be shared between many DatabaseHandle instances
class Database:

    # Constructor
    #  @param prefix Argos transaction database prefix to open.
    #  Transaction database file extensions will be appended to determine the
    #  actual filenames to open
    def __init__(self, prefix: str, update_enabled: bool) -> None:

        self.__filename = prefix
        self.__loc_mgr = LocationManager(prefix, update_enabled)
        self.__clk_mgr = ClockManager(prefix)

        # stores extra data shared between objects across layout contexts using
        # the same database
        self.__metadata: Dict[str, Dict[str, Any]] = {}
        self.__metadata_tick: Optional[int] = None

        # Note that this will need to move if multiple layout contexts access
        # the same database sporadically
        logging.getLogger('Database').debug(
            'Database %s about to open query API',
            self
        )
        self.__dbapi = transactiondb.TransactionDatabase(
            self.filename,
            1 + self.location_manager.getMaxLocationID(),
            update_enabled
        )
        logging.getLogger('Database').debug(
            'Database opened with node length %s, heartbeat size %s',
            self.__dbapi.getNodeLength(),
            self.__dbapi.getChunkSize()
        )

    # Gets the database implementation module
    @property
    def dbmodule(self) -> ModuleType:
        return transactiondb

    # Query API (TransactionDatabase)
    @property
    def api(self) -> transactiondb.TransactionDatabase:
        return self.__dbapi

    # ClockManager object containing all clocks in this database
    @property
    def clock_manager(self) -> ClockManager:
        return self.__clk_mgr

    # LocationManager object containing all locations in this database
    @property
    def location_manager(self) -> LocationManager:
        return self.__loc_mgr

    # Filename (prefix) of the database referenced by this object
    @property
    def filename(self) -> str:
        return self.__filename

    def __str__(self) -> str:
        return f'<Database "{self.filename}">'

    def __repr__(self) -> str:
        return self.__str__()

    # Returns the tick at which the current metadata was written.
    #  This is used to determine whether to keep or purge the metadata during
    #  an element_set Update
    def GetMetadataTick(self) -> Optional[int]:
        return self.__metadata_tick

    # Sets the metadata tick. See GetMetadataTick
    def SetMetadataTick(self, tick: int) -> None:
        self.__metadata_tick = tick

    # Returns values stored under a string 'name'
    # One current use: coloring and generated info on graph nodes
    def GetMetadata(self, objname: str) -> Optional[Dict[str, Any]]:
        return self.__metadata.get(objname)

    def DumpMetadata(self) -> Dict[str, Dict[str, Any]]:
        return self.__metadata

    def PurgeMetadata(self) -> None:
        self.__metadata.clear()

    # Add or update specified key-value pair(s)
    def AddMetadata(self, objname: str, data: Dict[str, Any]) -> None:
        old_data = self.__metadata.get(objname)
        if old_data:
            old_data.update(data)
        else:
            self.__metadata[objname] = data
