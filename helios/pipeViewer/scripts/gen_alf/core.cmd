###########################################################################
# core.cmd
#
# This file contains commands to generate the FLA file which will be
# converted into an ALF file
#
# pipe[mstic]	"My Caption"	base.name	start	[step]	end
# - m = minipipe
# - s = follow this unit with space padding
# - t = truncate:  last index uses (index-1) value
# - i = output indexed caption per element
# - c = colorize only; don't munge display string
#
# [var] in front of a command means to only evaluate the command if "var" is true
#

#--------------------------------------------------
# General configuration
#
include		vars.cmd


########################################################################### Start
print	---

caption_x       = $left_margin


########################################################################### Multi-cycle pipeline column
y       	= $top_margin
content_base_x	= $caption_x + $cap_wt - 1	# Base X-offset of content display
wt      	= $onechar_width		# Element width
wt_spc		= $wt - 1			# Default spacing between horizontally adjacent elements
dir_mode	= 0

#-------------------------------------------------- Show core pipeline elements
include		core_column.cmd

#-------------------------------------------------- Show cycles legend
show_cycles
show_ver_sep

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Detailed pipeline column
y       	= $top_margin
content_base_x	= $caption_x + $cap_wt - 1	# Base X-offset of content display
wt      	= 160				# Width of detailed entry
wt_spc		= $wt - 1			# Default spacing between horizontally adjacent elements
dir_mode	= 1

#-------------------------------------------------- Show core pipeline elements
include		core_column.cmd

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Directory column
y		= $top_margin
content_base_x	= $caption_x + $cap_wt - 1	# Base X-offset of content display
wt		= 3200				# Width of directory entry
wt_spc		= $wt - 1			# Default spacing between horizontally adjacent elements
dir_mode	= 1

#-------------------------------------------------- ROB
pipeis	"ROB"		rob.ReorderBuffer.ReorderBuffer				29	-1	0

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Tail
printlast	...
