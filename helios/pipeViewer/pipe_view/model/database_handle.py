## @package database_handle.py
#  @brief Structure that associates an ISL API and a Database instance

from __future__ import annotations
import os
import sys
import logging
from typing import Callable

from .database import Database, TransactionDatabase


## Associates a database with a query method and some result caching
#
#  Also allows browsing of database static structure as well as creating handles
#  to database transaction info.
#
#  The static database structure exposed through this class and its children is
#  can be shared between many DatabaseHandle instances
class DatabaseHandle:

    ## Constructor
    #  @param db argos Database instance
    def __init__(self, db: Database) -> None:

        if not isinstance(db, Database):
            raise TypeError('db must be an instance of database.Database, is a {0}'
                            .format(type(db)))

        self.__db = db
        self.__dbapi = self.__db.api

        if logging.root.isEnabledFor(logging.DEBUG):
            self.__dbapi.setVerbose(True)

    ## Associated database object which provides access to filename, clocks,
    #  and locations
    #
    #  Transaction information refering to the locations and clockos in this
    #  database is available through the 'api' attribute
    @property
    def database(self) -> Database:
        return self.__db

    def query(self, start_inc: int, end_inc: int, cb: Callable, mod_tracking: bool = True) -> None:
        assert end_inc >= start_inc, 'Query range must be ordered such that start <= end'

        # Handles queries with negative starts by invoking the callback for each
        # one because the update algorithm requires a callback at each tick
        if start_inc < 0:
            # Allow fake callbacks to be made here which contain no transactions
            self.__dbapi.clearCurrentTickContent()
            for t in range(start_inc, min(0,end_inc+1)):
                # print 'faking callback at ', t
                cb(t, self.__dbapi)

        # Warning. Do NOT clamp range here. Update algorithm expects callbacks
        # through entire range. Do skip over negative absolute times
        query_start_inc = max(0, start_inc)
        if end_inc >= 0:
            self.__dbapi.query(query_start_inc,
                               end_inc,
                               cb,
                               modify_tracking = mod_tracking)

    @property
    def api(self) -> TransactionDatabase:
        return self.__dbapi

    def __str__(self) -> str:
        return '<DatabaseHandle database="{0}">'.format(self.database)

    def __repr__(self) -> str:
        return self.__str__()
