from wx import Window, Panel, Scrolled, ID_ANY, DefaultPosition, DefaultSize, PointUnion, SizeUnion
from typing import overload

class HtmlWindowInterface:
    ...

HW_DEFAULT_STYLE: int
HW_SCROLLBAR_AUTO: int

class HtmlWindow(Scrolled, Panel, HtmlWindowInterface):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self,
                 parent: Window,
                 id: int = ID_ANY,
                 pos: PointUnion = DefaultPosition,
                 size: SizeUnion = DefaultSize,
                 style: int = HW_DEFAULT_STYLE,
                 name: str = 'htmlWindow') -> None: ...
    def SetPage(self, source: str) -> bool: ...
