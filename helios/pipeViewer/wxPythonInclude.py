# Small util to print the include path for wxPython

import pkgutil
import os.path
from _frozen_importlib_external import SourceFileLoader

wx_pkg = pkgutil.get_loader('wx')
if wx_pkg is None:
    raise RuntimeError("wx python library is not installed!")
assert isinstance(wx_pkg, SourceFileLoader)

print(os.path.join(os.path.dirname(wx_pkg.get_filename()), 'include'))
