# # @package contentoptions
#  This module exists to provide a centralized listing of all content options
#  an Element can display, combined with descriptions of those content
#  options and the logic to extract the specified content from a transaction

# # Transaction type strings known to the viewer
TRANSACTION_TYPES = ['Annotation', 'Instruction', 'MemoryOp']


# # These methods are used to actually acquire the data specified by the
#  various content options
#
#  TODO: Implement checks on each of these Get* methods to see if there is
#  valid data, otherwise return a string corresponding to 'no data' from
#  __DISPLAY_STATES
def GetType(trans, e, *args):
    return TRANSACTION_TYPES[trans.getType()]


def GetStart(trans, e, *args):
    return trans.getLeft()


def GetEnd(trans, e, *args):
    return trans.getRight()


def GetTransactionID(trans, e, *args):
    return trans.getTransactionID()


def GetLocation(trans, e, *args):
    return e.GetProperty('LocationString')


def GetLocationTrunc(trans, e, dbhandle, tick, loc_vars, *args):
    return dbhandle.database.location_manager.replaceLocationVariables(e.GetProperty('LocationString'), loc_vars).split('.')[-1]


def GetLocationID(trans, e, dbhandle, tick, loc_vars, *args):
    return dbhandle.database.location_manager.getLocationInfo(e.GetProperty('LocationString'), loc_vars)[0]


def GetFlags(trans, *args):
    return hex(int(trans.getFlags()))


def GetParent(trans, *args):
    return trans.getParentTransactionID()


def GetOpcode(trans, *args):
    return hex(int(trans.getOpcode()))


def GetVAddr(trans, *args):
    return hex(int(trans.getVirtualAddress()))


def GetRAddr(trans, *args):
    return hex(int(trans.getRealAddress()))


def GetAnnotation(trans, *args):
    return trans.getAnnotation()


def GetCaption(trans, e, *args):
    return e.GetProperty('caption')


def GetImage(*args):
    return ''


def GetClock(trans, e, dbhandle, tick, loc_vars, *args):
    clock_id = dbhandle.database.location_manager.getLocationInfo(e.GetProperty('LocationString'), loc_vars)[2]
    if clock_id != dbhandle.database.location_manager.NO_CLOCK:
        clk = dbhandle.database.clock_manager.getClockDomain(clock_id)
        return clk.name
    return "#NO! <clock>"


def GetCycle(trans, e, dbhandle, tick, loc_vars, *args):
    clock_id = dbhandle.database.location_manager.getLocationInfo(e.GetProperty('LocationString'), loc_vars)[2]
    if clock_id != dbhandle.database.location_manager.NO_CLOCK:
        clk = dbhandle.database.clock_manager.getClockDomain(clock_id)
        return str(clk.HypercycleToLocal(tick))
    return "#NO! <clock>"


def GetStartCycle(trans, e, dbhandle, tick, loc_vars, *args):
    return GetCycle(trans, e, dbhandle, trans.getLeft(), loc_vars, *args)


def GetEndCycle(trans, e, dbhandle, tick, loc_vars, *args):
    return GetCycle(trans, e, dbhandle, trans.getRight(), loc_vars, *args)


def GetTickDuration(trans, *args):
    return trans.getRight() - trans.getLeft()


def IsContinued(trans, e, *args):
    return trans.isContinued()


# # The dictionary with all the content options, how to get the specified
#  content, and a description of each content option. Add additional content
#  options in this dictionary, and write the related function above
__CONTENT_PROC = {'type':                  (GetType, "Whether the transaction is an annotation, instruction, or memory_op"),
                'start':                 (GetStart, "Start time in ticks"),
                'end':                   (GetEnd, "End time in ticks"),
                'start_cycle':           (GetStartCycle, "Start time in location's clock domain"),
                'end_cycle':             (GetEndCycle, "End time in location's clock domain"),
                'transaction':           (GetTransactionID, "Transaction ID"),
                'loc':                   (GetLocation, "Location Name (string)"),
                'loc_id':                (GetLocationID, "Internal Location ID (int)"),
                'truncated_location':    (GetLocationTrunc, "Location Name Truncated (string)"),
                'flags':                 (GetFlags, "Flags from transaction"),
                'parent':                (GetParent, "Parent Transaction ID"),
                'opcode':                (GetOpcode, "instruction opcode (\'-\' if transaction is a memory op"),
                'vaddr':                 (GetVAddr, "instruction/mem-op virtual address"),
                'paddr':                 (GetRAddr, "instruction/mem-op physical address"),
                'annotation':            (GetAnnotation, "Show annotation selected by \'annotation\' property"),
                'auto_color_annotation': (GetAnnotation, "Show annotation selected by \'annotation\' property colored based on seq ID"),
                'auto_color_anno_notext':(GetAnnotation, "Color element based on seq ID, but do not display annotation"),
                'auto_color_anno_nomunge':(GetAnnotation, "Color element based on seq ID, but do not munge text"),
                'caption':               (GetCaption, "Caption (static text) from Element \'caption\' property"),
                'image':                 (GetImage, "Image (currently hardcoded to nothing)"),
                'clock':                 (GetClock, "Name of clock associated with this location"),
                'cycle':                 (GetCycle, "Current Cycle of clock associated with this location"),
                'tick_duration':         (GetTickDuration, "Duration of this transaction in ticks"),
                'continued':             (IsContinued, "Whether the transaction is continued over a heartbeat interval")
                }
# # The dictionary with all display states and the string which will be used
#  to override the val of an Element Value
__DISPLAY_STATES = {'normal':   '', # Display whatever belongs inside the element
                  'no trans': '', # Display empy space
                  'no data':  '#NO! ', # Display a MS Excel-style "#NO! ContentOption" Where
                                       # ContentOption is some inappropriate value for content for a transaction
                  'no loc':   '?' # Display when the element has a location string that does not refer to an
                                       # actual location in the locations file
                  }

# # A listing of the content options which do not require a transaction
#  from a stabbing query in order to be determined. Use a dictionary for faster
#  lookup in the critical path
NO_TRANSACTIONS_REQUIRED = {'caption':None,
                            'clock':None,
                            'loc':None,
                            'loc_id':None,
                            'truncated_location':None,
                            'cycle':None}

# # A listing of the content options which do not require a database at all.
#  Use a dictionary for faster lookup in the critical path
NO_DATABASE_REQUIRED = {'caption':None}

# Check NO_DATABASE_REQUIRED table
for x in NO_DATABASE_REQUIRED:
    assert x in NO_TRANSACTIONS_REQUIRED, \
           'All content types specified in NO_DATABASE_REQUIRED must also be found in ' \
           'NO_TRANSACTIONS_REQUIRED. "{}" violated this'.format(x)


def OverrideState(key):
    return __DISPLAY_STATES[key]


# # This method is used by an Ordered Dictionary to populate it's
#  Element_Value's with the data
def ProcessContent(content, trans, e, dbhandle, tick, loc_vars):
    return __CONTENT_PROC[content][0](trans, e, dbhandle, tick, loc_vars)


# # Returns the string describing the specified content option
def ContentDescription(content):
    return __CONTENT_PROC[content][1]


__CONTENT_OPTIONS = list(__CONTENT_PROC.keys())


# # Returns a listing of the available content options
def GetContentOptions():
    return __CONTENT_OPTIONS
