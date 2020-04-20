#!/usr/bin/env python

# # @package Metrics.py
#  @brief Argos tool metrics controller

show_info = False


# # Indicates the tool is starting
def starting():
    try:
        # Track usage. Fails silently if module cannot be found
        import tmet.tmet1 as tm
        tm.UsageInfo('argos', tm.ACTION_START_TOOL).submit(not show_info)
    except:
        pass # Ignore all error
