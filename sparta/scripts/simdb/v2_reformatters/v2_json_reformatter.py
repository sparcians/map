import math

class v2JsonReformatterBase:
    def ReformatValue(self, val):
        orig_val = val
        val = float(val) if isinstance(val, str) else val
        if val == 0:
            return "0"

        exponent = math.floor(math.log10(abs(val))) if val != 0 else 0
        if exponent > 0 or abs(exponent) >= 6:
            return self.__TrimExponent(str(orig_val))

        num_digits = 10 + abs(exponent)
        val = f"{val:.{num_digits}f}".rstrip('0').rstrip('.')
        return self.__TrimExponent(val)

    def __TrimExponent(self, val):
        # Turn scientific notation like "1.23e-08" into "1.23e-8"
        if 'e-0' in val:
            return val.replace('e-0', 'e-')
        if 'e+0' in val:
            return val.replace('e+0', 'e+')
        if 'E-0' in val:
            return val.replace('E-0', 'E-')
        if 'E+0' in val:
            return val.replace('E+0', 'E+')
        return val