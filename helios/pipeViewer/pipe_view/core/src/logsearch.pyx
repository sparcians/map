

## @package Argos search for SPARTA logs

import sys

from common cimport *
from libc.stdlib cimport strtoul

_DUMMY="dummy"

cimport cython

cdef extern from "log_search.cpp":

    cdef cppclass c_LogSearch "LogSearch":
        c_LogSearch(string filename)

        uint64_t getLocationByTick(uint64_t tick, uint64_t earlier_location)

        uint64_t BAD_LOCATION

cdef class LogSearch(object):

    cdef c_LogSearch* c_logsearch

    def __cinit__(self, filename):
        self.c_logsearch = new c_LogSearch(filename)

    def __init__(self):
        pass

    def __dealloc__(self):
        del self.c_logsearch

    property BAD_LOCATION:
        "Represents bad result from getLocationByTick"
        def __get__(self):
            return self.c_logsearch.BAD_LOCATION

    def __str__(self):
        return '<Argos Optimized SPARTA Log Searcher>'

    def __repr__(self):
        return self.__str__()

    def getLocationByTick(self, long tick, long earlier_loc=0):
        assert self.c_logsearch != NULL, 'Null logsearch object within wrapper. It may never have been allocated'

        return self.c_logsearch.getLocationByTick(<uint64_t>tick, <uint64_t>earlier_loc)

