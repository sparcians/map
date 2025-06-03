import re
from .v2_json_reformatter import v2JsonReformatterBase

class v2JsonDetailReformatter(v2JsonReformatterBase):
    def __init__(self):
        v2JsonReformatterBase.__init__(self)

    def Reformat(self, dest_file, report_style):
        reformatted_lines = []

        # JSON detail reports always start with this:
        #
        #     {
        #         "_id": " ",
        #
        # And need to start with this:
        #
        #    {  "_id": " ",
        reformatted_lines.append('{ "_id": " ",\n')

        with open(dest_file, 'r', encoding='utf-8') as file:
            lines = file.readlines()

            # Start at the third line, since the first two lines are already handled.
            for line in lines[2:]:
                # See if the current line is of the form:
                #   "name": "some.path.to.a.stat",
                #
                # Where the "name" is exact, and the "some.path.to.a.stat" is any path.
                match = re.match(r'^\s*"name":\s*"([^"]+)"\s*,\s*$', line)

                # If it is, then we need to write the previous line + the current line.
                # Otherwise, we just write the current line.
                #
                # Example before reformatting:
                #
                #    {
                #        "name": "decode.AUTO.FetchQueue_utililization_UF",
                #        "desc": "underflow bin",
                #        "vis": "100000000",
                #        "class": "50"
                #    }
                #
                # After reformatting, it should look like this:
                #
                #    { "name": "decode.AUTO.FetchQueue_utililization_UF",
                #      "desc": "underflow bin",
                #      "vis": "100000000",
                #      "class": "50"
                #    }
                if match:
                    reformatted_line = reformatted_lines[-1]
                    reformatted_line = reformatted_line.rstrip() + ' '
                    reformatted_line += line.strip() + '\n'
                    reformatted_lines[-1] = reformatted_line
                else:
                    # Just write the current line
                    reformatted_lines.append(line)

        # Pop the last line if it is empty, as it is not needed.
        if reformatted_lines and reformatted_lines[-1].strip() == '':
            reformatted_lines.pop()

        # Write the reformatted lines back to the file
        with open(dest_file, 'w', encoding='utf-8') as fout:
            for line in reformatted_lines:
                fout.write(line)
