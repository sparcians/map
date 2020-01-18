import logging
import weakref

class Group(object):

    def __init__(self):
        self.__contexts = [] # Managed contexts (weak refs)

    def AddContext(self, frame):
        self.__contexts.append(weakref.ref(frame, self.__RemoveContext))

    def GetContexts(self):
        return [ctxt() for ctxt in self.__contexts if ctxt() != None]

    ## Set current tick in the context of
    def MoveTo(self, tick, context, no_broadcast = False):
        ## \todo All contexts in this group are synced at the same tick.
        #  In the future, they will be synced with relative offsets (per
        #  context). Support for this must be added.

        lg = logging.getLogger('Group')
        lg.debug('Moving tick of group {} to {}. Caused by {}' \
                 .format(self, tick, context))

        assert context is not None, 'Cannot invoke Group.MoveTo with a context argument of None'

        context.SetHC(tick, no_broadcast = no_broadcast) # Implies Update
        ##context.Update() # SetHC(tick) implies update

        # Move other contexts
        for ctxtref in self.__contexts:
            ctxt = ctxtref()
            if ctxt is not None and ctxt is not context:
                lg.debug('  Updating {}'.format(ctxt))
                ctxt.SetHC(tick, no_broadcast = no_broadcast)
                ##ctxt.Update() # SetHC(tick) implies update

        # Refresh the invoking context LAST so that its draws "faster".
        # This is unintuitive, but maybe wx events are stored in a stack?
        for ctxtref in self.__contexts:
            ctxt = ctxtref()
            if ctxt is not None and ctxt is not context:
                lg.debug('  Refreshing {}'.format(ctxt))
                ctxt.RefreshFrame()
        context.RefreshFrame()
    
    def RemoveContext(self, context):
        if context is not None:
            for ctxtref in self.__contexts:
                ctxt = ctxtref()
                if ctxt is context:
                    logging.getLogger('Group').debug('  Removing {}'.format(ctxt))
                    self.__RemoveContext(ctxtref)
                    break

    def __RemoveContext(self, context):
        assert isinstance(context, weakref.ref)
        assert context in self.__contexts
        self.__contexts.remove(context)
