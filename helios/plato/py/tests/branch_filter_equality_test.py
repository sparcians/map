# Check that branch filters can be tested for equality

from matplotlib import pyplot as plt
import numpy as np
from os import path


import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.branch_common import *


branch_filter = {"addresses":
                     [
                      {"address":
                           {"type": "Address",
                            "addr": "0x2bfe13ff2e"
                            },
                       "include": False,
                       },
                      {"address":
                           {"type": "MaskedAddress",
                            "addr": "0x2bfe13ff00",
                            "mask": "0xffffffffffffff00"
                            },
                       "include": True,
                       },
                      ],
                  "classes": {
                      ##"directness": "direct",
                      ##"conditionality": "unconditional"
                  }
                 }

f1 = make_branch_filter(branch_filter)
f2 = make_branch_filter(branch_filter)
assert(f1 == f2)


branch_filter = {"addresses":
                     [
                      {"address":
                           {"type": "Address",
                            "addr": "0x2bfe13ff2f" # SINGLE CHANGE
                            },
                       "include": False,
                       },
                      {"address":
                           {"type": "MaskedAddress",
                            "addr": "0x2bfe13ff00",
                            "mask": "0xffffffffffffff00"
                            },
                       "include": True,
                       },
                      ],
                  "classes": {
                      ##"directness": "direct",
                      ##"conditionality": "unconditional"
                  }
                 }

f3 = make_branch_filter(branch_filter)
assert(f3 != f2)

branch_filter = {"addresses":
                     [
                      {"address":
                           {"type": "Address",
                            "addr": "0x2bfe13ff2f"
                            },
                       "include": False,
                       },
                      {"address":
                           {"type": "MaskedAddress",
                            "addr": "0x2bfe13ff00",
                            "mask": "0xffffffffffffff00"
                            },
                       "include": True,
                       },
                      ],
                  "classes": {
                      "directness": "direct",  # SINGLE CHANGE
                      ##"conditionality": "unconditional"
                  }
                 }

f4 = make_branch_filter(branch_filter)
assert(f4 != f3 and f4 != f2)

print('All good')