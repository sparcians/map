# distutils: language = c++


## @package Setup file for transactiondb Python module

import sys
import re

from common cimport *
from libcpp.vector cimport vector
from libcpp.utility cimport pair
from libcpp.string cimport string
import inspect

cdef extern from "TransactionDatabaseInterface.hpp" namespace "sparta::pipeViewer":

    ctypedef uint32_t const_interval_idx "sparta::pipeViewer::TransactionDatabaseInterface::const_interval_idx const"
    ##ctypedef callback_fxn "sparta::pipeViewer::TransactionDatabaseInterface::callback_fxn"
    ##cdef void (__stdcall *callback_fxn)(void_ptr obj, \
    ##                                    uint64_t tick, \
    ##                                    const_interval_idx * content, \
    ##                                    c_TransactionInterval_uint64_t* transactions, \
    ##                                    uint32_t content_len)

    cdef cppclass c_TransactionDatabaseInterface "sparta::pipeViewer::TransactionDatabaseInterface":
        c_TransactionDatabaseInterface(string, uint32_t, bint) except +IOError # Can throw if file not opened

        void unload()
        void resetQueryState()
        void query(const uint64_t start_inc, \
                   const uint64_t end_inc, \
                   void (*cb)(void* obj, \
                              uint64_t tick, \
                              const_interval_idx * content, \
                              c_TransactionInterval_uint64_const_t* transactions, \
                              uint32_t content_len) except +, \
                   void* user_data, \
                   const bint modify_tracking) except +

        uint64_t getFileStart()
        uint64_t getFileEnd()
        uint64_t getWindowStart()
        uint64_t getWindowEnd()

        uint32_t getFileVersion() # const

        uint32_t getNodeLength() # const
        uint64_t getChunkSize() # const

        void setVerbose(bint verbose)
        bint getVerbose() # const

        string getNodeStates()
        string getNodeDump(uint32_t node_idx, \
                           uint32_t location_start, \
                           uint32_t location_end, \
                           uint32_t tick_entry_limit) # const
        string stringize()
        uint64_t getSizeInBytes()
        bint updateReady()
        void ackUpdate()
        void enableUpdate()
        void disableUpdate()
        void forceUpdate()

cdef extern from "TransactionDatabaseInterface.hpp" namespace "sparta::pipeViewer::TransactionDatabaseInterface":
    const_interval_idx NO_TRANSACTION # static const

cdef extern from "Reader.hpp" namespace "sparta::pipeViewer":
    string formatPairAsAnnotation(const uint64_t transaction_ID, \
                                  const uint64_t display_ID, \
                                  const uint16_t length, \
                                  const vector[string]& nameVector, \
                                  const vector[string]& stringVector);

cdef class Transaction:

    FLAGS_MASK_TYPE = 0b111
    CONTINUE_FLAG = 0x10
    ANNOTATION_TYPE_STR = 'annotation'
    INSTRUCTION_TYPE_STR = 'instruction'
    MEMORY_OP_TYPE_STR = 'memory_op'
    PAIR_TYPE_STR = 'pair'

    cdef c_TransactionInterval_uint64_const_t * __trans # Pointer to transaction data
    cdef bint __is_proxy # Is this transaction a proxy, or does it own a copy

    def __cinit__(self, *args):
        self.__trans = NULL
        self.__is_proxy = False

    def __init__(self, trans_ptr, is_proxy):
        """Internally construct a new transaction object (or proxy)
        DO NOT CALL this method outside of transaction database code.

        trans_ptr: Pointer (integer) to transaction object or None
        is_proxy: Is this object a proxy, If False, Makes a new Transaction
        based on trans_ptr and trans_ptr must not be None/0. If True,
        trans_ptr may be None or any integer
        """
        if (trans_ptr is not None) and (not isinstance(trans_ptr, (int, long))):
            raise TypeError('trans_ptr must be either None or an integer or long to be converted to a C pointer, is type {0}' \
                            .format(type(trans_ptr)))
        if not isinstance(is_proxy, bool):
            raise TypeError('is_proxy must be a bool, is type {0}'.format(type(is_proxy)))

        self.__is_proxy = <bint>is_proxy
        if self.__is_proxy == False:
            if trans_ptr is None or trans_ptr == 0:
                raise ValueError('is_proxy was False but trans_ptr did not refer to a valid object, was {0}' \
                                 .format(trans_ptr))

            # Not a proxy, make and own a copy
            #self.__trans = new c_TransactionInterval_uint64_t((<c_TransactionInterval_uint64_t*><ptr_t>trans_ptr)[0])
            self.__trans = new c_TransactionInterval_uint64_t((<c_TransactionInterval_uint64_t*><void*>trans_ptr)[0])
        else:
            if trans_ptr is None:
                self.__trans = NULL
            else:
                #self.__trans = <c_TransactionInterval_uint64_const_t*><ptr_t>trans_ptr
                self.__trans = <c_TransactionInterval_uint64_const_t*><void*>trans_ptr


    def __dealloc__(self):
        if not self.__is_proxy and self.__trans != NULL:
            del self.__trans

    def __str__(self):
        if self.__trans == NULL:
            return '<NULL Transaction>'

        type_flags = self.getFlags() & 0b111
        if type_flags == ANNOTATION:
            return '<Annotation ID:{0} LOC:{1} [{2},{3}] parent:{4} "{5}">' \
                .format(self.getTransactionID(), self.getLocationID(), self.getLeft(), self.getRight(), \
                        self.getParentTransactionID(), self.getAnnotation())
        elif type_flags == INSTRUCTION:
            return '<Instruction ID:{0} LOC:{1} [{2},{3}] op:{4:#x} v:{5:#x} p:{6:#x} parent:{7}>' \
                .format(self.getTransactionID(), self.getLocationID(), self.getLeft(), self.getRight(), \
                        self.getOpcode(), self.getVirtualAddress(), self.getRealAddress(), \
                        self.getParentTransactionID())
        elif type_flags == MEMORY_OP:
            return '<MemoryOp ID:{0} LOC:{1} [{2},{3}] v:{4:#x} p:{5:#x} parent:{6}>' \
                .format(self.getTransactionID(), self.getLocationID(), self.getLeft(), self.getRight(), \
                        self.getVirtualAddress(), self.getRealAddress(), \
                        self.getParentTransactionID())
        elif type_flags == PAIR:
            return '<Pair ID:{0} pairID:{1} LOC:{2} [{3},{4}] parent:{5} "{6}">'\
                .format(self.getTransactionID(), self.getPairID(), self.getLocationID(), self.getLeft(), self.getRight(),\
                    self.getParentTransactionID(), self.getAnnotation())
        else:
            return '<UnkonwnTransactionType ID:{0} LOC:{1} [{2},{3}] op:{4:#x} v:{5:#x} p:{6:#x} parent:{7}>' \
                .format(self.getTransactionID(), self.getLocationID(), self.getLeft(), self.getRight(), \
                        self.getOpcode(), self.getVirtualAddress(), self.getRealAddress(), \
                        self.getParentTransactionID())

    def __repr__(self):
        return self.__str__()


    # Proxy-Related Methods
    def makeRealCopy(self):
        """This object may only be a proxy (see isProxy) to a transaction, so
        modifying or destroying the related IntervalList object can cause this
        object to refer to a different transaction, A null-transaction (see
        isValid) or even be illegal to access because the proxied memory was
        deleted externally

        This method makes a full copy of the current transaction which is
        completely independet of any IntervalWindow or IntervalList. This is
        the safest way to hold onto a transaction object, but there is memory
        and time overhead in making a real copy.
        """
        return Transaction(<long><void*>self.__trans, False)

    cdef void _setProxiedTransaction(self, c_TransactionInterval_uint64_const_t* trans):
        """Updates the underlying translation proxied by this object.
        It is an error to call this method if this Transaction is not a proxy
        (see isProxy)
        """
        if self.__is_proxy == False:
            raise RuntimeError('Cannot set new proxied Transaction on a Transaction object that is not a proxy')

        self.__trans = trans

    cpdef bint isProxy(self):
        """Returns True if this object is a proxy for some transaction, False if
        it is a copy, which implies it has its own allocated object
        """
        return self.__is_proxy

    cpdef bint isValid(self):
        """Returns True if this Transaction is currently pointing to a valid
        object. If this Transaction is a proxy, this can be True or False. If
        this Transaction is not a proxy and is a real transaction copy,
        value will always be True.

        If False, do not attempt to call data accessors or copy this object
        """
        return self.__trans != NULL


    # Transaction Attributes
    def getComponentID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.control_ProcessID

    def getTransactionID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.transaction_ID

    def getPairID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.pairId

    def getDisplayID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.display_ID

    def getLocationID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.location_ID

    def getFlags(self):
        if self.__trans == NULL:
            return None
        return self.__trans.flags

    def getParentTransactionID(self):
        if self.__trans == NULL:
            return None
        return self.__trans.parent_ID

    def getOpcode(self):
        if self.__trans == NULL:
            return None
        return self.__trans.operation_Code

    def getVirtualAddress(self):
        if self.__trans == NULL:
            return None
        return self.__trans.virtual_ADR

    def getRealAddress(self):
        if self.__trans == NULL:
            return None
        return self.__trans.real_ADR

    def getAnnotationLength(self):
        if self.__trans == NULL:
            return None
        return self.__trans.length

    def getAnnotation(self):
        if self.__trans == NULL:
            return None
        cdef bytes py_str_preamble
        cdef bytes py_str_body
        cdef int i

        bytes_re = re.compile("b'(.*?)'")  # Look for b'xxx'

        py_str_preamble = b''
        py_str_body     = b''
        if self.getType() == ANNOTATION:
            if not self.__trans.annt.empty():
                value_str = bytes(self.__trans.annt)
                if str(value_str).isnumeric():
                    hex_string = format(int(value_str) & 0xf, 'x')
                    my_display_id = "R" + hex_string + hex_string
                    py_str_body = my_display_id.encode('utf-8') + b' ' + value_str
                else:
                    py_str_body = value_str
        else:
            py_str_body = formatPairAsAnnotation(self.__trans.transaction_ID,
                                                 self.__trans.display_ID,
                                                 self.__trans.length,
                                                 self.__trans.nameVector,
                                                 self.__trans.stringVector)

        # TODO this could already be a string to avoid decoding over and over

        decoded_str = bytes_re.sub(r'\1', (py_str_preamble + py_str_body).decode('utf-8'))    # Replace b'xxx' with xxx
#        decoded_str = decoded_str.replace('\x00', '')    # Remove null bytes
        decoded_str = decoded_str.replace(r'\x00', '')    # Remove null byte strings
        return decoded_str

    def getLeft(self):
        """
        Gets the left endpoint of the transaction in hypercycles
        """
        return self.__trans.getLeft()

    def getRight(self):
        """
        Gets the right endpoint of the transaction in hypercycles
        """
        return self.__trans.getRight()

    def getType(self):
        """
        Returns the integer type of this transaction extracted from flags
        """
        return self.getFlags() & self.FLAGS_MASK_TYPE

    def getTypeString(self):
        """
        Returns the string type of this transaction based on getType.
        Possible return values are ANNOTATION_TYPE_STR, MEMORY_OP_TYPE_STR, or
        INSTRUCTION_TYPE_STR
        """
        type_flags = self.getType()
        if type_flags == ANNOTATION:
            return self.ANNOTATION_TYPE_STR
        elif type_flags == INSTRUCTION:
            return self.INSTRUCTION_TYPE_STR
        elif type_flags == MEMORY_OP:
            return self.MEMORY_OP_TYPE_STR
        elif type_flags == PAIR:
            return self.PAIR_TYPE_STR
        else:
            raise RuntimeError('Got a transaction type value {0:#x} from flags ' \
                                   '{1:#x} that did not map to a known transaction type.' \
                                   .format(type_flags, self.getFlags()))

    def contains(self, hc):
        """
        Determines if this Transaction interval contains the given hypercycle
        """
        return self.__trans.contains(<uint64_t>hc)

    def containsInterval(self, l, r):
        """
        Determines if this Transaction interval contains the given interval
        defined by endpoint integers [l, r] in hypercycles
        """
        return self.__trans.containsInterval(<uint64_t>l, <uint64_t>r)

    def isContinued(self):
        return self.getFlags() & self.CONTINUE_FLAG != 0


cdef class TransactionDatabase:
    """
    Represents a sliding window view into a transaction database file.
    Can be used to perfom queries of active transactions at a particular
    hypercycle (tick number).
    """

    OBJECT_DESTROYED_ERROR = 'Cannot operate on a TransactionDatabase once _destroy()\'ed'

    cdef c_TransactionDatabaseInterface * __window # C implementation. Freed at destruction
    cdef object __filename # Name of file/dir containing the transaction database

    cdef const_interval_idx* __cur_content
    cdef c_TransactionInterval_uint64_const_t* __cur_transactions
    cdef uint32_t __cur_content_len
    cdef object __cur_callback
    cdef dict __cached_annotations

    cdef Transaction __trans_proxy

    def __cinit__(self, *args, **kwargs):
        self.__window = NULL
        self.__filename = None
        self.__cur_content = NULL
        self.__cur_transactions = NULL
        self.__cur_content_len = 0
        self.__cur_callback = None
        self.__cached_annotations = {}
        self.__trans_proxy = Transaction(None, True) # Create a Proxy

    def __init__(self, filename, num_locs, update_enabled):
        """
        Create a c_TransactionDatabaseInterface* based on the chosen filename
        """
        if not isinstance(filename, (str, unicode)):
            raise TypeError('filename must be a str, is type {0}'.format(type(filename)))

        self.__filename = filename
        cdef uint32_t c_num_locs = num_locs
        cdef char* c_str = <bytes><str>filename
        cdef string c_s = filename.encode('utf-8')
        self.__window = new c_TransactionDatabaseInterface(c_s, num_locs, update_enabled)

    def __dealloc__(self):
        self._destroy()

    ## Handles query result callbacks (per tick) from C++ transaction database
    #  library
    cdef void handleTickCallback(self, \
                                 uint64_t tick, \
                                 const_interval_idx * content, \
                                 c_TransactionInterval_uint64_const_t* transactions, \
                                 uint32_t content_len) except *:
        # Store current query results so that Python callback may access them
        # NOTE: Must update same set of values as clearCurrentTickContent
        self.__cur_content = content
        self.__cur_transactions = transactions
        self.__cur_content_len = content_len

        # Invoke python callback with current tick and self. Callback will
        # access this instance to look into result data
        self.__cur_callback(tick, self)

    def clearCurrentTickContent(self):
        """
        Clears the all pointers to content of the current tick. This will
        result in all queries for a proxied transaction to return None which
        effectively allows dummy callbacks with empty data to be made
        """
        # NOTE: Must update same set of values ad handleTickCallback
        self.__cur_content = NULL
        self.__cur_transactions = NULL
        self.__cur_content_len = 0

    def __str__(self):
        if self.__window == NULL:
            return '<TransactionDatabase DESTROYED>'

        ##return '<TransactionDatabase file="{0}" range=[{1},{2}] window=[{3},{4}]>' \
        ##    .format(self.__filename, self.getFileStart(), self.getFileEnd(), \
        ##            self.getWindowLeft(), self.getWindowRight())

        #cdef bytes py_s = self.__window.stringize().c_str()
        #cdef str s = <str>py_s
        s = self.__window.stringize().c_str()
        s += ' + {0}B cached annotations ({1})' \
             .format(int(self._getSizeOfCachedAnnotations()), len(self.__cached_annotations)).encode('utf-8')
        return s.decode('utf-8')

    def __repr__(self):
        return self.__str__()

    def getFileVersion(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getFileVersion()

    def getNodeLength(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getNodeLength()

    def getChunkSize(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getChunkSize()

    def setVerbose(self, bint verbose):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        self.__window.setVerbose(verbose)

    def getVerbose(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return bool(self.__window.getVerbose())

    def _destroy(self):
        """Manually destoy this object by freeing the contained window data
        structures. Otherwise, internal memory is freed at Python destruction
        (__del__).

        This is meant for use when discarding a window to ensure that the
        memory taken up by the interval window is freed regardless of Python

        Calling this method when this IntervalWindow is no longer
        needed is recommended.
        """
        if self.__window != NULL:
            del self.__window
            self.__window = NULL

    def getNodeStates(self):
        """Returns a string containing state of each node current loaded
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        cdef bytes py_s = self.__window.getNodeStates().c_str()
        return py_s.decode('utf-8')

    def getNodeDump(self, node_idx, loc_start=0, loc_end=0, tick_entry_limit=0):
        """Gets a string representing a node's content. To fit this on the
        screen, the number of locations (length of a row in the output) can be
        limited by loc_start and loc_end. The number of tick entries (number of
        rows) can be limited by tick_entry_limit
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        cdef bytes py_s = self.__window.getNodeDump(node_idx, loc_start, loc_end, tick_entry_limit).c_str()
        return py_s.decode('utf-8')

    def getSizeInBytes(self):
        """Returns the current (approximate) memory used by this structure in
        bytes
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        cdef uint64_t size = self.__window.getSizeInBytes()
        size += self._getSizeOfCachedAnnotations()
        return size

    cdef uint64_t _getSizeOfCachedAnnotations(self):
        getsizeof = sys.getsizeof
        cdef uint64_t size = getsizeof(self.__cached_annotations)
        size += sum([getsizeof(a) + getsizeof(b) for (a,b) in self.__cached_annotations.iteritems()])
        return size;

    def unload(self):
        """Unloads current windowed data
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        self.__window.unload()

    def query(self, start_inc, end_inc, callback, modify_tracking=True):
        """Performs a query in the transaction database at the given tick range
        and Makes a callback for each tick
        Callback must accept (tick_num, StateSnapshot)
        Queries
        modify_tracking should be set to false when a query is made that is
        smaller than the previous query and should not affect the transactiondb
        library's prediction for the next query. Generally, set this to false
        when querying specific data within the visual range of data.
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        self.__cur_callback = callback

        try:
            self.__window.query(<uint64_t>start_inc, <uint64_t>end_inc, \
                                c_callback, <void*><TransactionDatabase>self, \
                                <bint>modify_tracking)
        except:
            self.__window.resetQueryState()
            raise

    cdef c_TransactionInterval_uint64_const_t* _getTransaction(self, uint32_t c_loc):
        if self.__cur_content == NULL:
            return NULL
        if c_loc >= self.__cur_content_len:
            raise IndexError('Cannot access location {0} because only {1} locations are available' \
                             .format(<int>c_loc, self.__cur_content_len))
        cdef uint32_t c_trans_idx = self.__cur_content[c_loc]
        if c_trans_idx == NO_TRANSACTION:
            return NULL

        return &self.__cur_transactions[c_trans_idx]

    ##def getTransactionProxy(self, loc):
    ##    cdef c_TransactionInterval_uint64_const_t* c_trans
    ##    c_trans = self._getTransaction(loc)
    ##    return Transaction(<ptr_t>c_trans, True) # Create a Proxy

    ## Gets the transaction proxy for the transaction at the selected
    #  location based on the latest data received from a query calback
    #  @return None if there is no current location/transaction data from an
    #  internal query callback of if there is simply no transction at the
    #  specified locaiton at the current time
    def getTransactionProxy(self, loc):
        if loc < 0 or loc > 0xffffffff:
            return None # Implicitly invalid
        cdef c_TransactionInterval_uint64_const_t* c_trans
        c_trans = self._getTransaction(loc)
        if c_trans == NULL:
            return None
        self.__trans_proxy._setProxiedTransaction(c_trans)
        return self.__trans_proxy

    def getTransactionAnnotation(self, uint32_t loc):
        cdef c_TransactionInterval_uint64_const_t* c_trans
        c_trans = self._getTransaction(loc)
        if not c_trans:
            return None

        cached_str = self.__cached_annotations.setdefault(c_trans.transaction_ID, None)
        if cached_str:
            return cached_str

        cdef bytes py_str_preamble
        cdef bytes py_str_body
        cdef int i

        bytes_re = re.compile("b'(.*?)'")  # Look for b'xxx'

        py_str_preamble = b''
        py_str_body     = b''
        if self.getType() == ANNOTATION:
            if not self.__trans.annt.empty():
                value_str = bytes(self.__trans.annt)
                value_string = bytes_re.sub('r\1', str(value_str))  # Replace b'xxx' with xxx
                hex_string = format(int(value_str) & 0xf, 'x')
                my_display_id = "R" + hex_string + hex_string
                py_str_body = my_display_id.encode('utf-8') + b' ' + value_str
        else:
            my_display_id = self.__trans.display_ID if self.__trans.display_ID < 0x1000 else self.__trans.transaction_ID
            py_str_preamble += format(my_display_id, '>03x').encode('utf-8') + b' '

            for i in range(1, self.__trans.length):
                my_name  = bytes_re.sub(r'\1', str(self.__trans.nameVector[i]))
                my_value = bytes_re.sub(r'\1', str(self.__trans.stringVector[i]))
                if my_name != "DID":
                    py_str_body += my_name.encode('utf-8') + b'(' + my_value.encode('utf-8') + b')' + b' '

                if my_name == "uid":
                    value_string = "u" + format(int(my_value) % 10000, '>4d') + " "
                    py_str_preamble += value_string.encode('utf-8')

                elif my_name == "pc":
                    value_string = "0x" + format(int(my_value, 16) & 0xffff, '>04x') + " "
                    py_str_preamble += value_string.encode('utf-8')

                elif my_name == "mnemonic":
                    value_string = format(my_value[0:7], '<7') + " "
                    py_str_preamble += value_string.encode('utf-8')


        # Randomly remove from cache
        if len(self.__cached_annotations) > 30000:
            # TODO: consider whether this can be improved using a replacement policy
            self.__cached_annotations.clear()

        # Update cache
        decoded_str = bytes_re.sub(r'\1', (py_str_preamble + py_str_body).decode('utf-8'))    # Replace b'xxx' with xxx
        self.__cached_annotations[c_trans.transaction_ID] = decoded_str
        return decoded_str

    def getPairID(self, uint32_t loc):
        cdef c_TransactionInterval_uint64_const_t* c_trans
        c_trans = self._getTransaction(loc)
        if c_trans == NULL:
            return None
        return <int>c_trans.pairId

    def getTransactionID(self, uint32_t loc):
        cdef c_TransactionInterval_uint64_const_t* c_trans
        c_trans = self._getTransaction(loc)
        if c_trans == NULL:
            return None
        return <int>c_trans.transaction_ID

    def getDisplayID(self, uint32_t loc):
        cdef c_TransactionInterval_uint64_const_t* c_trans
        c_trans = self._getTransaction(loc)
        if c_trans == NULL:
            return None
        return <int>c_trans.display_ID

    def getLocationMap(self):
        cdef list results = []*<int>self.__cur_content_len
        cdef uint32_t i
        cdef uint32_t c_trans_idx
        for i in range(self.__cur_content_len):
            c_trans_idx = self.__cur_content[i]
            if c_trans_idx == NO_TRANSACTION:
                results.append('no')
            else:
                results.append(self.__cur_transactions[c_trans_idx].transaction_ID)
        return results

    ## Get the number of cached annotations
    def getNumCachedAnnotations(self):
        return len(self.__cached_annotations)

    def getWindowLeft(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getWindowStart()

    def getWindowRight(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getWindowEnd()

    def getFileStart(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getFileStart()

    ## Get the exclusive endpoint
    def getFileEnd(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getFileEnd()

    ## Get the inclusive end point. If 0, file has no data
    def getFileInclusiveEnd(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        cdef uint64_t end = self.__window.getFileEnd()
        if end == 0:
            return 0
        return end - 1

    def isUpdateReady(self):
        return self.__window.updateReady()

    def ackUpdate(self):
        self.__window.ackUpdate()

    def enableUpdate(self):
        self.__window.enableUpdate()

    def disableUpdate(self):
        self.__window.disableUpdate()

    def forceUpdate(self):
        self.__window.forceUpdate()

cdef void c_callback(void* obj,
                     uint64_t tick,
                     const_interval_idx * content,
                     c_TransactionInterval_uint64_const_t* transactions,
                     uint32_t content_len):
    cdef TransactionDatabase c_tdbi
    c_tdbi = <TransactionDatabase><object>obj
    c_tdbi.handleTickCallback(tick, content, transactions, content_len)
