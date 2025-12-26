import json, re
from .v2_json_reformatter import v2JsonReformatterBase

class v2JsonReformatter(v2JsonReformatterBase):
    def __init__(self, stat_val_pattern=None):
        v2JsonReformatterBase.__init__(self)

        # Subclasses may provide a custom regex pattern to match
        # statistics values that need to be reformatted.
        self.stat_val_pattern = stat_val_pattern

        if not self.stat_val_pattern:
            # The default json format displays stat values with this pattern:
            #   "val": <value>
            self.stat_val_pattern = re.compile(
                r'^(?P<indent>\s*)"(?P<key>[^"]+)"\s*:\s*(?P<value>[+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)(?P<trailing_comma>\s*,?)\s*$'
            )

    def Reformat(self, dest_file, report_style):
        if not self.stat_val_pattern:
            return

        with open(dest_file, 'r', encoding='utf-8') as fin:
            lines = fin.readlines()

        reformatted_lines = []
        for line in lines:
            reformatted_lines.append(self.__ReformatLine(line.rstrip()))

        with open(dest_file, 'w', encoding='utf-8') as fout:
            fout.write('\n'.join(reformatted_lines))

    def __ReformatLine(self, line):
        match = self.stat_val_pattern.match(line)
        if match:
            indent = match.group("indent")
            key    = match.group("key")
            value  = match.group("value")
            comma  = match.group("trailing_comma")

            # Reformat value according to MAP v2 standards
            new_value = self.ReformatValue(value)
            new_line = f'{indent}"{key}": {new_value}{comma}'
            return new_line
        else:
            return line
