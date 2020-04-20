###########################################################################
# exe.cmd
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

#-------------------------------------------------- Show execution pipeline elements
include		exe_column.cmd

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

#-------------------------------------------------- Show execution pipeline elements
include		exe_column.cmd

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Scheduler column
y       	= $top_margin
content_base_x	= $caption_x + $cap_wt - 1	# Base X-offset of content display
wt      	= $onechar_width		# Width of scheduler entry
wt_spc		= $wt - 1			# Default spacing between horizontally adjacent elements
num_t		= 11
start_t		= -5
pixel_offset    = -($start_t) * ($onechar_width - 1)            # How many pixels from the left edge to place the zero-offset
dir_mode	= 0

#-------------------------------------------------- Show schedulers
pipeis		"B"	b.g.s.s_array		0	7
pipeis		"I"	a.g.S.s_array		0	15
pipeis		"I"	a.g.s.s_array		0	15
pipeis		"I"	a.g.s.s_array		0	7
#pipeis		"S"	s.i.s.s_array		0	7
pipeis		"L"	l.g.s.s_array			0	7
pipeis		"S"	s.g.s.s_array			0	7
pipeis		"S"	s.g.s.s_array		0	7
pipeis		"L"	l.l.l.ls_q					11	-1	0
pipel           "C"         r.r.r.r

pipeis		"F"	f.f.s.s		0	31
pipeis		"F"	f.f.s.s		0	7

#-------------------------------------------------- Show cycles legend
pipel           "C"         r.r.r.r

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Directory column
y		= $top_margin
content_base_x	= $caption_x + $cap_wt - 1	# Base X-offset of content display
wt		= 3200				# Width of directory entry
wt_spc		= $wt - 1			# Default spacing between horizontally adjacent elements
dir_mode	= 1

#-------------------------------------------------- ROB
pipeis		"R"		r.r.r.r				0	75

#-------------------------------------------------- Done with this column
caption_x	= $column_right_x + $column_sep


########################################################################### Tail
printlast	...
