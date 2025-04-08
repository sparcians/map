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
        [ "Getting Help and Simulation Layout", "skeleton_example.html#autotoc_md25", null ],
        [ "Running the simulator", "skeleton_example.html#autotoc_md26", null ],
        [ "Running the simulator and generating reports", "skeleton_example.html#autotoc_md27", null ],
        [ "Running the simulator and generating logs", "skeleton_example.html#autotoc_md28", null ],
        [ "Running the simulator and configuring it", "skeleton_example.html#autotoc_md29", null ],
        [ "Looking at the Code", "skeleton_example.html#skeleton_looking", null ]
      ] ]
    ] ],
    [ "SystemC Models", "systemc.html", [
      [ "Building", "systemc.html#building_sysc", null ],
      [ "SystemC/Sparta Adapter", "systemc.html#sysc_adapter", null ]
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
"AddressTypes_8hpp_source.html",
"MetaStructs_8hpp.html",
"SpartaSharedPointerBaseAllocator_8hpp_source.html",
"classcore__example_1_1Fetch_1_1FetchParameterSet.html",
"classsparta_1_1Array.html#a4e0dceb28fe2dd2cd96e51869f5bbee3",
"classsparta_1_1CircularBuffer.html#a3d4e9481961a1911966e38bfe8c1c7f8",
"classsparta_1_1CycleHistogramBase.html#a0732a7b45751a34d98bce32894acceaf",
"classsparta_1_1DataView.html#adf40338ed77b64d6f1ed79262a997eef",
"classsparta_1_1HistogramBase.html#a5258946c4817315712f08f13e6ad2455",
"classsparta_1_1NotificationSourceBase.html#a6ad355b5a3865f40b6f2a62442a5240e",
"classsparta_1_1ParameterBase.html#a1aef03b182d09beae70776f9326aebd3",
"classsparta_1_1ParameterTree.html#a4d8c6b1cef74b832eda5277d57d1fbbf",
"classsparta_1_1PhasedUniqueEvent.html#a9b1f9bc7e261a217392ea43e7714fdcf",
"classsparta_1_1Port.html#a1bb9f0cdf1eef4f206a76acf63fb9271",
"classsparta_1_1RegisterBankTable.html#ad71c7433e43844487fbf078afa3a3504",
"classsparta_1_1RegisterSet.html#a450b47a0bc9c80151c514c60848818cf",
"classsparta_1_1RootTreeNode.html#a5000407bd2674c968234ebe45b2e9d4e",
"classsparta_1_1Scoreboard.html",
"classsparta_1_1SpartaSharedPointerAllocator.html#a85e059a11a23bcd39d29b27eb369a260",
"classsparta_1_1StatisticInstance.html#a43c8447d72f66f3b334fa87453e2c563",
"classsparta_1_1Tag.html#af1968afa394fde66b67f29eae506e37c",
"classsparta_1_1TreeNode.html#a8830a6be5d9f6e60e4bff8124d19b289",
"classsparta_1_1Unit.html#a90b5f7769c5be6001b0ba5458df01a51",
"classsparta_1_1app_1_1DefaultValues.html#a0939189ea426e43f794b4749939131d1",
"classsparta_1_1app_1_1Simulation.html#a10b48925db5f3c65de0f7536ddc9057c",
"classsparta_1_1app_1_1SimulationConfiguration.html#a32d4ce78029af6a0f1bceaaa959acdc7",
"classsparta_1_1collection_1_1Collector.html#a4f07eaf1d769f336176f0e86c5205ba0",
"classsparta_1_1memory_1_1BlockingMemoryIFNode.html#a2b73c1294a01d27a0170d61e288e7a23",
"classsparta_1_1memory_1_1Translation.html#a5fae274cd13b38f548c80dbe40a958f4",
"classsparta_1_1statistics_1_1BinaryIArchive.html",
"classsparta_1_1tracker_1_1StatePool.html#a8c62e2ad5838c1b33878733d7ea01ab9",
"classsparta_1_1utils_1_1ValidValue.html#a5029387705c6fbfd61cbb40b4c7dc102",
"namespacesparta.html#a4f295b2cf0f4cec80b27714ab48be2b7",
"structMetaStruct_1_1return__type_3_01R_01_6_07T_1_1_5const_01_6_08_07_08_01const_01_4.html",
"structsparta_1_1memory_1_1BlockingMemoryIFNode_1_1PostWriteAccess.html#a0a4322352d4dc45932b62e04e9a16eae",
"structsparta_1_1tracker_1_1has__ostream__operator__impl_1_1has__ostream__operator.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';