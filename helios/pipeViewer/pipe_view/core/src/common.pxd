## @package Common files for transction database interface

cdef extern from "stdint.h":

    ctypedef long uint8_t
    ctypedef long uint16_t
    ctypedef long uint32_t
    ctypedef long uint64_t
    ctypedef long int8_t
    ctypedef long int16_t
    ctypedef long int32_t
    ctypedef long int64_t

cdef extern from "cstring" namespace "std":

    ctypedef int size_t

    cdef size_t strlen(char*)

cdef extern from "helpers.h" namespace "pipeViewer":

    ctypedef long ptr_t

cdef extern from "helpers.h":

    cdef char* PIPEVIEWER_VERSION "_PIPEVIEWER_VERSION"

cdef extern from "string" namespace "std":

    cdef cppclass string:
        string(char* c) # c is const
        char* c_str()
