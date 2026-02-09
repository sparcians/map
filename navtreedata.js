/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "The Sparta Modeling Framework", "index.html", [
    [ "Sparta", "index.html", null ],
    [ "Best Practices, Suggestions on using Sparta", "best_practices.html", [
      [ "Using Sparta Asserts", "best_practices.html#sparta_asserts", null ],
      [ "Using Sparta compile-time hints", "best_practices.html#sparta_expect", null ],
      [ "Take Advantage of C++11/17!", "best_practices.html#cpp_eleven", null ],
      [ "How to Use the Port/Event Mechansim Effectively", "best_practices.html#discrete_event", [
        [ "Phases", "best_practices.html#phases", null ],
        [ "Using The Phases", "best_practices.html#using_phase", null ]
      ] ]
    ] ],
    [ "Building and Integrating the Framework", "building.html", null ],
    [ "Checkpointing", "checkpoint_format.html", null ],
    [ "Command Command Line Interface", "common_cmdline.html", [
      [ "CommandLineSimulator", "common_cmdline.html#cmdline_sim", [
        [ "Logging Configuration", "common_cmdline.html#cmdline_log_cfg", null ],
        [ "Command Line Simulation Configuration", "common_cmdline.html#cmdline_sim_cfg", null ]
      ] ],
      [ "Example Output", "common_cmdline.html#example_cmdline_help_out", null ]
    ] ],
    [ "Communication, Events, and Scheduling", "communication.html", [
      [ "Ports", "communication.html#comm_ports", [
        [ "sparta::SyncPorts", "communication.html#sync_ports", null ]
      ] ],
      [ "Events", "communication.html#comm_events", null ],
      [ "Clocks", "communication.html#comm_clocks", null ],
      [ "Scheduling", "communication.html#comm_schedulers", [
        [ "sparta::Scheduler", "communication.html#sparta_Scheduler", null ],
        [ "sparta::SysCSpartaSchedulerAdapter", "communication.html#sysc_scheduler", null ]
      ] ]
    ] ],
    [ "Core Example Using Sparta", "core_example.html", [
      [ "Example Core Layout", "core_example.html#example_core", null ],
      [ "Building", "core_example.html#Building", null ],
      [ "Invocations", "core_example.html#Invocations", [
        [ "Running/Configuring", "core_example.html#run_config_core_example", null ],
        [ "Debugging/Logging", "core_example.html#debugging_logging_core_example", null ],
        [ "Generating Reports", "core_example.html#reporting_core_example", null ],
        [ "Generating Pipeouts", "core_example.html#pipeouts_core_example", null ]
      ] ]
    ] ],
    [ "Sparta Command Line Interface End-User Guide", "end_user.html", [
      [ "1 Simulator Invocation", "end_user.html#invocation", [
        [ "1.1 I/O Policies", "end_user.html#io_policies", null ],
        [ "1.2 Sparta Basic Command-Line Options", "end_user.html#sparta_cmds", null ],
        [ "1.3 Application-Specific Commands", "end_user.html#app_cmds", null ],
        [ "1.4 Sparta Advanced commands", "end_user.html#sparta_advanced_cmds", null ],
        [ "1.5 Sparta Simulation Debug commands", "end_user.html#sim_dbg_cmds", null ]
      ] ],
      [ "2 Control and Configuration", "end_user.html#ctrl_cfg", [
        [ "2.1 Parameters", "end_user.html#ctrl_cfg_parameters", null ],
        [ "2.2 Selecting Architectures", "end_user.html#ctrl_cfg_architecture", null ],
        [ "2.3 Numeric Constants", "end_user.html#numeric_constants", null ]
      ] ],
      [ "3 Simulator Output", "end_user.html#output", [
        [ "3.1 Automatic Summary", "end_user.html#auto_summary", null ],
        [ "3.2 Report Generation", "end_user.html#report_gen", null ],
        [ "3.3 Message Logging", "end_user.html#msg_logging", null ],
        [ "3.4 Notification Logging", "end_user.html#notification_logging", null ],
        [ "3.5 Performance Events", "end_user.html#perf_events", null ],
        [ "3.6 Pipeline Collection", "end_user.html#pipeline_collection", null ],
        [ "3.7 Post-Run Debug dumps", "end_user.html#debug_dump", null ],
        [ "3.4 Backtraces", "end_user.html#backtraces", null ],
        [ "3.8 Device Tree Inspection", "end_user.html#tree_inspection", null ]
      ] ],
      [ "4 Running with a debugger", "end_user.html#run_with_debugger", [
        [ "GDB", "end_user.html#run_with_dbg_gdb", null ],
        [ "Other Debuggers", "end_user.html#run_with_dbg_other", null ]
      ] ],
      [ "4 Post-processing and Visualization", "end_user.html#data_proc_vis", [
        [ "4.1 Pipeline viewer (Argos)", "end_user.html#argos", null ]
      ] ]
    ] ],
    [ "Sparta FAQ", "sparta_faq.html", [
      [ "Modeling Determinism", "sparta_faq.html#modeling_determinism", null ],
      [ "DES Debugging Techniques", "sparta_faq.html#des_debugging", null ]
    ] ],
    [ "Sparta File Formats", "formats.html", null ],
    [ "Parameter/Configuration Format (.cfg,.yaml)", "param_format.html", [
      [ "Nesting", "param_format.html#param_format_nesting", null ],
      [ "Parameter Assignment Attributes and Optional Parameters", "param_format.html#param_format_attributes", null ],
      [ "Examples", "param_format.html#param_format_examples", null ]
    ] ],
    [ "Report Definition Format (.rrep,.yaml)", "report_def_format.html", [
      [ "Overview", "report_def_format.html#report_def_overview", null ],
      [ "Structure", "report_def_format.html#report_def_structure", [
        [ "Report Fields", "report_def_format.html#report_def_field", null ],
        [ "Example Report Definition", "report_def_format.html#report_def_example", null ],
        [ "Field Declarations", "report_def_format.html#report_def_field_declarations", null ],
        [ "Field Name Variables", "report_def_format.html#report_def_field_name_variables", null ],
        [ "Subreports", "report_def_format.html#report_def_subreports", null ],
        [ "Scope Wildcards", "report_def_format.html#report_def_scope_wildcards", null ],
        [ "Statistical Expressions", "report_def_format.html#report_def_expressions", null ],
        [ "Include Directives", "report_def_format.html#report_def_includes", null ],
        [ "Style section", "report_def_format.html#report_def_style", null ],
        [ "Autopopulation Blocks", "report_def_format.html#report_def_autopop", null ]
      ] ],
      [ "Field Declaration Path/Expression Ambiguity", "report_def_format.html#report_def_var_name_ambiguity", null ],
      [ "Report Definition Directive and Option Reference", "report_def_format.html#report_def_directive_ref", null ],
      [ "Limitations of Report Definitions", "report_def_format.html#report_def_lims", null ]
    ] ],
    [ "Report Output Formats", "report_out_format.html", [
      [ "Plaintext (.txt, .text)", "report_out_format.html#report_out_format_plaintext", null ],
      [ "CSV (.csv)", "report_out_format.html#report_out_format_csv", null ],
      [ "BasicHTML (.html, .htm)", "report_out_format.html#report_out_format_basichtml", null ],
      [ "Gnuplot (.gnuplot, .gplt)", "report_out_format.html#report_out_format_gnuplot", null ],
      [ "PythonDict (.python, .python)", "report_out_format.html#report_out_format_pythondict", null ],
      [ "JavascriptObject (.json)", "report_out_format.html#report_out_format_json", null ]
    ] ],
    [ "Logging Output formats (.log, .log.raw, .log.basic, log.verbose)", "log_out_format.html", null ],
    [ "Framework Development", "framework_dev.html", [
      [ "Coding Style", "framework_dev.html#coding_style", null ],
      [ "Regression Testing", "framework_dev.html#regression", null ],
      [ "Doxygen Convention", "framework_dev.html#doxygen_convention", [
        [ "Code Documentation Convention", "framework_dev.html#code_conv", null ],
        [ "Textual Documentation Convention", "framework_dev.html#text_conv", null ]
      ] ],
      [ "Meta Documentation", "framework_dev.html#meta_doc", null ]
    ] ],
    [ "Textual Message Logging", "logging.html", [
      [ "Logging System Goals", "logging.html#logging_goals", null ],
      [ "Logging System Design Requirements", "logging.html#log_des_reqs", null ],
      [ "Conceptual Usage", "logging.html#logg_concept_usage", [
        [ "Scoped Logging", "logging.html#scoped_log", null ],
        [ "Global Logging", "logging.html#global_log", null ]
      ] ],
      [ "Capturing Log Messages", "logging.html#log_capturing", null ],
      [ "Usage Notes", "logging.html#log_usage_notes", null ],
      [ "Implementation Notes", "logging.html#log_impl_notes", null ]
    ] ],
    [ "Sparta API", "modeling.html", null ],
    [ "Simulator Configuration", "config.html", [
      [ "Configuration Goals", "config.html#config_goals", null ],
      [ "Simulator Subclass Configuration", "config.html#config_usage", null ],
      [ "Configuration System Design Requirements", "config.html#config_des_req", null ]
    ] ],
    [ "Skeleton Pipeline Using Sparta", "skeleton_example.html", [
      [ "Skeleton Pipeline Layout", "skeleton_example.html#skeleton_layout", [
        [ "Getting Help and Simulation Layout", "skeleton_example.html#autotoc_md26", null ],
        [ "Running the simulator", "skeleton_example.html#autotoc_md27", null ],
        [ "Running the simulator and generating reports", "skeleton_example.html#autotoc_md28", null ],
        [ "Running the simulator and generating logs", "skeleton_example.html#autotoc_md29", null ],
        [ "Running the simulator and configuring it", "skeleton_example.html#autotoc_md30", null ],
        [ "Looking at the Code", "skeleton_example.html#skeleton_looking", null ]
      ] ]
    ] ],
    [ "SystemC Models", "systemc.html", [
      [ "Building", "systemc.html#building_sysc", null ],
      [ "SystemC/Sparta Adapter", "systemc.html#sysc_adapter", null ],
      [ "SystemC/Sparta Examples", "systemc.html#sysc_examples", null ]
    ] ],
    [ "Precedence operators for EventNode/Scheduleables", "precedence_rules.html", null ],
    [ "RegisterProxy Usage", "RegisterProxy_Usage.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"1__phase_2sparta-target_2SpartaTLMTargetGasket_8cpp_source.html",
"MemoryProfiler_8hpp.html",
"SpartaHandler_8hpp.html#a58d70e3106cf85934ee106347a7ee8a8",
"classcore__example_1_1Dispatch.html#ac34d44524012fb5df1018f79b3a5a022",
"classsparta_1_1Array.html#a2b458c2ca7766f2f6414241bcf4505fe",
"classsparta_1_1CircularBuffer.html#a087ee0b57c0b3402e6224d1145be5c8e",
"classsparta_1_1CycleCounter.html#af405cdbb75549418ef1c7baf5981db08",
"classsparta_1_1DataView.html#ab33d5ee7fbf8e269c353487e5f2c60e8",
"classsparta_1_1HistogramBase.html#a306d93ffc77dcfc06ff11f09da36ff06",
"classsparta_1_1NotificationSourceBase.html#a2590bdc3cdd8103ce2d921cd468012a6",
"classsparta_1_1ParameterBase.html#a11b2f68d5b3c7edb192dc5a1dc57f47e",
"classsparta_1_1ParameterTree.html#a3be389fb5fdcad1e20bb9e484a031444",
"classsparta_1_1PhasedPayloadEvent.html#aff862c00151e82e1bd89e795e02a81a9",
"classsparta_1_1Pipeline.html#ae08f5f438f2e89a12bc75a23a3af5b74",
"classsparta_1_1ReadOnlyCounter.html#af8accb0eb92764823f07f61f75aceadf",
"classsparta_1_1RegisterProxy.html#a1109043e30032d2aaea07c8494035892",
"classsparta_1_1ResourceTreeNode.html#acea50c9888b0e5a9b1df54c68cb94b10",
"classsparta_1_1Scheduler.html#a6d6cc1c9c8922a360b83a28d26d675c6",
"classsparta_1_1SpartaFatalError.html#a538debbd71ef61fb6656c04fffc5e2cd",
"classsparta_1_1StatisticDef.html#ab0fc3ad25963cec6009a291b0b2074a3",
"classsparta_1_1SyncOutPort.html#aa8fc86d1e1801c085fc847b06972f477",
"classsparta_1_1TreeNode.html#a6e0acc7bdbe1cde5dd450e2a5677316f",
"classsparta_1_1TreeNode.html#affdb43d52e9eb2d550b2ab40c4b2722c",
"classsparta_1_1app_1_1CommandLineSimulator.html#a9f9c6c875da9b7dbb423f2090f5ab0e3",
"classsparta_1_1app_1_1ReportDescriptor.html#aa013eca6b03cc8bbf8723bef62f4cae7",
"classsparta_1_1app_1_1Simulation.html#ae5a2810562889b8eb9861d746dd7e982",
"classsparta_1_1bounded__int.html#a8d3668dac3ef02b6375d525394d421e7",
"classsparta_1_1log_1_1Tap.html#a7b29559f1f2c6b8b0d108a591934fa71",
"classsparta_1_1memory_1_1SimpleMemoryMap.html#a8018c73a24f34bc73000ef89edabe529",
"classsparta_1_1serialization_1_1checkpoint_1_1FastCheckpointer.html#a9ea4d4529c8d5545869d16293f6db1ca",
"classsparta_1_1statistics_1_1expression_1_1Expression.html#aa4551bf218698b35150abc6742c69f39",
"classsparta_1_1utils_1_1LockedValue.html",
"functions_vars_d.html",
"structMetaStruct_1_1is__any__pointer_3_01std_1_1unique__ptr_3_01T_01_4_01const_01_4.html",
"structsparta_1_1StatisticDef_1_1ExpressionArg.html",
"structsparta_1_1statistics_1_1expression_1_1StatVariable.html#a0d95651a8745d62a1cf496d16e52e03c"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';