import re

__uop_uid_regex = re.compile('(?:annotation:)?\s*([0-9a-f]{3}\s+U[0-9]+)')


# @profile
def __AnnotationToUopUid(anno_string):
    uid_match = __uop_uid_regex.match(anno_string)

    if uid_match is not None:
        return uid_match.group(1)
    else:
        return None


# @profile
def GetUopUid(anno_string):
    if not isinstance(anno_string, str):
        return None
    uop_uid_str = __AnnotationToUopUid(anno_string)
    if uop_uid_str is not None:
        return hash(uop_uid_str)
    else:
        return None

