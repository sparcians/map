import logging
from typing import List, Optional
import weakref

from model.layout_context import Layout_Context

class Group(object):

    def __init__(self) -> None:
        self.__contexts: List[weakref.ReferenceType[Layout_Context]] = [] # Managed contexts (weak refs)

    def AddContext(self, frame: Layout_Context) -> None:
        self.__contexts.append(weakref.ref(frame, self.__RemoveContext))

    def GetContexts(self) -> List[Layout_Context]:
        return [c for ctxt in self.__contexts if (c := ctxt()) is not None]

    ## Set current tick in the context of
    def MoveTo(self, tick: int, context: Layout_Context, no_broadcast: bool = False) -> None:
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
    
    def RemoveContext(self, context: Optional[Layout_Context]) -> None:
        if context is not None:
            for ctxtref in self.__contexts:
                ctxt = ctxtref()
                if ctxt is context:
                    logging.getLogger('Group').debug('  Removing {}'.format(ctxt))
                    self.__RemoveContext(ctxtref)
                    break

    def __RemoveContext(self, context: weakref.ReferenceType[Layout_Context]) -> None:
        assert isinstance(context, weakref.ref)
        assert context in self.__contexts
        self.__contexts.remove(context)
