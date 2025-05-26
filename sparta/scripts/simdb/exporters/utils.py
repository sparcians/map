import math

def FormatNumber(val, decimal_places=-1, as_string=True):
    if math.isnan(val):
        return "nan" if as_string else float('nan')
    elif math.isinf(val):
        return str(val) if as_string else val  # Python will format as 'inf' or '-inf'
    else:
        fractional, _ = math.modf(val)
        if fractional == 0:
            return str(int(val)) if as_string else int(val)  # Convert to int to avoid ".0" in output
        else:
            if decimal_places >= 0:
                val = round(val, decimal_places)
            else:
                # Format to 6 significant digits
                val = format(val, ".6g")

            return val if as_string else float(val)
