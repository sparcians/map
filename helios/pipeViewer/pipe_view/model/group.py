from __future__ import annotations
import logging
from typing import List, Optional
import weakref

from .layout_context import Layout_Context


class Group:
    def __init__(self) -> None:
        # Managed contexts (weak refs)
        self.__contexts: List[weakref.ReferenceType[Layout_Context]] = []

    def AddContext(self, frame: Layout_Context) -> None:
        self.__contexts.append(weakref.ref(frame, self.__RemoveContext))

    def GetContexts(self) -> List[Layout_Context]:
        return [c for ctxt in self.__contexts if (c := ctxt()) is not None]

    # Set current tick in the context of
    def MoveTo(self,
               tick: int,
               context: Layout_Context,
               no_broadcast: bool = False) -> None:
        # \todo All contexts in this group are synced at the same tick.
        # In the future, they will be synced with relative offsets (per
        # context). Support for this must be added.

        lg = logging.getLogger('Group')
        lg.debug('Moving tick of group %s to %s. Caused by %s',
                 self,
                 tick,
                 context)

        assert context is not None, \
            'Cannot invoke Group.MoveTo with a context argument of None'

        context.SetHC(tick, no_broadcast=no_broadcast)  # Implies Update

        # Move other contexts
        for ctxtref in self.__contexts:
            ctxt = ctxtref()
            if ctxt is not None and ctxt is not context:
                lg.debug('  Updating %s', ctxt)
                ctxt.SetHC(tick, no_broadcast=no_broadcast)

        # Refresh the invoking context LAST so that its draws "faster".
        # This is unintuitive, but maybe wx events are stored in a stack?
        for ctxtref in self.__contexts:
            ctxt = ctxtref()
            if ctxt is not None and ctxt is not context:
                lg.debug('  Refreshing %s', ctxt)
                ctxt.RefreshFrame()
        context.RefreshFrame()

    def RemoveContext(self, context: Optional[Layout_Context]) -> None:
        if context is not None:
            for ctxtref in self.__contexts:
                ctxt = ctxtref()
                if ctxt is context:
                    logging.getLogger('Group').debug('  Removing %s', ctxt)
                    self.__RemoveContext(ctxtref)
                    break

    def __RemoveContext(
        self,
        context: weakref.ReferenceType[Layout_Context]
    ) -> None:
        assert isinstance(context, weakref.ref)
        assert context in self.__contexts
        self.__contexts.remove(context)
