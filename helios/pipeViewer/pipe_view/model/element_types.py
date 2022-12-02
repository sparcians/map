from __future__ import annotations
from .element import FakeElement, BoxElement, ImageElement
from .schedule_element import (ScheduleElement,
                               ScheduleLineElement,
                               ScheduleLineRulerElement)
from .rpc_element import RPCElement
from .speedo_element import SpeedoElement

# This module serves as a unified place to import Elements from

creatables = {
    BoxElement.GetType():               BoxElement,
    ScheduleElement.GetType():          ScheduleElement,
    ScheduleLineElement.GetType():      ScheduleLineElement,
    ScheduleLineRulerElement.GetType(): ScheduleLineRulerElement,
    ImageElement.GetType():             ImageElement,
    RPCElement.GetType():               RPCElement,
    SpeedoElement.GetType():            SpeedoElement,
}
# Disabled until complete: LogElement.GetType()          : LogElement}

special = {'fake': FakeElement}
