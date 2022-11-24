# # @package elementpropertiesvalidation
#  This module exists to pair with element.py and validate anything that is
#  being attempted to set as a value for one of an Element's properties

from . import content_options as content
# used for tuplifying
import string
import ast
from typing import List, Optional, Tuple, TypeVar, Union, cast

# # A listing of the available options for the 'Content' field of an Element
__CONTENT_OPTIONS = content.GetContentOptions()


# # Returns the listing of options for the 'Content' field of an Element
def GetContentOptions() -> List[str]:
    return __CONTENT_OPTIONS

StringTuple = Union[str, Tuple[int, int]]

# # Confirms that a position is in the form (int, int) for (x,y)
def validatePos(name: str, raw: StringTuple) -> Tuple[int, int]:
    if isinstance(raw, str):
        val = tuplify(raw)
    else:
        val = raw
    if not isinstance(val, tuple):
        raise TypeError(f'Parameter {name} must be a 2-tuple of ints')
    if len(val) != 2:
        raise ValueError(f'Parameter {name} must be 2-tuple of ints')
    if not isinstance(val[0], int):
        raise TypeError(f'Parameter {name}: only integers allowed for x-coords')
    if not isinstance(val[1], int):
        raise TypeError(f'Parameter {name}: only integers allowed for y-coords')
    val = cast(Tuple[int, int], val)
    return val


# # Confirms that dimensions are in the form (int, int) for (width, height)
def validateDim(name: str, raw: StringTuple) -> Tuple[int, int]:
    if isinstance(raw, str):
        val = tuplify(raw)
    else:
        val = raw
    if not isinstance(val, tuple):
        raise TypeError('Parameter ' + name + ' must be a 2-tuple of ints')
    if len(val) != 2:
        raise ValueError('Parameter ' + name + ' must be 2-tuple of ints')
    if not(val[0] > 0 and val[1] > 0):
        raise ValueError('Parameter ' + name + ' must only have positive values')
    if not isinstance(val[0], int):
        raise TypeError('Parameter ' + name + ': only integers allowed for width')
    if not isinstance(val[1], int):
        raise TypeError('Parameter ' + name + ': only integers allowed for height')
    val = cast(Tuple[int, int], val)
    return val


# # Confirms that a color is in the form (int, int, int) for (R,G,B)
def validateColor(name: str, raw: Union[str, Tuple[int, int, int]]) -> Tuple[int, int, int]:
    if isinstance(raw, str):
        val = tuplify(raw)
    else:
        val = raw
    if not isinstance(val, tuple):
        raise TypeError('Parameter ' + name + ' must be a 3-tuple of ints')
    if len(val) != 3:
        raise ValueError('Parameter ' + name + ' must be 3-tuple of ints')
    if not isinstance(val[0], int):
        raise TypeError('Parameter ' + name + ': only integers allowed for red')
    if not isinstance(val[1], int):
        raise TypeError('Parameter ' + name + ': only integers allowed for green')
    if not isinstance(val[2], int):
        raise TypeError('Parameter ' + name + ': only integers allowed for blue')
    if not 0 <= val[0] <= 255:
        raise ValueError('Parameter ' + name + ': red must be between 0 & 255')
    if not 0 <= val[1] <= 255:
        raise ValueError('Parameter ' + name + ': green must be between 0 & 255')
    if not 0 <= val[2] <= 255:
        raise ValueError('Parameter ' + name + ': blue must be between 0 & 255')
    val = cast(Tuple[int, int, int], val)
    return val


# # Confirms that an LocationString is a str
def validateLocation(name: str, val: str) -> str:
    val = str(val)
    if not isinstance(val, str):
        raise TypeError('Parameter ' + name + ' must be an str')
    return val


# # Confirms that the Content specification is a str corresponding to the
#  available options
def validateContent(name: str, val: str) -> str:
    if not isinstance(val, str):
        raise TypeError('Parameter ' + name + ' must be a str')
    if val not in __CONTENT_OPTIONS:
        raise ValueError('Parameter ' + name + ' must be one of the options: '
        +str(__CONTENT_OPTIONS))
    return val


# ## Confirms that the clock offset is valid
def validateClockOffset(name: str, raw: StringTuple) -> Tuple[int, int]:
    if isinstance(raw, str):
        val = tuplify(raw)
    else:
        val = raw

    if not isinstance(val, tuple):
        raise TypeError('Parameter ' + name + ' must be a tuple of (clock, cycles)')

    val = cast(Tuple[int, int], val)
    return val


# ## Confirms that scale factor is an int
def validateTimeScale(name: str, raw: Union[str, float, int]) -> float:
    try:
        val = float(raw)
    except:
        raise TypeError('Parameter ' + name + ' must be a number')
    return val


# Confirms that an offset is an int
def validateOffset(name: str, raw: Union[str, int]) -> int:
    val: Union[str, int]
    if isinstance(raw, str) and isNumeral(raw):
        val = int(raw)
    else:
        val = raw
    if not isinstance(val, int):
        raise TypeError('Parameter ' + name + ' must be a int')
    return val


# # Confirms this is a string
#  Treats None objects as empty string
def validateString(name: str, val: Optional[str]) -> str:
    if val is None:
        val = ''
    else:
        val = str(val)
        if not isinstance(val, str):
            raise TypeError('Parameter ' + name + ' must be a str')
    return val


# # Confirms this is a bool and converts if necessary
def validateBool(name: str, val: Optional[bool]) -> bool:
    if val is None:
        val = False
    else:
        val = not not val
    return val


# # Confirms this is list
T = TypeVar('T')
def validateList(name: str, val: Union[str, List[T]]) -> List[T]:
    if isinstance(val, str):
        val = ast.literal_eval(val)
    else:
        val = list(val)
    if not isinstance(val, list):
        raise TypeError('Parameter ' + name + ' must be a list')
    return val


# # Confirms that an X/Y scale value is in the form (number, number)
def validateScale(name: str, raw: StringTuple) -> Tuple[int, int]:
    if isinstance(raw, str):
        val = tuplify(raw)
    else:
        val = raw
    if not isinstance(val, tuple):
        raise TypeError(f'Parameter {name} must be a 2-tuple of numbers')
    if len(val) != 2:
        raise ValueError(f'Parameter {name} must be 2-tuple of numbers')
    if not isinstance(val[0], (int, float)):
        raise TypeError(f'Parameter {name}: only numbers allowed for x-scale factors')
    if not isinstance(val[1], (int, float)):
        raise TypeError(f'Parameter {name}: only numbers allowed for y-scale factors')
    val = cast(Tuple[int, int], val)
    return val


# Takes a string (of supposed user input) and converts it, if possible, to a
#  tuple of ints. Floats are currently discarded and disregarded
def tuplify(raw: str) -> Tuple[int, ...]:
    # strip away any leading characters that are not numeric digits
    strip = string.whitespace + string.ascii_letters + string.punctuation
    strip = strip.replace('-', '')
    strip = strip.replace(',', '')
    temp_str = raw.strip(strip)
    temp = [t.split(' ') for t in temp_str.split(',')]
    nums = []
    for element in temp:
        for s in element:
            if isNumeral(s):
                nums.append(int(s))
    val = tuple(nums)
    return val


# # A simple helper method for tuplify(), providing a level of abstraction for
#  checking that every character within a string is a digit (base 10)
def isNumeral(s: str) -> bool:
    options = string.digits + '-'
    res = True
    for char in s:
        if char not in options:
            res = False
    if len(s) == 0:
        res = False
    return res
