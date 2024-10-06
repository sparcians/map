# distutils: language = c++


## @package Common files for transction database interface

# import some common integer and string c++ type definitions

import sys
sys.path.append("../../")
from libcpp.vector cimport vector
from libcpp.utility cimport pair
from libcpp.string cimport string
from libc.stdint cimport *

cdef extern from "helpers.hpp" namespace "transactiondb":

    ctypedef long ptr_t

#ctypedef ptr_t void_ptr "void*"
#cdef ptr_t void_ptr "void*"

cdef extern from "sparta/pipeViewer/transaction_structures.hpp":

    cdef extern int ANNOTATION "is_Annotation"
    cdef extern int INSTRUCTION "is_Instruction"
    cdef extern int MEMORY_OP "is_MemoryOperation"
    cdef extern int PAIR "is_Pair"

cdef extern from "TransactionInterval.hpp" namespace "sparta::pipeViewer":

    cdef cppclass c_TransactionInterval_uint64_t "sparta::pipeViewer::transactionInterval<uint64_t>":

        c_TransactionInterval_uint64_t(c_TransactionInterval_uint64_t) # Const ref arg

        const uint16_t control_ProcessID
        const uint64_t transaction_ID
        const uint64_t display_ID
        const uint32_t location_ID
        const uint16_t flags
        const uint64_t parent_ID
        const uint32_t operation_Code
        const uint64_t virtual_ADR
        const uint64_t real_ADR
        const uint16_t length
        const string annt
        const uint16_t pairId
        const vector[uint16_t] sizeOfVector
        const vector[pair[uint64_t, bint]] valueVector
        const vector[string] nameVector
        const vector[string] stringVector
        const vector[string] delimVector

        uint64_t getLeft() # const
        uint64_t getRight() # const
        bint contains(uint64_t V) # const # V is a const ref
        bint containsInterval(uint64_t l, uint64_t r) # l and r are const refs

cdef extern from *:
    ctypedef c_TransactionInterval_uint64_t c_TransactionInterval_uint64_const_t "sparta::pipeViewer::transactionInterval<uint64_t> const"
