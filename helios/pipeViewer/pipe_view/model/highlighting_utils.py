from __future__ import annotations
import re
from typing import Optional, Union

__UOP_UID_REGEX = re.compile(r'.*\buid\(([0-9]+)\)')

# @profile
def GetUopUid(anno_string: Optional[Union[int, str]]) -> Optional[int]:
    if not isinstance(anno_string, str):
        return None

    uid_match = __UOP_UID_REGEX.search(anno_string)

    if uid_match is not None:
        return int(uid_match.group(1))
    else:
        return None
