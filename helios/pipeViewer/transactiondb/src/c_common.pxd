
## @package c_common Some useful c++ type definitions for cython

cdef extern from "stdint.h":

    ctypedef long uint8_t
    ctypedef long uint16_t
    ctypedef long uint32_t
    ctypedef long uint64_t

cdef extern from "string" namespace "std":

    cdef cppclass string:
        string(char* c) # c is const
        char* c_str()
