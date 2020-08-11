###########################################################################
# core_column.cmd
#
# This file shows one column of the core layout and is intended to be included from core.cmd.
#

#-------------------------------------------------- Fetch
pipecs	"Next PC"	fetch.next_pc

#-------------------------------------------------- Decode
pipem	"Decode"	decode.FetchQueue.FetchQueue				9	-1	4
pipeis	"Decode"	decode.FetchQueue.FetchQueue				3	-1	0

#-------------------------------------------------- Rename
pipem	"Rename"	rename.rename_uop_queue.rename_uop_queue	9	-1	4
pipeis	"Rename"	rename.rename_uop_queue.rename_uop_queue	3	-1	0

#-------------------------------------------------- Dispatch
pipem	"Dispatch"	dispatch.dispatch_queue.dispatch_queue		9	-1	4
pipeis	"Dispatch"	dispatch.dispatch_queue.dispatch_queue		3	-1	0

pipec	"FPU  cred"	dispatch.in_fpu_credits
pipec	"ALU0 cred"	dispatch.in_alu0_credits
pipec	"ALU1 cred"	dispatch.in_alu1_credits
pipec	"BR   cred"	dispatch.in_br_credits
pipec	"LSU  cred"	dispatch.in_lsu_credits
pipec	"ROB  cred"	dispatch.in_reorder_buffer_credits
pipecs	"ROB flush"	dispatch.in_reorder_flush

#-------------------------------------------------- ALU0
pipem	"ALU0 Sch"	alu0.scheduler_queue.scheduler_queue		7	-1	4
pipeis	"ALU0 Sch"	alu0.scheduler_queue.scheduler_queue		3	-1	0

#-------------------------------------------------- ALU1
pipem	"ALU1 Sch"	alu1.scheduler_queue.scheduler_queue		7	-1	4
pipeis	"ALU1 Sch"	alu1.scheduler_queue.scheduler_queue		3	-1	0

#-------------------------------------------------- FPU
pipem	"FPU Sch"	fpu.scheduler_queue.scheduler_queue			7	-1	4
pipeis	"FPU Sch"	fpu.scheduler_queue.scheduler_queue			3	-1	0

#-------------------------------------------------- BR
pipem	"BR Sch"	br.scheduler_queue.scheduler_queue			7	-1	4
pipeis	"BR Sch"	br.scheduler_queue.scheduler_queue			3	-1	0

#-------------------------------------------------- LSU
pipecs	"DL1 busy"	lsu.dcache_busy
pipeis	"LSU Pipe"	lsu.LoadStorePipeline.LoadStorePipeline		0	2

pipem	"LSU IQ"	lsu.lsu_inst_queue.lsu_inst_queue			7	-1	4
pipeis	"LSU IQ"	lsu.lsu_inst_queue.lsu_inst_queue			3	-1	0

#-------------------------------------------------- Retire
pipem	"ROB"		rob.ReorderBuffer.ReorderBuffer				29	-1	8
pipeis	"ROB"		rob.ReorderBuffer.ReorderBuffer				7	-1	0
