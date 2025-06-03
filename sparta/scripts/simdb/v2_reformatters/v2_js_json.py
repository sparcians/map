import json, io
from .v2_json_reformatter import v2JsonReformatterBase

class v2JsJsonReformatter(v2JsonReformatterBase):
    def __init__(self):
        v2JsonReformatterBase.__init__(self)

    def Reformat(self, dest_file, report_style):
        with open(dest_file, 'r', encoding='utf-8') as fin:
            data = json.load(fin)

        out = io.StringIO()
        out.write('{\n')

        indent = 2
        decimal_places = int(report_style.get('decimal_places', 2))
        for key, value in data.items():
            self.__WriteLevel(out, key, value, decimal_places, indent)

        out.write('}')
        with open(dest_file, 'w', encoding='utf-8') as fout:
            fout.write(out.getvalue())

        with open(dest_file, 'r', encoding='utf-8') as fin:
            lines = fin.readlines()

        lines = [line.rstrip() + '\n' for line in lines if line.strip()]

        # Compare the indentation level for all lines. If two adjacent lines
        # have the same indentation level, then we need to append a comma
        # to the previous line.
        for i in range(1, len(lines)):
            if lines[i].strip() and lines[i-1].strip():
                prev_indent = len(lines[i-1]) - len(lines[i-1].lstrip())
                curr_indent = len(lines[i]) - len(lines[i].lstrip())
                if prev_indent == curr_indent:
                    lines[i-1] = lines[i-1].rstrip() + ',\n'

        # Append a whitespace character for each list element in 'ordered_keys'
        # (except the last element).
        in_ordered_keys = False
        for i in range(len(lines)):
            if lines[i].find("ordered_keys") != -1:
                in_ordered_keys = True
                continue
            elif in_ordered_keys and lines[i].rstrip().endswith(','):
                lines[i] = lines[i].rstrip() + ' \n'
            elif in_ordered_keys:
                in_ordered_keys = False

        # Write the final output to the file
        with open(dest_file, 'w', encoding='utf-8') as fout:
            fout.write(''.join(lines))

    def __WriteLevel(self, out, key, value, decimal_places, indent):
        if key in ('units', 'vis', 'siminfo'):
            out.write(' '*indent + f'"{key}" : ')
        else:
            out.write(' '*indent + f'"{key}": ')

        if isinstance(value, dict):
            if set(value.keys()) == {'val', 'vis', 'desc'}:
                val = float(value['val'])
                vis = value['vis']
                desc = value['desc']

                def is_whole_number(x):
                    return isinstance(x, (int, float)) and x == int(x)

                # Rule 1: Whole numbers should be printed as integers
                if is_whole_number(val):
                    out.write('{ "val" : ' + str(int(val)) + ', ')

                # Rule 2: Fractional numbers should be printed with a fixed number of decimal places
                else:
                    out.write('{ "val" : ' + f"{val:.{decimal_places}f}" + ', ')

                out.write('"vis" : ' + str(vis) + ', ')
                out.write('"desc" : "' + desc + '"}\n')
            elif key == 'vis':
                hidden = value['hidden']
                support = value['support']
                detail = value['detail']
                normal = value['normal']
                summary = value['summary']

                out.write('{\n')
                out.write(' ' * (indent + 2) + f'"hidden"  : {hidden}\n')
                out.write(' ' * (indent + 2) + f'"support" : {support}\n')
                out.write(' ' * (indent + 2) + f'"detail"  : {detail}\n')
                out.write(' ' * (indent + 2) + f'"normal"  : {normal}\n')
                out.write(' ' * (indent + 2) + f'"summary" : {summary}\n')
                out.write(' ' * indent + '}\n')
            elif key == 'siminfo':
                out.write('{\n')
                for sub_key, sub_value in value.items():
                    out.write(' ' * (indent + 2))
                    if isinstance(sub_value, str):
                        out.write(f'"{sub_key}" : "{sub_value}"\n')
                    else:
                        out.write(f'"{sub_key}" : {sub_value}\n')

                out.write(' ' * indent)
                out.write('}\n')
            elif key == 'report_metadata':
                out.write('{\n')
                longest_sub_key = max(len(sub_key) for sub_key in value.keys())
                for sub_key, sub_value in value.items():
                    sub_key = f'"{sub_key}"'
                    sub_key = sub_key.ljust(longest_sub_key + 2)
                    out.write(' ' * (indent + 2))
                    if isinstance(sub_value, str):
                        out.write(f'{sub_key}: "{sub_value}"\n')
                    else:
                        out.write(f'{sub_key}: {sub_value}\n')

                out.write(' ' * indent)
                out.write('}\n')
            else:
                out.write('{\n')
                for sub_key, sub_value in value.items():
                    self.__WriteLevel(out, sub_key, sub_value, decimal_places, indent + 2)

                out.write(' ' * indent)
                out.write('}\n')
        elif isinstance(value, list):
            out.write('[\n')
            for item in value:
                out.write(' ' * (indent + 2))
                out.write(f'"{item}" \n')

            out.write(' ' * indent)
            out.write(']\n')
