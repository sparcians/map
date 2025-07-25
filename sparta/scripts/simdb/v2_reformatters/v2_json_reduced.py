import re
from .v2_json import v2JsonReformatter

class v2JsonReducedReformatter(v2JsonReformatter):
    def __init__(self):
        # The only tweaks we need to do is to reformat some statistics values.
        # Look for lines that match this regex:
        #
        #     "stat_name": float_value
        #
        # We will only reformat the floating point value to be the same as MAP v2.
        pattern = re.compile(
            r'^(?P<indent>\s*)"(?P<key>[^"]+)"\s*:\s*(?P<value>[+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)(?P<trailing_comma>\s*,?)\s*$'
        )

        v2JsonReformatter.__init__(self, stat_val_pattern=pattern)
