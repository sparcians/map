# # @package version.py
#  @brief Argos version finder

from __future__ import annotations
import os
import core # Argos core


# # Attempts to determine the version of this argos by its .VERSION file
def get_version() -> int:
	return core.get_argos_version()

	# Read the .VERSION file
	# #join = os.path.join
	# #dirname = os.path.dirname
	# #abspath = os.path.abspath
	# #version_file = join(dirname(abspath(__file__)), '../../.VERSION')
	# #try:
	# #	with open(version_file) as vf:
	# #		verstr = vf.readline().strip()
	# #		return verstr
	# #except IOError as ex:
	# #	return 'unknown'
