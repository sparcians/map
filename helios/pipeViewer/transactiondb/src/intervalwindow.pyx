# distutils: language = c++


## @package Setup file for transactiondb Python module

from libcpp.vector cimport vector
from libcpp.utility cimport pair
from libcpp.string cimport string

cdef extern from "stdint.h":

    ctypedef long uint8_t
    ctypedef long uint16_t
    ctypedef long uint32_t
    ctypedef long uint64_t

'''
cdef extern from "string" namespace "std":

    cdef cppclass string:
        string(char* c) # c is const
        char* c_str()
'''

cdef extern from "helpers.h" namespace "transactiondb":

    ctypedef long ptr_t

cdef extern from "sparta/pipeViewer/transaction_structures.hpp":

    cdef extern int ANNOTATION "is_Annotation"
    cdef extern int INSTRUCTION "is_Instruction"
    cdef extern int MEMORY_OP "is_MemoryOperation"
    cdef extern int PAIR "is_Pair"

cdef extern from "TransactionInterval.hpp" namespace "sparta::pipeViewer":

    cdef cppclass c_TransactionInterval_uint64_t "sparta::pipeViewer::transactionInterval<uint64_t>":

        c_TransactionInterval_uint64_t(c_TransactionInterval_uint64_t) # Const ref arg

        uint16_t control_ProcessID
        uint64_t transaction_ID
        uint16_t location_ID
        uint16_t flags
        uint64_t parent_ID
        uint32_t operation_Code
        uint64_t virtual_ADR
        uint64_t real_ADR
        uint16_t length
        uint8_t *annt
        vector[uint16_t] sizeOfVector
        vector[pair[uint64_t, bint]] valueVector
        vector[string] nameVector
        vector[string] stringVector
        vector[string] delimVector

        uint64_t getLeft() # const
        uint64_t getRight() # const
        bint contains(uint64_t V) # const # V is a const ref
        bint containsInterval(uint64_t l, uint64_t r) # l and r are const refs


cdef extern from "ISL/IntervalList.hpp" namespace "ISL":

    cdef cppclass c_IntervalListElt "ISL::IntervalList<sparta::pipeViewer::transactionInterval<uint64_t>>::IntervalListElt":

        c_IntervalListElt* next

        ##void set_next(c_IntervalListElt* nextElt)
        c_IntervalListElt* get_next()
        c_TransactionInterval_uint64_t* getInterval()
        c_TransactionInterval_uint64_t* getI()

    cdef cppclass c_IntervalList "ISL::IntervalList<sparta::pipeViewer::transactionInterval<uint64_t>>":

        c_IntervalList()

        #bint contains(c_IntervalListElt * I) # I is const ptr
        int isEqual(c_IntervalList* l)
        int length()
        #void empty()
        bint isEmpty() # const
        #c_IntervalListElt* get_next(IntervalListElt* element) # const # element is const ptr
        c_IntervalListElt* get_first() # const


cdef extern from "IntervalWindow.hpp" namespace "sparta::pipeViewer":

    cdef cppclass c_IntervalWindow "sparta::pipeViewer::IntervalWindow":
        c_IntervalWindow(string) except +IOError # Can throw if file not opened

        void setOffsetLeft( uint64_t sOL )
        void setOffsetRight( uint64_t sOR )
        uint64_t getWindowL()
        uint64_t getWindowR()

        uint64_t getFileStart()
        uint64_t getFileEnd()
        void stabbingQuery(uint64_t qClock, c_IntervalList List) # List is a ref



cdef class Transaction(object):

    FLAGS_MASK_TYPE = 0b111
    CONTINUE_FLAG = 0x10
    ANNOTATION_TYPE_STR = 'annotation'
    INSTRUCTION_TYPE_STR = 'instruction'
    MEMORY_OP_TYPE_STR = 'memory_op'
    PAIR_TYPE_STR = 'pair'
    cdef c_TransactionInterval_uint64_t * __trans # Pointer to transaction data
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
                #self.__trans = <c_TransactionInterval_uint64_t*><ptr_t>trans_ptr
                self.__trans = <c_TransactionInterval_uint64_t*><void*>trans_ptr


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
            return '<Pair ID:{0} LOC:{1} [{2},{3}] parent:{4} "{5}">'\
                .format(self.getTransactionID(), self.getLocationID(), self.getLeft(), self.getRight(),\
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

    cdef void _setProxiedTransaction(self, c_TransactionInterval_uint64_t* trans):
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
        cdef bytes py_str
        cdef char * ann
        cdef int i

        if self.getType() == ANNOTATION:
            ann = <char*>self.__trans.annt
            if ann == NULL:
                py_str = b''
            else:
                py_str = ann

        else:
            py_str = b''
            py_str = py_str + str(self.__trans.transaction_ID).encode('utf-8') + b' '
            for i in range(1, self.__trans.length):
                py_str += str(self.__trans.nameVector[i]).encode('utf-8') + b'(' + str(self.__trans.stringVector[i]).encode('utf-8') + b')' + b' '

        return py_str.decode('utf-8')

    def getLeft(self):
        """Gets the left endpoint of the transaction in hypercycles
        """
        return self.__trans.getLeft()

    def getRight(self):
        """Gets the right endpoint of the transaction in hypercycles
        """
        return self.__trans.getRight()
    def getType(self):
        """Returns the integer type of this transaction extracted from flags
        """
        return self.getFlags() & self.FLAGS_MASK_TYPE

    def getTypeString(self):
        """Returns the string type of this transaction based on getType.
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
        """Determines if this Transaction interval contains the given hypercycle
        """
        return self.__trans.contains(<uint64_t>hc)

    def containsInterval(self, l, r):
        """Determines if this Transaction interval contains the given interval
        defined by endpoint integers [l, r] in hypercycles
        """
        return self.__trans.containsInterval(<uint64_t>l, <uint64_t>r)

    def isContinued(self):
        return self.getFlags() & self.CONTINUE_FLAG != 0


cdef class IntervalListIterator(Transaction):
    """Iterator object for IntervalList. Implements Python iterator protocol.

    Also acts as a IntervalListElement proxy for the current element for
    performance reasons. Therefore, the object returned during iteration is THIS
    object and that result should not be cached. To hold onto a transaction
    result OUTSIDE the context of this iterator object, a copy must be made
    using makeRealCopy or makeProxy.

    See IntervalList for more details
    """

    cdef bint __done # Is iteration complete
    cdef object __interval_list # List being walked. Holds reference to keep refcount up and prevent destruction
    cdef c_IntervalListElt * __elt # Element currently proxying. Incremented on next()
    #cdef c_TransactionInterval_uint64_t * __trans # Transaction contained in current element (__elt)

    def __init__(self, interval_list):
        if not isinstance(interval_list, IntervalList):
            raise TypeError('interval_list must be an instance of IntervalList, is type {0}' \
                .format(type(interval_list)))

        self.__done = False
        self.__interval_list = <IntervalList>interval_list
        self.__elt = NULL

        super(self.__class__, self).__init__(None, True) # Construct Transction as NULLed proxy


    # Python Iteration Protocol

    def __iter__(self):
        return self

    def __next__(self):
        if self.__elt == NULL:
            if self.__done == True:
                raise StopIteration # May have had no elements or somehow re-invoked

            # Get initial interval
            self.__elt = (<IntervalList>self.__interval_list).getCppList().get_first()
        else:
            self.__elt = self.__elt.get_next()

        if self.__elt == NULL:
            self._setProxiedTransaction(NULL)
            self.__done = True # Reached end of iteration, flag done
            raise StopIteration

        self._setProxiedTransaction(self.__elt.getInterval())

        return self


    # Proxy Methods

    def makeRealCopy(self):
        """This object is only be a proxy (see isProxy) to a transaction, so
        continuing iteration will cause this object to refer to a new
        transaction or NULL. This is done for iteration performance. to avoid
        the overhead of constructing a new object N times during iteration.

        This method makes a full copy of the current proxied transaction which
        is completely independant of any IntervalWindow or IntervalList. This is
        the safest way to hold onto a transaction object, but there is memory
        and time overhead in making a real copy.
        """
        return super(self.__class__, self).makeRealCopy()

    def makeProxy(self):
        """This object is merely a proxy to the current transaction, so
        continuing iteration will cause the object to refer to a new transaction
        or NULL.

        This method makes a proxy of the current transaction which is still
        depdendant on the IntervalList being iterated. If the IntervalList is
        destroyed, the proxy copy made here becomes invalid.
        """
        #return Transaction(<ptr_t>self.__trans, True)
        return Transaction(<long><void*>self.__trans, True)


cdef class IntervalList(object):
    """Represents a list of intervals retrieved as the result of a query

    This is an iterable object. Iterating this object is the preferred and only
    means of accessing elements because the Iterator object also acts as a
    proxy to the current item. This eliminates the construction/destruction of
    python objects during iteration for better performance. Random access is not
    an expected use case.

    This object is NOT currently indexable or slicable because this requires
    creating a temporary Python object which would be detrimental to
    performance.

    When iterating, either real or proxy copies must be made of the current
    object in order to use that object outside the life of the iterator or even
    within a different iteration of the same loop.

    Example:
    # Some IntervalWindow iw
    # Some IntervalList il (from iw)
    interesting_transaction_temp = None
    interesting_transaction_copy = None
    interesting_transaction_proxy = None
    for t in il:
        print t
        if isInteresting(t):
            interesting_transaction_temp = t # BAD will not be same during next iter. Invalid outside of loop
            interesting_transaction_copy = t.makeRealCopy()
            interesting_transaction_proxy = t.makeProxy()
            break

    assert interesting_transaction_temp.isValid

    # The proxy is still valid since the interval list still exists
    print interesting_transaction_proxy

    del il
    del iw

    # This transaction still exists after the interval list has been destroyed,
    # the iterator is gone, and the window constaining this transaction has been
    # destroyed as well
    print interesting_transaction_copy
    """

    OBJECT_DESTROYED_ERROR = 'Cannot operate on an IntervalList once _destroy()\'ed'

    cdef c_IntervalList * __list # List of intervals held within. Freed at destruction

    def __cinit__(self, list):
        self.__list = NULL

    def __init__(self, list):
        #self.__list = <c_IntervalList*><ptr_t>list
        self.__list = <c_IntervalList*><void*>list
        if self.__list == NULL:
             raise TypeError('list must be a non-NULL c_IntervalList pointer')

    def __dealloc__(self):
        self._destroy()

    def __str__(self):
        if self.__list == NULL:
            return '<IntervalList DESTROYED>'

        return '<IntervalList count={0}>' \
            .format(self.getLength())

    def __repr__(self):
        return self.__str__()

    def _destroy(self):
        """Manually destoy this object by freeing the contained interval
        structures. Otherwise, internal memory is freed at Python destruction
        (__del__). Note: Any IntervalListIterators referring to this object
        (created when iterating this object) will become invalid and illegal to
        use at this time.

        This is meant for use when discarding a IntervalList dataset to ensure
        that the memory taken up by the intervals is freed regardless of Python
        garbage-collection behavior.

        Calling this method when this IntervalList is no longer
        needed is recommended.
        """
        if self.__list != NULL:
            del self.__list
            self.__list = NULL

    cdef c_IntervalList* getCppList(self):
        return self.__list

    def __iter__(self):
        """This class supports iteration
        """
        if self.__list == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return IntervalListIterator(self)

    def __len__(self):
        return self.getLength()

    def getLength(self):
        """Returns the length of this interval list
        """
        if self.__list == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return <int>self.__list.length()

    def isEmpty(self):
        """Returns True if this list is empty
        """
        if self.__list == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return bool(self.__list.isEmpty())



cdef class IntervalWindow:
    """Represents a sliding window view into a transaction database file.
    Can be used to perfom queries of active transactions at a particular
    hypercycle (tick number).
    """

    OBJECT_DESTROYED_ERROR = 'Cannot operate on an IntervalWindow once _destroy()\'ed'

    cdef c_IntervalWindow * __window # Window object. Freed at destruction
    cdef object __filename # Name of file/dir containing the transaction database

    def __cinit__(self, filename):
        self.__window = NULL

    def __init__(self, filename):
        """Create an IntervalWindow based on the chosen filename
        """
        if not isinstance(filename, (str, unicode)):
            raise TypeError('filename must be a str, is type {0}'.format(type(filename)))

        self.__filename = filename
        cdef char* c_str = <bytes><str>filename
        cdef string s = filename
        self.__window = new c_IntervalWindow(s)

    def __dealloc__(self):
        self._destroy()

    def __str__(self):
        if self.__window == NULL:
            return '<IntervalWindow DESTROYED>'

        return '<IntervalWindow file="{0}" range=[{1},{2}] window=[{3},{4}]>' \
            .format(self.__filename, self.getFileStart(), self.getFileEnd(), \
                    self.getWindowLeft(), self.getWindowRight())

    def __repr__(self):
        return self.__str__()

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

    def query(self, hc):
        """Performs a query in the transaction database at the given tick number
        and returns an IntervalList object containing the results.
        If hc is < 0, returns an empty IntervalList.
        """
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        cdef c_IntervalList* c_l = new c_IntervalList()
        if hc >= 0:
            self.__window.stabbingQuery(<uint64_t>hc, c_l[0])
        #return IntervalList(<ptr_t>c_l)
        return IntervalList(<long><void*>c_l)

    def setOffsetLeft(self, hc):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        self.__window.setOffsetLeft(hc)

    def setOffsetRight(self, hc):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        self.__window.setOffsetRight(hc)

    def getWindowLeft(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getWindowL()

    def getWindowRight(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getWindowR()

    def getFileStart(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getFileStart()

    def getFileEnd(self):
        if self.__window == NULL:
            raise RuntimeError(self.OBJECT_DESTROYED_ERROR)

        return self.__window.getFileEnd()
