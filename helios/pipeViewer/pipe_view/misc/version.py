# @package version.py
#  @brief Argos version finder

from __future__ import annotations
import core  # Argos core


# Attempts to determine the version of this argos by its .VERSION file
def get_version() -> int:
    return core.get_argos_version()
