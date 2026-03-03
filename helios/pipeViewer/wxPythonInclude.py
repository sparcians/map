# Small util to print the include path for wxPython

import os.path
from _frozen_importlib_external import SourceFileLoader

import importlib.util
wx_pkg = importlib.util.find_spec('wx')

if wx_pkg is None:
    raise RuntimeError("wx python library is not installed!")
assert isinstance(wx_pkg.loader, SourceFileLoader)

print(os.path.join(os.path.dirname(wx_pkg.loader.get_filename()), 'include'))
