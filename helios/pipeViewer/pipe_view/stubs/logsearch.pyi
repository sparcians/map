from typing import Any

_DUMMY: str

class LogSearch:
    BAD_LOCATION: Any
    def __init__(self, *args: Any) -> None: ...
    def getLocationByTick(self, tick: int, earlier_loc: int = 0) -> int: ...
