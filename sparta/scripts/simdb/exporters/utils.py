import math

def FormatNumber(val, float_scinot_allowed=True, decimal_places=-1):
    if math.isnan(val):
        return "nan"
    elif math.isinf(val):
        return str(val)  # Python will format as 'inf' or '-inf'
    else:
        fractional, integral = math.modf(val)
        if fractional == 0:
            return str(int(val))  # Convert to int to avoid ".0" in output
        else:
            if decimal_places >= 0:
                format_spec = f".{decimal_places}f" if not float_scinot_allowed else f".{decimal_places}g"
                return format(val, format_spec)
            else:
                # Format to 6 significant digits
                return format(val, ".6g")

