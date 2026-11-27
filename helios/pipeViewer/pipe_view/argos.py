#!/usr/bin/env python3

# @package argos.py
#  @brief Legacy startup wrapper for Argos.
#
#  The Argos viewer has moved to Sparta's submodule "SimDB". This script exists only to
#  forward all command line arguments to the new argos.py so existing callers keep working.
#
#  New entry point:
#    map/sparta/simdb/python/argos/argos.py

import os
import sys

# Resolve the new entry point relative to this file so the wrapper works
# regardless of the current working directory.
_THIS_DIR = os.path.dirname(os.path.realpath(__file__))
_NEW_ARGOS = os.path.normpath(
    os.path.join(
        _THIS_DIR,
        '..', '..', '..',
        'sparta', 'simdb', 'python', 'argos', 'argos.py',
    )
)

if __name__ == '__main__':
    if not os.path.isfile(_NEW_ARGOS):
        sys.stderr.write(
            f'error: could not find the new argos entry point at '
            f'"{_NEW_ARGOS}"\n'
        )
        sys.exit(1)

    # Replace this process with the new script, forwarding all arguments.
    os.execv(sys.executable, [sys.executable, _NEW_ARGOS, *sys.argv[1:]])
