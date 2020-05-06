###########################################################################
# vars.cmd
#
# This file contains general variable setup and is intended to be included
# from other *.cmd files
#

#--------------------------------------------------
# General configuration
#
ht		= 13		# Height for single-character element
ht_spc		= $ht - 1	# Default spacing between vertically adjacent elements (default = $height - 1)

mht		= int($ht/3)	# Height for mini-element
mht_spc		= $mht - 1	# Default spacing between vertically adjacent elements (default = $mheight - 1)

start_t		= -10		# Starting (leftmost) cycle offset
dir_t		= 0		# Cycle corresponding to the detailed directory

grid_color      = "gray75"	# Normal grid color

sep_ht		= 1		# Height of horizontal separator
sep_wt		= 1		# Width of vertical separator
sep_interval	= 4		# Every n pipe stages, output a horizontal separator
sep_color	= "black"	# Separator color

ver_sep_interval = 0		# Every n cycles, output a vertical separator
ver_sep_interval_color = "gray50"

top_margin	= 2 		# Top margin, in pixels
left_margin	= 10		# Left margin, in pixels

cap_wt		= 90		# Caption width, in pixels

column_sep	= 15		# Separator between columns


#------------------------- Pipeline column variables
#num_t		= 41		# Number of cycles to generate
onechar_width	= 9		# Width for single-character element
pixel_offset    = -($start_t) * ($onechar_width - 1)            # How many pixels from the left edge to place the zero-offset


#--------------------------------------------------
# Cycle legend configuration
#
cycle_legend_interval		= 5
cycle_legend_current_wt		= $cap_wt - 25
cycle_legend_color		= 1

cycle_disp_width = 50
