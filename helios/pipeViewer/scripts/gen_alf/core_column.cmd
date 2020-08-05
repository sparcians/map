###########################################################################
# exe_column.cmd
#
# This file shows one column of execution layout and is intended to be included from exe.cmd.
#

#-------------------------------------------------- Dispatch
pipems		"DispatchQ"	dispatch.dispatch_queue.dispatch_queue			11	-1	0
pipeis		"Dispatch"	dispatch.dispatch_					3	-1	0
pipes		"DispStall"	dispatch.dispatch_stall_reason

#-------------------------------------------------- SPR
#pipecs		"Spr Cred"	dispatch.dispatch_spr_credits
#pipems		"Spr Sch"	spr_unit.integer_scheduler.scheduler_array.scheduler_array		0	7
#pipe		"Spr Pick"	spr_unit.integer_scheduler.scheduler_pick_0
#pipe		"Spr Reg"	spr_unit.register_file_read_0.registerFileRead
#pipes		"Spr Exe"	spr_unit.execution_pipeline_0.executionPipeline

#-------------------------------------------------- Branch
pipecs		"Br Cred"	dispatch.dispatch_branch_credits
pipems		"Br Sch"	branch_unit_0.generic_scheduler.scheduler_array.scheduler_array		0	7
pipe		"Br Pick"	branch_unit_0.generic_scheduler.scheduler_pick_0
pipe		"Br Reg"	branch_unit_0.register_file_read_0.registerFileRead
pipes		"Br Exe"	branch_unit_0.execution_pipeline_0.executionPipeline

#-------------------------------------------------- ALU_A_0
pipecs		"ALU_A_0 Cred"	dispatch.dispatch_alu_a_0_credits
pipems		"ALU_A_0 Sch"	alu_a_0.generic_scheduler.scheduler_array.scheduler_array		0	7
pipe		"ALU_A_0 Pick"	alu_a_0.generic_scheduler.scheduler_pick_0
pipe		"ALU_A_0 Reg"	alu_a_0.register_file_read_0.registerFileRead
pipes		"ALU_A_0 Exe"	alu_a_0.execution_pipeline_0.executionPipeline

#-------------------------------------------------- ALU_A_1
pipecs		"ALU_A_1 Cred"	dispatch.dispatch_alu_a_1_credits
pipems		"ALU_A_1 Sch"	alu_a_1.generic_scheduler.scheduler_array.scheduler_array		0	7
pipe		"ALU_A_1 Pick"	alu_a_1.generic_scheduler.scheduler_pick_0
pipe		"ALU_A_1 Reg"	alu_a_1.register_file_read_0.registerFileRead
pipes		"ALU_A_1 Exe"	alu_a_1.execution_pipeline_0.executionPipeline

#-------------------------------------------------- ALU_X_0
#pipecs		"Int C0 Cred"	dispatch.dispatch_alu_c_0_credits
pipec		"Int X0 Cred"	dispatch.dispatch_alu_x_0_credits
pipems		"Int 2 Sch"	alu_x_0.generic_scheduler.scheduler_array.scheduler_array		0	7
pipe		"Int 2 Pick"	alu_x_0.generic_scheduler.scheduler_pick_0
pipe		"Int 2 Reg"	alu_x_0.register_file_read_0.registerFileRead
pipes		"Int 2 Exe"	alu_x_0.execution_pipeline_0.executionPipeline

#-------------------------------------------------- SPR Unit
#pipecs		"SPR Cred"	dispatch.dispatch_spr_credits
#pipems		"SPR Sch"	spr_unit.integer_scheduler.scheduler_array.scheduler_array		0	7

#-------------------------------------------------- Show cycles
pipel           "Cycle"         retire.rob_t0.rob_age_ordered.rob_age_ordered0

#-------------------------------------------------- Load AGU
#pipecs		"LD AGU Cred"	dispatch.dispatch_load_agu0_credits
#pipems		"LD AGU Sch"	load_agu0.generic_scheduler.scheduler_array.scheduler_array			0	7
#pipe		"LD AGU Pick"	load_agu0.generic_scheduler.scheduler_pick_0
#pipe		"LD AGU Reg"	load_agu0.register_file_read_0.registerFileRead
#pipes		"LD AGU Exec"	load_agu0.execution_pipeline_0.executionPipeline

#-------------------------------------------------- Store AGU
#pipecs		"ST AGU Cred"	dispatch.dispatch_store_agu0_credits
#pipems		"ST AGU Sch"	store_agu0.generic_scheduler.scheduler_array.scheduler_array			0	7
#pipe		"ST AGU Pick"	store_agu0.generic_scheduler.scheduler_pick_1
#pipe		"ST AGU Reg"	store_agu0.register_file_read_1.registerFileRead
#pipes		"ST AGU Exec"	store_agu0.execution_pipeline_1.executionPipeline

#-------------------------------------------------- SD
pipecs		"SD Cred"	dispatch.dispatch_sd_sched_credits
pipems		"SD Sch"	store_data_unit.generic_scheduler.scheduler_array.scheduler_array		0	7
pipe		"SD Pick"	store_data_unit.generic_scheduler.scheduler_pick_0
pipe		"SD Reg"	store_data_unit.register_file_read_0.registerFileRead
pipes		"SD Exec"	store_data_unit.execution_pipeline_0.executionPipeline

#-------------------------------------------------- LS
pipecs		"LS Cred"	dispatch.dispatch_ls_sched0_credits
pipems		"LS Sch"	lsu.ls_sched0.ls_sched_q.ls_sched_q					11	-1	0

#-------------------------------------------------- FP
pipecs		"FP CS Cred"	dispatch.dispatch_fp_credits
pipecs		"FP FS Cred"	fp_unit_0.fp_coarse_scheduler.fs_credit
pipe      "Water Mark"   fp_unit_0.fp_coarse_scheduler.watermark
pipems		"FPcrs Sch"	fp_unit_0.fp_coarse_scheduler.scheduler_array.scheduler_array		0	31
pipe		"FPcrs Pick0"	fp_unit_0.fp_coarse_scheduler.scheduler_pick_0
pipe		"FPcrs Pick1"	fp_unit_0.fp_coarse_scheduler.scheduler_pick_1
pipes		"FPcrs Pick2"	fp_unit_0.fp_coarse_scheduler.scheduler_pick_2

pipems		"FPfine Sch"	fp_unit_0.fp_fine_scheduler_0.scheduler_array.scheduler_array		0	7
pipe		"FPfine Pk Pipe0"	fp_unit_0.fp_fine_scheduler_0.scheduler_pick_0
pipe		"FPfine Pk Pipe1"	fp_unit_0.fp_fine_scheduler_0.scheduler_pick_1
pipes		"FPfine Pk Pipe2"	fp_unit_0.fp_fine_scheduler_0.scheduler_pick_2

# Note that the Exe0 stage of the FPUs is the _last_ stage, not the first.
pipe		"FP0 Reg"	fp_unit_0.register_file_read_0.registerFileRead
pipes		"FP0 Exe"	fp_unit_0.execution_pipeline_0.executionPipeline

pipe		"FP1 Reg"	fp_unit_0.register_file_read_1.registerFileRead
pipes		"FP1 Exe"	fp_unit_0.execution_pipeline_1.executionPipeline

pipe		"FP2 Reg"	fp_unit_0.register_file_read_2.registerFileRead
pipes		"FP2 Exe"	fp_unit_0.execution_pipeline_2.executionPipeline

#-------------------------------------------------- MiniROB
pipemst		"MiniROB"	retire.rob_t0.rob_age_ordered.rob_age_ordered				96	-8	0	1

#-------------------------------------------------- ROB
#pipeis		"ROB"		retire.rob_t0.rob_age_ordered.rob_age_ordered				15	-1	0

#-------------------------------------------------- Retire
pipeis		"Retire"	retire.rob_t0.retire_uops_coll_				3	-1	0

#-------------------------------------------------- Integer Detailed
#pipeis		"Br Sch"	branch_unit_0.generic_scheduler.scheduler_array.scheduler_array		0	7
#pipeis		"ALU_A_0 Sch"	alu_a_0.integer_scheduler.scheduler_array.scheduler_array		0	7
#pipeis		"ALU_A_1 Sch"	alu_a_1.integer_scheduler.scheduler_array.scheduler_array		0	7
#pipeis		"Int 2 Sch"	alu_x_0.integer_scheduler.scheduler_array.scheduler_array		0	7

#-------------------------------------------------- FP Detailed
#pipeis		"FPcrs Sch"	fp_unit_0.fp_coarse_scheduler.scheduler_array.scheduler_array		0	31
#pipeis		"FPfine Sch"	fp_unit_0.fp_fine_scheduler_0.scheduler_array.scheduler_array		0	7
