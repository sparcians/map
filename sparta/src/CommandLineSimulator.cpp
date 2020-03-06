// <CommandLineSimulator> -*- C++ -*-


/*!
 * \file CommandLineSimulator.cpp
 * \brief Class for creating a simulator based on command-line arguments
 */

#include "sparta/app/CommandLineSimulator.hpp"

#include <boost/system/error_code.hpp>
#include <ctype.h>
#include <limits.h>
#include <cstddef>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/bad_lexical_cast.hpp>
#include <boost/program_options/detail/parsers.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <boost/filesystem/path.hpp>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <exception>
#include <iterator>
#include <ratio>
#include <tuple>
#include <utility>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/report/format/BaseFormatter.hpp"
#include "sparta/report/db/ReportVerifier.hpp"
#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/report/DatabaseInterface.hpp"
// // For filtered printouts
#include "sparta/statistics/Counter.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/utils/File.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "simdb/ObjectManager.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/Colors.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/kernel/SleeperThreadBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/utils/TimeManager.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/app/AppTriggers.hpp"
#include "sparta/app/MetaTreeNode.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/argos/InformationWriter.hpp"
#include "sparta/log/Destination.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"

namespace bfs = boost::filesystem;

namespace sparta {
    namespace app {

DefaultValues CommandLineSimulator::DEFAULTS;

const uint32_t OPTIONS_DOC_WIDTH = 140;

const char INVALID_HELP_TOPIC[] = "<invalid help topic>";
const char MULTI_INSTRUCTION_TRIGGER_ERROR_MSG[] = \
    "Cannot use more than one of --debug-on, --debug-on-icount, and instruction based pevent "
    "triggering at the same time. This is not yet supported/tested";

//! Prints logging help text
void showLoggingHelp()
{
    std::cout << "Logging:\n\n  The \"--log\" DEST parameter can be \"" << utils::COUT_FILENAME
              << "\" to refer to stdout, \"" << utils::CERR_FILENAME
              << "\" to refer to stderr, or a filename which can contain any extension shown\n"
                 "below for a particular type of formatting:\n"
              << std::endl;
    sparta::log::DestinationManager::dumpFileExtensions(std::cout, true);
}

void showConfigHelp()
{
    std::cout << "Config:\n\n  "
                 "Note that parameters and configuration files specified by the -c (global config\n"
                 "file), -n (node config file), and -p (parameter value) options are applied in the\n"
                 "left-to-right order on the command line, overwriting any previous values.\n\n"
              << std::endl;
}

//! Prints reports help text
void showReportsHelp()
{
    std::cout << "Reports:\n\n"
                 "  The \"--report\" PATTERN parameter can refer to any number of "
                 "nodes in the device tree. For each node referenced, a new Report will be created "
                 "and appended to the file specified by DEST for that report. If these reports "
                 "should be written to different files, variables can be used in the destination "
                 "filename to differentiate:\n"
                 "    %l => Location in device tree of report instantiation\n"
                 "    %i => Index of report instantiation\n"
                 "    %p => Host process ID\n"
                 "    %t => Timestamp\n"
                 "    %s => Simulator name\n\n"
                 "  Additionaly, the DEST parameter can be a filename or \""
              << utils::COUT_FILENAME << "\", referring to stdout, or \""
              << utils::CERR_FILENAME << "\", referring to stderr\n"
                 "  If outputting to stdout/stderr. the optional report FORMAT parameter should be "
                 "omitted or \"txt\" ."
                 "\n\n"
                 "  Valid formats include:\n";
    sparta::report::format::BaseFormatter::dumpFormats(std::cout);
    std::cout << std::endl;
}

CommandLineSimulator::CommandLineSimulator(const std::string& usage,
                                           const DefaultValues& defs) :
    sim_config_(defs),
    usage_(usage),
    runtime_clock_(sim_config_.getDefaults().run_time_clock),
    auto_summary_(defs.auto_summary_default),
    help_topic_(INVALID_HELP_TOPIC),
    sparta_opts_("General Options", OPTIONS_DOC_WIDTH),
    param_opts_("Parameter Options", OPTIONS_DOC_WIDTH),
    debug_opts_("Debug Options", OPTIONS_DOC_WIDTH),
    run_time_opts_("Run-time Options", OPTIONS_DOC_WIDTH),
    pipeout_opts_("Pipeline-Collection Options", OPTIONS_DOC_WIDTH),
    log_opts_("Logging Options", OPTIONS_DOC_WIDTH),
    report_opts_("Report Options", OPTIONS_DOC_WIDTH),
    simdb_opts_("SimDB Options", OPTIONS_DOC_WIDTH),
    simdb_internal_opts_("SimDB Options (internal / developer use)", OPTIONS_DOC_WIDTH),
    app_opts_("Application-Specific Options", OPTIONS_DOC_WIDTH),
    feature_opts_("Feature Evaluation Options", OPTIONS_DOC_WIDTH),
    advanced_opts_("Advanced Options", OPTIONS_DOC_WIDTH)
{
    static std::stringstream heartbeat_doc;
    heartbeat_doc << \
        "The interval in ticks at which index pointers will be written to file during pipeline "
        "collection. The heartbeat also represents the longest life duration of lingering "
        "transactions. Transactions with a life span longer than the heartbeat will be finalized "
        "and then restarted with a new start time. Must be a multiple of 100 for efficient reading "
        "by Argos. Large values will reduce responsiveness of Argos when jumping to different "
        "areas of the file and loading.\nDefault = "
        << DefaultHeartbeat << " ticks.\n";

    sparta_opts_.add_options()
        ("help,h",
         "Show complete help message on stdout then exit", // Verbose Description
         "Show this help message")  // Brief Description
        ("help-brief",
         "Show brief help on stdout then exit",
         "Brief help for common commands")
        ("verbose-help",
         "Deprecated. Use --help")
        ("help-topic",
         named_value<std::string>("TOPIC", &help_topic_),
         "Show help information on a particular topic then exit. Use \"topics\" as TOPIC to show "
         "all topic options",
         "Show topic information. Use \"topics\" to start") // Brief

        ("no-run",
         "Quit with exit code 0 prior to finalizing the simulation. When running without this (or "
         "without other option having the same effect such as --show-parameters), the simulator "
         "will still attempt to run and may exit with an error if the default configuration does "
         "not run successfully as-is",
         "Quit with exit code 0 prior to finalizing the simulation") // Brief

        // Show sparta tree states
        ("show-tree",
         "Show the device tree during all stages of construction excluding hidden nodes. This also "
         "enables printing of the tree when an exception is printed")
        ("show-parameters",
         "Show all device tree Parameters after configuration excluding hidden nodes. Shown in a "
         "separate tree printout from all other --show-* parameters.\n"
         "See related: --write-final-config")
        ("show-ports",
         "Show all device tree Ports after finalization. Shown in a "
         "separate tree printout from all other --show-* parameters")
        ("show-counters",
         "Show the device tree Counters, Statistics, and other instrumentation after finalization. "
         "Shown in a separate tree printout from all other --show-* parameters")
        ("show-stats",
         "Same as --show-counters")
        ("show-notifications",
         "Show the device tree notifications after finalization excluding hidden nodes and Logger "
         "MessageSource nodes. Shown in a separate tree printout from all other --show-* parameters")
        ("show-loggers",
         "Show the device tree logger MessageSource nodes after finalization.  Shown in a "
         "separate tree printout from all other --show-* parameters")
        ("show-dag", "Show the dag tree just prior to running simulation")
        ("show-clocks", "Show the clock tree after finalization. Shown in a seperate tree printout"
         "from all other --show-* parameters")

        ("help-tree",
         "Sets --no-run and shows the device tree during all stages of construction excluding "
         "hidden nodes. This also enables printing of the tree when an exception is printed")
        ("help-parameters",
         "Sets --no-run and shows all device tree Parameters after configuration excluding hidden "
         "nodes. Shown in a separate tree printout from all other --show-* parameters.\n"
         "See related: --write-final-config")
        ("help-ports",
         "Sets --no-run and shows all device tree Ports after finalization. Shown in a "
         "separate tree printout from all other --show-* parameters")
        ("help-counters",
         "Sets --no-run and shows the device tree Counters, Statistics, and other instrumentation "
         "after finalization. Shown in a separate tree printout from all other --show-* parameters")
        ("help-stats",
         "Same as --help-counters")
        ("help-notifications",
         "Sets --no-run and shows the device tree notifications after finalization excluding "
         "hidden nodes and Logger MessageSource nodes. Shown in a separate tree printout from all "
         "other --show-* parameters")
        ("help-loggers",
         "Sets --no-run and shows the device tree logger MessageSource nodes after finalization. "
         "Shown in a separate tree printout from all other --show-* parameters")
        ("help-clocks",
         "Sets --no-run and shows the device tree clock nodes after finalization. "
         "Shown in a separate tree printout from all other --show-* parameters")
        ("help-pevents",
         "Sets --no-run and shows the pevents types in the model after finalization. "
         )


        // Validation & Debug
        ("validate-post-run",
         "Enable post-run validation. After run completes without throwing an exception, the "
         "entire tree is walked and posteach resource is allowed to perform post-run-validation if "
         "it chooses. Any resource with invalid state have the opportunity to throw an exception "
         "which will cause the simulator to exit with an error. Note that this validation may not "
         "aways be appropriate because the simulation can be be ended abruptly with an "
         "instruction-count or cycle-count limit",
         "Enable post-run validation after run completes without exception") // Brief
        ("disable-infinite-loop-protection",
         "Disable detection of infinite loops during simulation.") // Brief
        ("debug-dump",
         named_value<std::string>("POLICY", &dump_debug_type_),
         "Control debug dumping to a file of the simulator's choosing. Valid values "
         "include 'error': (default) dump when exiting with an exception. 'never': never dump, "
         "'always': Always dump on success, failure, or error.\n"
         "Note that this dump will not be triggered on command-line errors such as invalid options "
         "or unparseable command-lines. Bad simulation-tree parameters (-p) will trigger this "
         "error dump.",
         "Control post-run debug dumping to a file of the simulator's choosing. Values: "
         "{error,never,always}") // Brief
        ("debug-dump-options",
         named_value<std::string>("OPTIONS", &debug_dump_options_),
         "When debug dumping is enabled, use this option to narrow down what specifically should "
         "be captured in the error log. Valid values include 'all', 'asserts_only', and 'backtrace_only'",
         "Options to only dump subsets of error logs to file")
        ("debug-dump-filename",
         named_value<std::string>("FILENAME", &sim_config_.dump_debug_filename),
         "Sets the filename used when creating a debug dump after running or durring an run/setup "
         "error. Defaults to \"\" which causes the simulator to create a name in the form "
         "\"error-TIMESTAMP.dbg\"",
         "Sets the filename used when creating a debug dump after running") // Brief

        // PEvents.
        ("pevents",
         named_value<std::vector<std::string> >("FILENAME CATEGORY", 2, 2)->multitoken(),
         "Log pevents in category CATEGORY that are passed to the PEventLogger during simulation "
         "to FILENAME.\n"
         "when CATEGORY == ALL, all pevent types will be logged to FILENAME\n"
         "Examples: \n--pevents output.pevents ALL\n"
         "--pevents log.log complete,retire,decode")
        ("verbose-pevents",
         named_value<std::vector<std::string> >("FILENAME CATEGORY", 2, 2)->multitoken(),
         "Log more verbose pevents in category CATEGORY that are passed to the PEventLogger during "
         "simulation to FILENAME.\n"
         "when CATEGORY == ALL, all pevent types will be logged to FILENAME\n"
         "Examples: \n--pevents output.pevents ALL\n"
         "--pevents log.log RETIRE,decode")
        ("pevents-at",
         named_value<std::vector<std::string>>("FILENAME TREENODE CATEGORY", 3, 3)->multitoken(),
         "Log pevents of type CATEGORY at and below TREENODE.\nWhen CATEGORY == ALL then all pevent types will be logged below and at TREENODE."
         "Example: \"--pevents-at lsu_events.log top.core0.lsu ALL\" "
         "This option can be specified none or many times.") // Brief
        ("verbose-pevents-at",
         named_value<std::vector<std::string>>("FILENAME TREENODE CATEGORY", 3, 3)->multitoken(),
         "Log verbose pevents of type CATEGORY at and below TREENODE.\nWhen CATEGORY == ALL then all pevent types will be logged below and at TREENODE."
         "Example: \"--verbose-pevents-at lsu_events.log top.core0.lsu ALL\" "
         "This option can be specified none or many times.") // Brief
        ;
    run_time_opts_.add_options()
        // Run Control
        ("run-length,r",
         named_value<std::vector<std::vector<std::string>>>("[CLOCK] CYCLE", 1, 2)->multitoken(),
         "Run the simulator for the given cycles based on the optional clock\n"
         "Examples:\n'-r core_clk 500'\n"
         "'-r 500,'\n"
         "If no clock is specified, this value is interpreted in a a simulator-specific way."
         "Run a length of simulation in cycles on a particular clock. With no clock "
         "specified, this is interpted in a simulator-specific way") // Brief
        ("wall-timeout",
         named_value<std::vector<std::vector<std::string>>>("HOURS EXIT_TYPE", 1, 2)->multitoken(),
         "Run the simulator until HOURS wall clock time has passed.\n"
         "Examples:\n'--wall-timeout 5 clean'\n"
         "'--wall-timeout 5 error'\n"
         "The only exit types are \"clean\" and \"error\". error throws an exception, clean will stop simulation nicely.") // Brief
        ("cpu-timeout",
         named_value<std::vector<std::vector<std::string>>>("HOURS EXIT_TYPE", 1, 2)->multitoken(),
         "Run the simulator until HOURS cpu user clock time has passed.\n"
         "Examples:\n'--cpu-timeout 5 clean'\n"
         "'--cpu-timeout 5 error'\n"
         "The only exit types are \"clean\" and \"error\". error throws an exception, clean will stop simulation nicely.") // Brief

        ;

    debug_opts_.add_options()
        // Infrastructure Debugging
        ("debug-on",
         named_value<std::vector<std::vector<std::string>>>("[CLOCK] CYCLE", 1, 2)->multitoken(),
         "\nDelay the recording of useful information starting until a specified simulator cycle "
         "at the given clock. If no clock provided, a default is chosen, typically the fastest. "
         "This includes any user-configured pipeline collection or logging (builtin logging of "
         "warnings to stderr is always enabled). Note that this is just a "
         "delay; logging and pipeline collection must be explicitly enabled.\n"
         "WARNING: Must not be specified with --debug-on-icount\n"
         "WARNING: The CYCLE may only be partly included. It is dependent upon when the "
         "scheduler activates the trigger. It is recommended to schedule a few ticks before your "
         "desired area.\n"
         "Examples: '--debug-on 5002 -z PREFIX_ --log top debug 1' or '--debug-on core_clk 5002 "
         "-z PREFIX_'\n"
         "begins pipeline collection to PREFIX_ and logging to stdout at some point within tick "
         "5002 and will include all of tick 5003",
         "Begin all debugging instrumentation at a specific tick number") // Brief
        ("debug-on-icount",
         named_value<std::vector<std::vector<std::string>>>("INSTRUCTIONS"),
         "\nDelay the recording of useful information starting until a specified number of "
         "instructions.\n"
         "WARNING: Must not be specified with --debug-on\n"
         "See also --debug-on.\n"
         "Examples: '--debug-on-icount 500 -z PREFIX_'\n"
         "Begins pipeline collection to PREFIX_ when instruction count from this simulator's "
         "counter with the CSEM_INSTRUCTIONS semantic is equal to 500",
         "Begin all debugging instrumentation at a specific instruction count") // Brief
        ;

    // Pipeline configuration
    pipeout_opts_.add_options()
        ("pipeline-collection,z",
         named_value<std::vector<std::string>>("OUTPUTPATH", 1, 1)->multitoken(),
         "Run pipeline collection on this simulation, and dump the output files to OUTPUTPATH. "
         "OUTPUTPATH can be a prefix such as myfiles_ for the pipeline files and may be a "
         "directory\n"
         "Example: \"--pipeline-collection data/test1_\"\n"
         "Note: Any directories in this path must already exist.\n",
         "Enable pipline collection to files with names prefixed with OUTPATH") // Brief
        ("collection-at,k",
         named_value<std::vector<std::string>>("TREENODE", 1, 1),
         "Specify a treenode to recursively turn on at and below for pipeline collection."
         "Example: \"--collection-at top.core0.rename\" "
         "This option can be specified none or many times.") // Brief
        ("argos-collection-at,K",
         named_value<std::vector<std::string>>("ALFFILE", 1, 1),
         "Specify an Argos ALFFILE file to restrict pipeline collection to only those nodes found in the ALF."
         "Example: \"--argos-collection-at layouts/exe40.alf\" "
         "This option can be specified none or many times.") // Brief
        ("heartbeat",
         named_value<std::string>("HEARTBEAT", &pipeline_heartbeat_)->default_value(pipeline_heartbeat_),
         heartbeat_doc.str().c_str())
        ;

    std::stringstream arch_search_dirs_str;
    arch_search_dirs_str << sim_config_.getDefaults().arch_search_dirs;
    std::string ARCH_HELP =
        "Applies a configuration at the global namespace of the simulator device tree in a similar "
         "way as --config-file/-c. This configuration is effectively a set of new defaults for any "
         "included parameters. "
         "Example: \n\"--arch project_x\"\nValid arguments can be found in the --arch-search-dir "
         "directory which defaults to \"";
         ARCH_HELP += arch_search_dirs_str.str();
         ARCH_HELP += "\"";

    std::string ARCH_SEARCH_DIRS_HELP =
         "Base directory in which to search for the architecture configuration baseline chosen by "
         "--arch (default: \"";
         ARCH_SEARCH_DIRS_HELP += arch_search_dirs_str.str();
         ARCH_SEARCH_DIRS_HELP +=  "\")\nExample: \"--arch-search-dir /archive/20130201/architecures/\"\n";

    std::string CONFIG_SEARCH_DIRS_HELP =
         "Additional search directories in which to search for includes found in configuration files given by "
         "--config-file/-c <file.yaml> (default is : \"./\")\nExample: \"--config-search-dir /archive/20130201/configurations/\"\n";

    std::string REPORT_DEFN_SEARCH_DIRS_HELP =
        "Additional search directories in which to search for report definition files referenced inside a multi-report YAML file (SPARTA v1.6+) given by "
        "--report <file.yaml> (default is: \"./\")\nExample: \"--report-search-dir /full/path/to/definition/files/\"\n";

    // Configuration
    param_opts_.add_options()
        ("parameter,p",
         named_value<std::vector<std::vector<std::string>>>("PATTERN VAL", 2, 2)->multitoken(),
         "Specify an individual parameter value. Multiple parameters can be identified using '*' "
         "and '?' glob-like wildcards. \n"
         "Example: --parameter top.core0.params.foo value",
         "Set a specific parameter value") // Brief
        ("optional-parameter",
         named_value<std::vector<std::vector<std::string>>>("PATTERN VAL", 2, 2)->multitoken(),
         "Specify an optional individual parameter value. Unlike --parameter/-p, this will not fail "
         "if no parameter(s) matching PATTERN can be found. However, if matching nodes are found, "
         "the value given must be compatible with those parameter nodes. Otherwise, behavior is "
         "idenitical to --parameter/-p",
         "Set a specific parameter value if parameters can be found with the given location pattern") // Brief
        ("config-file,c",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Specify a YAML config file to load at the global namespace of the simulator device tree. "
         "Example: \"--config-file config.yaml\" "
         "This is effectively the same as --node-config-file top params.yaml",
         "Apply a YAML configuration file at a node in the simulator") // Brief
        ("read-final-config",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Read a previously generated final configuration file. When this is used parameters in the "
         "model are set purely off the values specified in FILENAME. The simulator can not override "
         "the values nor can -p or other configuration files be specified. In other words, simulation "
         "is guaranteed to run with the same values as the parameters specified in this file")
        ("node-config-file,n",
         named_value<std::vector<std::vector<std::string>>>("PATTERN FILENAME", 2, 2)->multitoken(),
         "Specify a YAML config file to load at a specific node (or nodes using '*' and '?' "
         "glob-like wildcards) in the device tree.\n"
         "Example: \"--node-config-file top.core0 core0_params.yaml\"")
        ("extension-file,e",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Specify a YAML extension file to load at the global namespace of the simulator device tree. "
         "Example: \"--extension-file extensions.yaml\"",
         "Apply a YAML extension file at the top node in the simulator") // Brief
        ("control",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Specify a YAML control file that contains trigger expressions for simulation pause, resume, "
         "terminate, and custom named events. "
         "Example: \"--control ctrl_expressions.yaml\"",
         "Apply simulation control trigger expressions to the simulator")
        ("arch",
         named_value<std::vector<std::string>>("ARCH_NAME", 1, 1),
         ARCH_HELP.c_str(),
         "Applies a configuration as parameter defaults") // Brief
        ("arch-search-dir",
         named_value<std::vector<std::string>>("DIR", 1, 1),
         ARCH_SEARCH_DIRS_HELP.c_str())
        ("config-search-dir",
         named_value<std::vector<std::string>>("DIR", 1, 1),
         CONFIG_SEARCH_DIRS_HELP.c_str())
        ("report-search-dir",
         named_value<std::vector<std::string>>("DIR", 1, 1),
         REPORT_DEFN_SEARCH_DIRS_HELP.c_str())
        ("write-final-config",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Write the final configuration of the device tree to the specified file before running "
         "the simulation",
         "Write parameter configuration to file")
        ("write-power-config",
         named_value<std::string>("FILENAME", &power_config_file_)->default_value(power_config_file_),
         "Write the configuration of the device tree to the specified file to be consumed by TESLA"
         "for modeling power",
         "Write power related parameter configuration to file")
        ("write-final-config-verbose",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Write the final configuration of the device tree to the specified file before running "
         "the simulation. The output will include parameter descriptions and extra whitespace for "
         "readability",
         "Write parameter configuration to file with long descriptions")
        ("enable-state-tracking",
         named_value<std::vector<std::string>>("FILENAME", 1, 1),
         "Specify a Text file to save State Residency Tracking Histograms. "
         "Example: \"--enable-state-tracking data/histograms.txt\"",
         "Note: Any directories in this path must already exist.\n",
         "Enable state residency tracking and write to file with name FILENAME.")
        ;

    // Logging
    log_opts_.add_options()
        ("log,l",
         named_value<std::vector<std::vector<std::string>>>("PATTERN CATEGORY DEST", 3, 3)->multitoken(),
         "Specify a node in the simulator device tree at the node described by PATTERN (or nodes "
         "using '*' and '?' glob wildcards) on which to place place a log-message tap (observer) "
         "that watches for messages having the category CATEGORY. Matching messages from those "
         "node's subtree are written to the filename in DEST. DEST may also be '1' to refer to "
         "stdout and '2' to refer to cerr. Any number of taps can be added anywhere in the device "
         "tree. An error is generated if PATTERN does not refer to a 1 or more nodes. Use "
         "--help for more details\n"
         "Example: \"--log top.core0 warning core0_warnings.log\"",
         "Example: \"--log top.core0 '*' core0_all.log\"",
         "Attaches logging tap(s) at nodes matching a location pattern. Directs output matching "
         "category to destination") // Brief
        ("warn-file",
         named_value<std::string>("FILENAME", &sim_config_.warnings_file),
         "Filename to which warnings from the simulator will be logged. This file will be "
         "overwritten. This has no relationship with --no-warn-stderr")
        ("no-warn-stderr",
         "Do not write warnings from the simulator to stderr. Unset by default. This is has no "
         "relationship with --warn-file")
        ;

    // Reports
    report_opts_.add_options()
        ("report",
         named_value<std::vector<std::vector<std::string>>>("DEF_FILE | PATTERN DEF_FILE DEST [FORMAT]", 1, 4)->multitoken(),
         "Specify a single definition file containing descriptions for more than one report. "
         "See the 'ReportTriggers.txt' file in this directory for formatting information.\n"
         "Example: \"--report all_report_descriptions.yaml\"\n"
         "Note that the option '--report DEF_FILE' is the only way to use report triggers of any "
         "kind, such as warmup.\n"
         "You can also provide YAML keyword replacements on a per-report-yaml basis.\n"
         "Example: \"--report foo_descriptor.yaml foo.yaml --report bar_descriptor.yaml bar.yaml\"\n"
         "In this usage, foo.yaml contains %KEYWORDS% that replace those found in foo_descriptor.yaml,\n"
         "while bar(_descriptor).yaml does the same without clashing with foo(_descriptor.yaml)\n"
         "See foo*.yaml and bar*.yaml in <sparta>/example/CoreModel for more details.\n"
         "You may also specify individual report descriptions one at a time with the options\n"
         "'PATTERN DEF_FILE DEST [FORMAT]' as follows:\n"
         "Specify a node in the simulator device tree at the node described by PATTERN (or nodes "
         "using '*' and '?' glob wildcards) at which generate a statistical report "
         "that examines the set of statistics based on the Report definition file DEF_FILE. At the "
         "end of simulation, the content of this report (or reports, if PATTERN refers to multiple "
         "nodes) is written to the file specified by DEST. "
         "DEST may also be  to refer to stdout and 2 to refer to stderr. Any number of reports can "
         "be added anywhere in the device tree.An error is generated if PATTERN "
         "does not refer to 1 or more nodes. FORMAT can be used to specify the format. "
         "See the report options section with --help for more details about formats.\n"
         "Example: \"--report top.core0 core_stats.yaml core_stats txt\"\n"
         "Example: \"--report top.core* core_stats.yaml core_stats.%l\"\n"
         "Example: \"--report _global global_stats.yaml global_stats\"",
         "Example: \"--report top.core0 @ all_core_stats\""
         "The final example uses an '@' in place of a yaml file to "
         "designate that the framework should auto-populate a hierarchical report based on all the "
         "statistics and counters at or below the locations described by PATTERN. This is like "
         "using --report-all at a specific node. _global is a keyword referring to the global "
         "search scope which contains all simulation and supporting trees including the SPARTA "
         "scheduler(s)", // Verbose
         "Attaches report(s) defined by a yaml file at nodes matching a location pattern and "
         "writes output to destination.") // Brief
        ("report-all",
         named_value<std::vector<std::vector<std::string>>>("DEST [FORMAT]", 1, 2)->multitoken(),
         "Generates a single report on the global simulation tree containing all counters and "
         "statistics below it. "
         "This report is written to the file specified by DEST using the format specified by "
         "FORMAT (if supplied). Otherwise, the format is inferred from DEST. "
         "DEST may be a filename or 1 to refer to stdout and 2 to refer to stderr. "
         "See the report options setcion with --help for more details."
         "This option can be used multiple times and does not interfere with --report.\n"
         "Example: \"--report-all core_stats.txt\"\n"
         "Example: \"--report-all output_file html\"\n"
         "Example: \"--report-all 1\"\n"
         "Attaches a single report containing everything below the global simulation tree and "
         "writes the output to destination") // Brief
        ("report-yaml-replacements",
         named_value<std::vector<std::vector<std::string>>>(
             "<placeholder_name> <value> <placeholder_name> <value> ...", 2, INT_MAX)->multitoken(),
         "Specify placeholder values to replace %PLACEHOLDER% specifiers in report description yaml files. \n")
        ("log-memory-usage",
         named_value<std::vector<std::vector<std::string>>>("[DEF_FILE]", 0, 1)->multitoken(),
         "Example: \"--log-memory-usage memory.yaml\"",
         "Capture memory usage statistics at periodic intervals throughout simulation")
        ("retired-inst-counter-path",
         named_value<std::string>("FILENAME", &sim_config_.parsed_path_to_retired_inst_counter_),
         "From 'top.core*', what is the path to the counter specifying "
         "retired instructions on a given core? \n"
         "For example, if the paths are: \n"
         "             top.core0.rob.stats.total_number_retired \n"
         "             top.core1.rob.stats.total_number_retired \n"
         "         Then the 'retired-inst-counter-path' is: \n"
         "             rob.stats.total_number_retired",
         "Path to the counter specifying retired instructions on a given core")
        ("generate-stats-mapping",
         "Automatically generate 1-to-1 mappings from CSV report column "
         "headers to StatisticInstance names",
         "Generate mappings from report headers to statistics names")
        ("no-json-pretty-print",
         "Disable pretty print / verbose print for all JSON statistics reports")
        ("omit-zero-value-stats-from-json_reduced",
         "Omit all statistics that have value 0 from json_reduced statistics reports")
        ("report-verif-output-dir",
         named_value<std::vector<std::string>>("DIR_NAME", 1, 1),
         "When SimDB report verification is enabled, this option will send all verification "
         "artifacts to the specified directory, relative to the current working directory.")
        ("report-warmup-icount",
         named_value<uint64_t>(""),
         "DEPRECATED")
        ("report-warmup-counter",
         named_value<std::vector<std::string>>("", 2, 2)->multitoken(),
         "DEPRECATED")
        ("report-update-ns",
         named_value<uint64_t>(""),
         "DEPRECATED")
        ("report-update-cycles",
         named_value<std::vector<std::vector<std::string>>>("", 1, 2)->multitoken(),
         "DEPRECATED")
        ("report-update-icount",
         named_value<std::vector<std::string>>("", 1, 3)->multitoken(),
         "DEPRECATED")
        ("report-update-counter",
         named_value<std::vector<std::string>>("", 2, 2)->multitoken(),
         "DEPRECATED")
        ("report-on-error",
         "Write reports normally even if simulation that has made it into the 'running' stage is "
         "exiting because of an exception during a run. This includes the automatic summary. "
         "Normally, reports are only written if simulation succeeds. Note that this does not apply "
         "to exits caused by fatal signal such as SIGKILL/SIGSEGV/SIGABRT, etc.",
         "Writes all reports even when run exits with error.")
        ;

    // SimDB Options
    simdb_opts_.add_options()
      ("simdb-dir",
       named_value<std::vector<std::string>>("DIR", 1, 1),
       "Specify the location where the simulation database will be written")
      ("simdb-enabled-components",
       named_value<std::vector<std::vector<std::string>>>("", 1, INT_MAX)->multitoken(),
       "Specify which simulator components should be enabled for SimDB access.\n"
       "Example: \"--simdb-enabled-components dbaccess.yaml\"")
        ;

    // SimDB Options (internal / developer use)
    simdb_internal_opts_.add_options()
      ("collect-legacy-reports",
       named_value<std::vector<std::string>>("DIR", 1, INT_MAX)->multitoken(),
       "Specify the root directory where all legacy report files will be written. "
       "This directory will be created if needed. Optionally supply one or more "
       "specific report format types that you *only* want to be collected, otherwise "
       "all report formats will be collected by default.\n"
       "Example: \"--collect-legacy-reports test/report/dir\"\n"
       "Example: \"--collect-legacy-reports test/report/dir json_reduced csv_cumulative\"")
      ;

    // Feature Options
    feature_opts_.add_options()
        ("feature",
         named_value<std::vector<std::vector<std::string>>>("NAME VALUE [options file(s)]", 2, INT_MAX)->multitoken(),
         "Enable a feature by name and value.\n"
         "Example: \"--feature hello_world 2\" would set the 'hello_world' feature value to 2")
        ;

    // Advanced Options
    advanced_opts_.add_options()
        ("no-colors",
         "Disable color in most output. Including the colorization in --show-tree.")
        ("show-hidden",
         "Show hidden nodes in the tree printout (--show-tree). Implicitly turns on --show-tree")
        ("verbose-config",
         "Display verbose messages when parsing any files (e.g. parameters, report definitions, "
         " etc.). This is not a generic verbose simulation option.")
        ("verbose-report-triggers",
         "Display verbose messages whenever report triggers are hit")
        ("show-options",
         "Show the options parsed from the command line")
        ("debug-sim",
         "Turns on simulator-framework debugging output. This is unrelated to general debug "
         "logging")
      #ifdef SPARTA_PYTHON_SUPPORT
        ("python-shell",
         "Use the Python shell")
      #endif
        ("auto-summary",
         named_value<std::string>("OPTION", &auto_summary_)->default_value(auto_summary_),
         "Controls automatic summary at destruction. Valid values include 'off': Do not write "
         "summary, 'on' or 'normal': (default) Write summary after running, and 'verbose': Write "
         "summary with detailed descriptions of each statistic",
         "Controls automatic summary at destruction. Valid values are {off,on,verbose}") // Brief"
        ;

    // Declare positional options
    //positional_opts_.add("thing", -1);
}

CommandLineSimulator::~CommandLineSimulator()
{
    if(sim_config_.debug_sim){
        // Logging destinations used
        std::cout << "\nLogging destinations used:" << std::endl;
        sparta::log::DestinationManager::dumpDestinations(std::cout, true);

        // Diagnostic printing of all unfreed TreeNodes. A few are expected
        std::cout << "\nSimulator Debug: Unfreed TreeNodes List (some globals expected):" << std::endl;
        std::cout << sparta::TreeNode::formatAllNodes() << std::endl;
    }
}

int CommandLineSimulator::parse(int argc,
                                char** argv) {
    std::cerr << "This application uses the deprecated CommandLineSimulator::parse signature" << std::endl;
    int err_code;
    return !parse(argc, argv, err_code);
}

bool CommandLineSimulator::parse(int argc,
                                 char** argv,
                                 int& err_code)
{
    argc_ = argc;
    argv_ = argv;
    ReportDescVec reports;

    // Note: it is safe to reparse, but probably a bad idea

    po::options_description all_opts("All Options", OPTIONS_DOC_WIDTH);
    all_opts.add(sparta_opts_.getVerboseOptions())
            .add(param_opts_.getVerboseOptions())
            .add(run_time_opts_.getVerboseOptions())
            .add(debug_opts_.getVerboseOptions())
            .add(log_opts_.getVerboseOptions())
            .add(pipeout_opts_.getVerboseOptions())
            .add(report_opts_.getVerboseOptions())
            .add(simdb_opts_.getVerboseOptions())
            .add(simdb_internal_opts_.getVerboseOptions())
            .add(app_opts_.getVerboseOptions())
            .add(feature_opts_.getVerboseOptions())
            .add(advanced_opts_.getVerboseOptions());

    // --arch option values (pattern, filename).
    utils::ValidValue<std::pair<std::string, std::string>> arch_pattern_name;
    // --config-file / --node-config-file / --read-final-config (pattern, filename)
    std::vector<std::tuple<std::string, std::string, bool>> config_pattern_names;
    // --parameter / -p (pattern, value as a string)
    std::vector<std::tuple<std::string, std::string, bool>> individual_parameter_values;

    // Parse options from command line
    try{
        std::vector<std::string> pos_opts;
        po::parsed_options opts = po::command_line_parser(argc, argv)
            .options(all_opts)
            .positional(positional_opts_)
            //.allow_unregistered() // Allow unregistered options to pass through
            .run();

        // Interpret parameter/config-file/node-config-file options in the order
        // given on the command line. Boost does not support this by
        int32_t latest_pos_key = -1;
        std::string last_pos_string_key = "";

        // How many times have we processed a pipeline-collection option
        bool collection_parsed = false;
        // Have we set any kind of delay'ed starting of report/collection/pevent output tools
        // We cannot use debug-on and --pevents-warmup-icount during the same run because
        // I'm iffy about what would happen.
        bool delayed_start = false;
        uint32_t dash_p_config_applicators_used = 0;

        bool throw_report_deprecated = false;
        for(size_t i = 0; i < opts.options.size(); /*increment conditionally*/){
            // Option: parameter -1 [top.cpu0.params.foo, 100] [-p, top.cpu0.params.foo, 100] 0 0
            auto o = opts.options[i];

            // Option debug
            //std::cout << "Option: " << o.string_key << " " << o.position_key << " "
            //          << o.value << " " << o.original_tokens << " " << o.unregistered
            //          << " " << o.case_insensitive << std::endl;

            // Update the positional key if there is one and track the latest.
            // Sometimes, we may shift new tokens into the positional arguments
            // which shifts later positional arguments in opts back (to higher
            // indices)
            if(o.position_key != -1){
                last_pos_string_key = o.string_key;
                if(o.position_key <= latest_pos_key){
                    o.position_key = ++latest_pos_key;
                }
                for(auto& s : o.value){
                    pos_opts.push_back(s);
                    opts.options.erase(opts.options.begin() + i);
                }
            }else if(o.string_key == "parameter"){
                if(o.value.size() != 2){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 2.\nExample:\n   -p top.core0.params.foo value"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                std::string pattern = o.value[0];
                std::string value = o.value[1];
                individual_parameter_values.emplace_back(pattern, value, false);
                config_applicators_used_++;
                dash_p_config_applicators_used++;
                opts.options.erase(opts.options.begin() + i);

            }else if(o.string_key == "optional-parameter"){
                if(o.value.size() != 2){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 2.\nExample:\n   --optional-parameter top.core0.params.foo value"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                std::string pattern = o.value[0];
                std::string value = o.value[1];
                config_applicators_used_++;
                dash_p_config_applicators_used++;
                individual_parameter_values.emplace_back(pattern, value, true);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "arch"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --arch my_arch"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                std::string pattern = ""; // global node
                std::string filename = o.value[0];
                config_applicators_used_++;
                // Store pair for now and resolve filename to an
                // architecture file/dir after parsing.  This will
                // take the last --arch on the command line
                arch_pattern_name = std::make_pair(pattern, filename);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "arch-search-dir"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --arch-search-dir /my/architectures/"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Prepend a directory to the list of search directories
                sim_config_.addArchSearchPath(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "config-search-dir"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --config-search-dir /my/configurations/"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Prepend a directory to the list of search directories
                sim_config_.addConfigSearchPath(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "extension-file"){
                if(o.value.size() != 1) {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --extension-file extensions.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Prepend a directory to the list of search directories
                sim_config_.processExtensionFile(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "enable-state-tracking") {
                if(o.value.size() != 1) {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --enable-state-tracking Histograms.txt"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                sim_config_.setStateTrackingFile(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "control"){
                if (o.value.size() != 1) {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample: \n   --control ctrl_expressions.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Prepend a simulation control file to parse for trigger expressions
                sim_config_.addControlFile(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "report-search-dir"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --report-search-dir /my/report/definitions/"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Prepend a directory to the list of search directories
                sim_config_.addReportDefnSearchPath(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "node-config-file"){
                if(o.value.size() != 2){
                    std::cerr << "command-line option \"" << o.string_key
                              << "\" had " << o.value.size()
                              << " tokens but requires 2.\nExample:\n   --node-config-file top.core0 params.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                std::string pattern = o.value[0];
                std::string filename = o.value[1];
                config_applicators_used_++;
                config_pattern_names.emplace_back(pattern, filename, false);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "config-file"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --config-file params.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                std::string pattern = ""; // top node
                std::string filename = o.value[0];
                config_applicators_used_++;
                config_pattern_names.emplace_back(pattern, filename, false);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "read-final-config"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --read-final-config params.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }

                // atleast enforce that --read-final-config comes first in cml because reading
                // a final config trumps all stats, so in order for -p overrides to
                // actually have an effect, they must occure after --read-final-config.
                if (dash_p_config_applicators_used > 0)
                {
                    std::cerr << "ERROR: command-line option \"" << "--read-final-config" << "\" must appear before other -p or -c options on command line." << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }

                std::string pattern = ""; // top node
                std::string filename = o.value[0];
                config_pattern_names.emplace_back(pattern, filename, true);
                opts.options.erase(opts.options.begin() + i);
            }else if (o.string_key == "write-final-config" || o.string_key == "write-final-config-verbose"){
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1.\nExample:\n   --write-final-config final.yaml"
                              << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Piggy back on ParameterApplicator to switch the meta parameter for is_final_config
                // to true when we have the --write-final-config option used.
                // Honestly, this may be a mute point since there isn't another way to generate
                // a config file, so any generated config file would be a final config, but
                // maybe this would change in the future.
                sim_config_.processParameter("meta.params.is_final_config", "true");
                if (o.string_key == "write-final-config")
                {
                    final_config_file_ = o.value[0];
                }
                else
                {
                    final_config_file_verbose_ = o.value[0];
                }
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "log"){
                if(o.value.size() != 3){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 3.\nExample:\n   --log top.core0 warning core0_warnings.log"
                              << std::endl;
                    printUsageHelp_();
                    showLoggingHelp();
                    err_code = 1;
                    return false;
                }
                std::string pattern = o.value[0];
                std::string cat = o.value[1];
                std::string dest = o.value[2];
                sim_config_.enableLogging(pattern, cat, dest);
                opts.options.erase(opts.options.begin() + i);
            }else if(o.string_key == "report"){
                if (o.value.size() == 1) {
                    // Add any report descriptors parsed from .yaml files
                    // specified with the '--reports' option
                    report_descriptor_def_files_.emplace_back(o.value[0]);
                    ++i;
                } else if (o.value.size() == 2) {
                    report_specific_yaml_placeholder_replacements_[o.value[0]] =
                        app::createReplacementsFromYaml(o.value[1]);
                    report_descriptor_def_files_.emplace_back(o.value[0]);
                    opts.options.erase(opts.options.begin() + i);
                } else {
                    std::string pattern;
                    std::string def_file;
                    std::string dest_file;
                    if(o.value.size() >= 3){
                        pattern = o.value[0];
                        def_file = o.value[1];
                        dest_file = o.value[2];
                    }
                    if(o.value.size() == 3){
                        reports.emplace_back(pattern, def_file, dest_file);
                        opts.options.erase(opts.options.begin() + i);
                    }else if(o.value.size() == 4){
                        std::string format = o.value[3];
                        if(ReportDescriptor::isValidFormatName(format)){
                            //std::cout << "--report is taking formatter \"" << format << "\"" << std::endl;
                            reports.emplace_back(pattern, def_file, dest_file, format);
                        }else{
                            // Move "format" to positional args because it is not understood as a formatter
                            //std::cout << "--report is NOT taking formatter \"" << format << "\"" << std::endl;
                            pos_opts.push_back(format);
                            reports.emplace_back(pattern, def_file, dest_file);
                        }
                        opts.options.erase(opts.options.begin() + i);
                    }else{
                        std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                                  << " tokens but requires 1, 3 or 4. Examples:\n"
                                  << "     --report all_reportsdefinition_file.yaml\n"
                                  << "     --report top.core0 report_def.yaml report.out\n"
                                  << "     --report top.core0 report_def.yaml report.out csv" << std::endl;
                        printUsageHelp_();
                        showReportsHelp();
                        err_code = 1;
                        return false;
                    }
                }
            }else if(o.string_key == "report-all"){
                std::string pattern = "";
                std::string def_file = "@";
                std::string dest_file;
                if(o.value.size() >= 1){
                    dest_file = o.value[0];
                }
                if(o.value.size() == 1){
                    reports.emplace_back(pattern, def_file, dest_file);
                    opts.options.erase(opts.options.begin() + i);
                }else if(o.value.size() == 2){
                    std::string format = o.value[1];
                    if(ReportDescriptor::isValidFormatName(format)){
                        //std::cout << "--report is taking formatter \"" << format << "\"" << std::endl;
                        reports.emplace_back(pattern, def_file, dest_file, format);
                    }else{
                        // Move "format" to positional args because it is not understood as a formatter
                        //std::cout << "--report-all is NOT taking formatter \"" << format << "\"" << std::endl;
                        pos_opts.push_back(format);
                        reports.emplace_back(pattern, def_file, dest_file);
                    }
                    opts.options.erase(opts.options.begin() + i);

                }else{
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1 or 2.\nExample:\n   --report-all report.out"
                              << std::endl;
                    printUsageHelp_();
                    showReportsHelp();
                    err_code = 1;
                    return false;
                }
            } else if (o.string_key == "report-yaml-replacements") {
                // Placeholder name/value pairs. These values are provided at the command
                // prompt in order to turn a report description yaml file like this:
                //
                // # Description file "template.yaml"
                //
                //    content:
                //      report:
                //        pattern:   _global
                //        def_file:  simple_stats.yaml
                //        dest_file: %TRACENAME%.stats.json
                //        format:    json_reduced
                //        ...
                //
                // # Into this:
                //
                //    content:
                //      report:
                //        pattern:   _global
                //        def_file:  simple_stats.yaml
                //        dest_file: my_foo_report.stats.json
                //        format:    json_reduced
                //        ...
                //
                // By running the command:
                //   <exe> --report template.yaml --report-yaml-replacements TRACENAME my_foo_report
                if (o.value.size() % 2 == 1) {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires an even number.\nExample:\n   --report-yaml-replacements"
                              << " DEF_FILE core_stats.yaml DEST_FILE out.json"
                              << std::endl;
                    printUsageHelp_();
                    showReportsHelp();
                    err_code = 1;
                    return false;
                }
                for (size_t idx = 0; idx < o.value.size() - 1; idx += 2) {
                    report_yaml_placeholder_replacements_.emplace_back(o.value[idx], o.value[idx+1]);
                }
                opts.options.erase(opts.options.begin() + i);
            }else if (o.string_key == "log-memory-usage") {
                std::string def_file = "@";
                if (!o.value.empty()) {
                    def_file = o.value[0];
                }
                sim_config_.setMemoryUsageDefFile(def_file);
                opts.options.erase(opts.options.begin() + i);
            }else if (o.string_key == "report-verif-output-dir") {
                db::ReportVerifier::writeVerifResultsTo(o.value[0]);
                opts.options.erase(opts.options.begin() + i);
            }else if (o.string_key == "report-warmup-icount") {
                throw_report_deprecated = true;
                ++i;
            }else if (o.string_key == "report-warmup-counter") {
                throw_report_deprecated = true;
                ++i;
            }else if (o.string_key == "report-update-ns") {
                throw_report_deprecated = true;
                ++i;
            }else if (o.string_key == "report-update-cycles") {
                throw_report_deprecated = true;
                ++i;
            }else if (o.string_key == "report-update-counter") {
                throw_report_deprecated = true;
                ++i;
            }else if (o.string_key == "report-update-icount") {
                throw_report_deprecated = true;
                ++i;

            }else if (o.string_key == "pipeline-collection") {
                //Enforce that we cannot set pipeline-collection options twice.
                if(collection_parsed)
                {
                    std::cerr <<"command-line option \"" << o.string_key << " was used multiple times."
                              << " You may only specify this option once. " << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                if(o.value.size() < 1 || o.value.size() > 2)
                {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 1 or 2. \nExample -z output_ top.core0" << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                //Check to make sure we are --pipeline-collection was not set twice.
                sim_config_.pipeline_collection_file_prefix = o.value.at(0);

                // Check that a valid file prefix was given
                if(sim_config_.pipeline_collection_file_prefix.empty()){
                    std::cerr << "Command line supplied an empty path for pipeline collection. "
                                 "This likely wasn't intended and is considered mis-use. Supply a "
                                 "non-empty string as the pipeout file prefix";
                    err_code = 1;
                    return false;
                }

                ++i;
                collection_parsed = true;

            } else if (o.string_key.find("collection-at") != std::string::npos) {
                if(!collection_parsed)
                {
                    std::cerr << "command-line option \"" << o.string_key << "\" must follow a --pipeline-collection option."
                              << " Please specify -z or --pipeline-collection in your command line before --collection-at or -k" << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                //you are required to specify one node per usage of collection-at, for multiple nodes
                //use collection-at many times.
                if(o.value.size() != 1){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires exactly 1.  See help message." << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                if(o.string_key.find("argos") != std::string::npos) {
                    // --argos-collection-at <file>
                    if(!openALFAndFindPipelineNodes_(o.value.at(0))) {
                        std::cerr << "Could not open/parse Argos ALF file: " << o.value.at(0)
                                  << std::endl;
                        err_code = 1;
                        return false;
                    }
                }
                else {
                    // Add the node specified to a list of names to be started by pipeline collection.
                    pipeline_enabled_node_names_.insert(o.value.at(0));
                }
                ++i;
            }else if (o.string_key == "pevents") {
                if(o.value.size() != 2){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 2. \n Example: \n --pevents log.pevents ALL" << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }

                //Setup the temporary global pevent logger to log some events
                bool verbose = false;
                for (auto& ev : TreeNode::parseNotificationNameString(o.value.at(1)))
                {
                    pevent_controller_.cacheTap(o.value.at(0), *ev, verbose);
                }

                ++i;
            }else if (o.string_key == "verbose-pevents") {
                if(o.value.size() != 2){
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requires 2. \n Example: \n --pevents log.pevents ALL" << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }

                //Setup the temporary global pevent logger to log some events
                bool verbose = true;
                for (auto& ev : TreeNode::parseNotificationNameString(o.value.at(1)))
                {
                    pevent_controller_.cacheTap(o.value.at(0), *ev, verbose);
                }

                ++i;
            }else if (o.string_key == "pevents-at") {
                if(o.value.size() != 3)
                {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requres 3. \n Example: \n --pevents-at retire.log top.core0.retire RETIRE" << std::endl;


                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Let the user add their pevent.
                // cacheTap(file, category, verbose, node_path)
                pevent_controller_.cacheTap(o.value.at(0),  o.value.at(2), false, o.value.at(1));
                ++i;

            }else if (o.string_key == "verbose-pevents-at") {
                if(o.value.size() != 3)
                {
                    std::cerr << "command-line option \"" << o.string_key << "\" had " << o.value.size()
                              << " tokens but requres 3. \n Example: \n --pevents-at retire.log top.core0.retire RETIRE" << std::endl;


                    printUsageHelp_();
                    err_code = 1;
                    return false;
                }
                // Let the user add their pevent.
                // cacheTap(file, category, verbose, node_path)
                pevent_controller_.cacheTap(o.value.at(0),  o.value.at(2), true, o.value.at(1));
                ++i;

            }else if(o.string_key == "run-length") {
                size_t end_pos;
                try {
                    // try -r <number>
                    if(o.value.at(0) == ""){
                        throw SpartaException(""); // Fall through to other case if empty string (ticks)
                    }
                    run_time_cycles_ = utils::smartLexicalCast<uint64_t>(o.value.at(0), end_pos);
                    // We are likely tripping over a position argument.
                    if(o.value.size() == 2)
                    {
                        pos_opts.push_back(o.value.at(1));
                    }
                } catch (...){
                    // try -r <corename> <number>
                    if(o.value.size() == 2) {
                        runtime_clock_ = o.value.at(0);
                        try {
                            run_time_cycles_ = utils::smartLexicalCast<uint64_t>(o.value.at(1), end_pos);
                        }
                        catch(...){
                            throw SpartaException("run-length must take an integer value, not \"")
                                << o.value.at(1) << "\"";
                        }
                    }
                    else {
                        throw SpartaException("run-length 1 argument must take an integer value, not \"")
                            << o.value.at(0) << "\"";
                    }
                }
                ++i;
            } else if(o.string_key == "debug-on") {
                if(delayed_start)
                {
                    std::cerr << MULTI_INSTRUCTION_TRIGGER_ERROR_MSG << std::endl;

                }
                delayed_start = true;

                if(sim_config_.trigger_on_value != static_cast<uint64_t>(SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE)) {
                    throw SpartaException("Cannot use both --debug-on and --debug-on-icount simultaneously");
                }

                // Parse the debug trigger on cycle number
                size_t end_pos;
                try {
                    // try --debug-on <number>
                    if(o.value.at(0) == ""){
                        throw SpartaException(""); // Fall through to other case if empty string (ticks)
                    }
                    sim_config_.trigger_on_value = utils::smartLexicalCast<uint64_t>(o.value.at(0), end_pos);
                    // We are likely tripping over a position argument.
                    if(o.value.size() == 2)
                    {
                        pos_opts.push_back(o.value.at(1));
                    }
                } catch (...){
                    // try --debug-on <corename> <number>
                    if(o.value.size() == 2) {
                        sim_config_.trigger_clock = o.value.at(0);
                        try {
                            sim_config_.trigger_on_value = utils::smartLexicalCast<uint64_t>(o.value.at(1), end_pos);
                        }
                        catch(...){
                            throw SpartaException("debug-on must take an integer value, not \"")
                                << o.value.at(1) << "\"";
                        }
                    }
                    else {
                        throw SpartaException("debug-on with one argument must take an integer value, not \"")
                            << o.value.at(0) << "\"";
                    }
                }

                if(sim_config_.trigger_on_value > 0){
                    sim_config_.trigger_on_type = SimulationConfiguration::TriggerSource::TRIGGER_ON_CYCLE;
                }
                ++i;
            } else if(o.string_key == "debug-on-icount") {
                if(delayed_start)
                {
                    std::cerr << MULTI_INSTRUCTION_TRIGGER_ERROR_MSG << std::endl;
                }
                delayed_start = true;

                if(sim_config_.trigger_on_value != static_cast<uint64_t>(SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE)){
                    throw SpartaException("Cannot use both --debug-on and --debug-on-icount simultaneously");
                }

                // Parse the debug trigger on cycle number
                try {
                    size_t end_pos;
                    // try --debug-on-icount <number>
                    sim_config_.trigger_on_value = utils::smartLexicalCast<uint64_t>(o.value.at(0), end_pos);
                    sim_config_.trigger_on_type = SimulationConfiguration::TriggerSource::TRIGGER_ON_INSTRUCTION;
                } catch (...){
                    throw SpartaException("debug-on-icount must take an integer value, not \"")
                        << o.value.at(0) << "\"";
                }
                ++i;
            } else if(o.string_key == "wall-timeout" || o.string_key == "cpu-timeout") {

                size_t end_pos;
                // Tell the scheduler that we are going to need to timeout.
                double hours = utils::smartLexicalCast<double>(o.value.at(0), end_pos);
                std::chrono::duration<double, std::ratio<3600> > duration(hours);
                bool clean_exit;
                // make stuff lower case.
                boost::to_lower(o.value.at(1));

                if(o.value.at(1) == "clean")
                {
                    clean_exit = true;
                }
                else if(o.value.at(1) == "error")
                {
                    clean_exit = false;
                }
                else{
                    throw SpartaException("wall-timeout and cpu-timeout can either exit clean or error, not \"")
                        << o.value.at(1) << "\"";
                }

                bool use_wall_clock;
                if(o.string_key == "cpu-timeout")
                    use_wall_clock = false;
                else if (o.string_key == "wall-timeout")
                    use_wall_clock = true;
                else
                    sparta_assert(false); // one can only hope that we can't get here logically.
                std::cout << " set timeout to " << hours << " hours" << std::endl;
                SleeperThread::getInstance()->setTimeout(duration, clean_exit, use_wall_clock);
                ++i;
            } else if(o.string_key == "simdb-dir") {
                const std::string & db_dir = o.value[0];
                auto p = bfs::path(db_dir);
                if (!bfs::exists(p)) {
                    bfs::create_directories(p);
                } else if (!bfs::is_directory(p)) {
                    throw SpartaException("Invalid 'simdb-dir' argument. Path ")
                        << "exists but is not a directory.";
                }
                sim_config_.setSimulationDatabaseLocation(db_dir);
                ++i;
            } else if(o.string_key == "simdb-enabled-components") {
                std::vector<std::string> yaml_opts_files;
                auto is_yaml_file = [](const std::string & opt) {
                    auto p = bfs::path(opt);
                    return (bfs::exists(p) && !bfs::is_directory(p));
                };

                for (size_t idx = 0; idx < o.value.size(); ++idx) {
                    sparta_assert(is_yaml_file(o.value[idx]),
                                "File not found: " << o.value[idx]);
                    yaml_opts_files.emplace_back(o.value[idx]);
                }

                for (const auto & opt_file : yaml_opts_files) {
                    sim_config_.addSimulationDatabaseAccessOptsYaml(opt_file);
                }
                ++i;
            } else if(o.string_key == "collect-legacy-reports") {
                const std::string & reports_root_dir = o.value[0];
                auto p = bfs::path(reports_root_dir);
                if (!bfs::exists(p)) {
                    bfs::create_directories(p);
                } else if (!bfs::is_directory(p)) {
                    throw SpartaException("Invalid 'collect-legacy-reports' argument. Path ")
                        << "exists but is not a directory.";
                }
                std::set<std::string> collected_formats;
                for (size_t idx = 1; idx < o.value.size(); ++idx) {
                    collected_formats.insert(o.value[idx]);
                }
                sim_config_.setLegacyReportsCopyDir(reports_root_dir, collected_formats);
                ++i;
            } else if(o.string_key == "feature") {
                const std::string & name = o.value[0];
                const int value = boost::lexical_cast<int>(o.value[1]);
                feature_config_.setFeatureValue(name, value);
                for (size_t opts_file_idx = 2; opts_file_idx < o.value.size(); ++opts_file_idx) {
                    feature_config_.setFeatureOptionsFromFile(name, o.value[opts_file_idx]);
                }
                opts.options.erase(opts.options.begin() + i);
            }
            else{
                ++i;
            }
        }

        if (throw_report_deprecated) {
            std::ostringstream oss;

            oss << std::endl;
            oss << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *" << std::endl;
            oss << "The following command-line options have been deprecated: " << std::endl;
            oss << "\t--report-warmup-icount"   << std::endl;
            oss << "\t--report-warmup-counter"  << std::endl;
            oss << "\t--report-update-ns"       << std::endl;
            oss << "\t--report-update-cycles"   << std::endl;
            oss << "\t--report-update-counter"  << std::endl;
            oss << "\t--report-update-icount"   << std::endl << std::endl;
            oss << "Please refer to the files 'ReportTriggers.txt' and 'SubreportTriggers.txt'" << std::endl;
            oss << "found in this directory for more information on how to specify these options" << std::endl;
            oss << "from YAML files directly." << std::endl;
            oss << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *";
            oss << std::endl;

            throw SpartaException(oss.str());
        }

        // The only config applicators that can be used along with read-final-config is -p options.
        // not --arch or -c options. This is because really, --read-final-config should contain MOST
        // config. -p can override a few things like trace parameters or something, but there should
        // not be major configuration changes involved while using --read-final-config.
        if (sim_config_.hasFinalConfig() && (config_applicators_used_ > 0))
        {
            // we give -p applicators a pass, but if any other config applicators
            // were used we need to error out.
            if (dash_p_config_applicators_used != config_applicators_used_)
            {
                    std::cerr <<"command-line option \"--read-final-config\" was used in conjunction with "
                              << "other config applicators such as -c or --arch. This is not allowed with --read-final-config. " << std::endl;
                    printUsageHelp_();
                    err_code = 1;
                    return false;
            }

        }

        // Parse only the positional arguments
        const std::string dummy;
        const char* const separator = "--";
        std::vector<const char*> new_argv{dummy.c_str(), separator}; // Start with blank arg to ignore and "--" so all are interpreted as positional
        for(auto& s : pos_opts){
            new_argv.push_back(s.c_str());
        }
        const int opts_style = (pocls::allow_short | pocls::short_allow_adjacent | pocls::short_allow_next
                              | pocls::allow_long | pocls::long_allow_adjacent | pocls::long_allow_next
                              | pocls::allow_sticky /* | pocls::allow_guessing */
                              | pocls::allow_dash_for_short);
        po::parsed_options new_opts = po::command_line_parser(new_argv.size(), &(new_argv[0]))
            .positional(positional_opts_)
            .style(opts_style)
            .run();

        // Merge the positional options into the first set of options
        for(auto& o : new_opts.options){
            opts.options.push_back(o);
        }

        // Allow subclass to modify parsed opts or interpret them in order
        postParse_(opts);

        po::store(opts, vm_);
        po::notify(vm_);
    }catch(po::multiple_occurrences& ex){
        std::cerr << "Error:\n  " << ex.what() << " from option \"" << ex.get_option_name() << "\""
                  << std::endl;
        printUsageHelp_();
        err_code = 1;
        return false;
    }catch(po::error& ex){
        std::cerr << "Error:\n  " << ex.what() << std::endl;
        printUsageHelp_();
        err_code = 1;
        return false;
    }catch(SpartaException& ex){
        std::cerr << "Error:\n  " << ex.what() << std::endl;
        printUsageHelp_();
        err_code = 1;
        return false;
    }

    // Interpret options
    if(help_topic_ != INVALID_HELP_TOPIC){
        if(help_topic_ == "topics"){
            showHelpTopics_();
        }else if(help_topic_ == "all" || help_topic_ == "verbose"){
            showVerboseHelp_();
        }else if(help_topic_ == "brief"){
            showBriefHelp_();
        }else if(help_topic_ == "parameters"){
            std::cout << param_opts_.getOptionsLevelUpTo(0) << std::endl;
            showConfigHelp();
        }else if(help_topic_ == "logging"){
            std::cout << log_opts_.getOptionsLevelUpTo(0) << std::endl;
            showLoggingHelp();
        }else if(help_topic_ == "reporting"){
            std::cout << report_opts_.getOptionsLevelUpTo(0) << std::endl;
            showReportsHelp();
        }else if(help_topic_ == "pipeout"){
            std::cout << pipeout_opts_.getOptionsLevelUpTo(0) << std::endl;
        }else{
            std::cout << "Unknown topic for --help-topic \"" << help_topic_
                      << "\". Valid topics are:" << std::endl;
            showHelpTopics_();
            err_code = 1;
            return false;
        }
        err_code = 0;
        return false;
    }

    if(vm_.count("help-brief")){
        showBriefHelp_();
        err_code = 0;
        return false;
    }

    if(vm_.count("help")){
        showVerboseHelp_();
        err_code = 0;
        return false;
    } else if(vm_.count("verbose-help")){
        std::cout << "Warning: --verbose-help is deprecated and will be removed in SPARTA 1.5. Use --help instead" << std::endl;
        showVerboseHelp_();
        err_code = 0;
        return false;
    }

    if(vm_.count("no-run")){
        no_run_mode_ = true;
    }

    if (vm_.count("generate-stats-mapping")) {
        sim_config_.generateStatsMapping();
    }

    if (vm_.count("no-json-pretty-print")) {
        sim_config_.disablePrettyPrintReports("json");
    }

    if (vm_.count("omit-zero-value-stats-from-json_reduced")) {
        sim_config_.omitStatsWithValueZeroForReportFormat("json_reduced");
    }

    // Check for valid arch config if required by defaults
    if(arch_pattern_name.isValid()) {
        sim_config_.processArch(arch_pattern_name.getValue().first, arch_pattern_name.getValue().second);
    }
    else {
        if(!sim_config_.archFileProvided()) {
            if(sim_config_.getDefaults().non_empty_arch_arg_required && sim_config_.getDefaults().arch_arg_default.empty()) {
                throw SpartaException("This simulator requires an architecture be selected with --arch to proceed: ")
                    << utils::ARCH_OPTIONS_RESOLUTION_RULES;
            }
            else if(!sim_config_.getDefaults().arch_arg_default.empty()) {
                // Parse the default arch file provided since one was not
                // provided on the command line
                std::string pattern = ""; // Start from global node
                sim_config_.processArch(pattern, sim_config_.getDefaults().arch_arg_default);
            }
        }
    }

    // Now that all --config-search-dir option(s) have been parsed, apply configurations
    for (const auto & cfg : config_pattern_names) {
        const std::string & pattern = std::get<0>(cfg);
        const std::string & filename = std::get<1>(cfg);
        const bool is_final = std::get<2>(cfg);
        sim_config_.processConfigFile(pattern, filename, is_final);
    }

    // **After** all arch/config/node-config yamls have been applied, consume
    // any --parameter/-p values to the sim config
    for (const auto & pvalue : individual_parameter_values) {
        const std::string & pattern = std::get<0>(pvalue);
        const std::string & value = std::get<1>(pvalue);
        const bool is_optional = std::get<2>(pvalue);
        //Individual extensions name/value pairs must be forwarded
        //to the dedicated ParameterTree for extensions.
        if (pattern.find(".extension") != std::string::npos) {
            auto & extensions_ptree = sim_config_.getExtensionsUnboundParameterTree();
            extensions_ptree.set(pattern, value, !is_optional);
        } else {
            sim_config_.processParameter(pattern, value, is_optional);
        }
    }

    // Interpret debug-dump post-run value
    std::transform(dump_debug_type_.begin(),
                   dump_debug_type_.end(),
                   dump_debug_type_.begin(),
                   ::tolower);
    if(dump_debug_type_ != "error"
       && dump_debug_type_ != "always"
       && dump_debug_type_ != "never"){
        std::cerr << "Error: value values for --dump-debug-post-run are 'error', 'never', 'always'. '"
                  << dump_debug_type_ << "'' was not understood" << std::endl;
        printUsageHelp_();
        printOptionsHelp_(MultiDetailOptions::BRIEF);
        err_code = 1;
        return false;
    }

    if(dump_debug_type_ == "always"){
        sim_config_.debug_dump_policy = SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_ALWAYS;
    }else if(dump_debug_type_ == "never"){
        sim_config_.debug_dump_policy = SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_NEVER;
    }else if(dump_debug_type_ == "error"){
        sim_config_.debug_dump_policy = SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_ERROR;
    }else{
        // Note: This should have been caught in parse()
        sparta_assert(0, "Unknown debug post-run value: '" << dump_debug_type_
                             << "'. This should have been caught during parsing");
    }

    if (debug_dump_options_ == "all") {
        sim_config_.debug_dump_options =
            SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_EVERYTHING;
    } else if (debug_dump_options_ == "asserts_only") {
        sim_config_.debug_dump_options =
            SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_NOTHING;
    } else if (debug_dump_options_ == "backtrace_only") {
        sim_config_.debug_dump_options =
            SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_BACKTRACE_ONLY;
    } else {
        throw SpartaException("Unrecognized debug dump option found: ")
            << debug_dump_options_
            << "\n\tValid options are: 'all', 'asserts_only', "
            << "or 'backtrace_only'";
    }

    sim_config_.validate_post_run = vm_.count("validate-post-run") > 0;

    if (!sim_config_.parsed_path_to_retired_inst_counter_.empty()) {
        sim_config_.path_to_retired_inst_counter.first =
            sim_config_.parsed_path_to_retired_inst_counter_;
        sim_config_.path_to_retired_inst_counter.second =
            sparta::app::DefaultValues::RetiredInstPathStrictness::Strict;
        sim_config_.parsed_path_to_retired_inst_counter_.clear();
    }

    if(vm_.count("disable-infinite-loop-protection") > 0)
    {
        sparta::SleeperThread::getInstance()->disableInfiniteLoopProtection();
    }


    // Interpret auto-summary value
    std::transform(auto_summary_.begin(),
                   auto_summary_.end(),
                   auto_summary_.begin(),
                   ::tolower);
    if(auto_summary_ != "off"
       && auto_summary_ != "on"
       && auto_summary_ != "normal"
       && auto_summary_ != "verbose"){
        std::cerr << "Error: value values for --auto-summary are 'off', 'on'/'normal', 'verbose'. '"
                  << auto_summary_ << " was not understood" << std::endl;
        printUsageHelp_();
        printOptionsHelp_(MultiDetailOptions::BRIEF);
        err_code = 1;
        return false;
    }

    if(auto_summary_ == "off"){
        sim_config_.auto_summary_state = SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_OFF;
    }else if(auto_summary_ == "on"){
        sim_config_.auto_summary_state = SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_NORMAL; // on = normal
    }else if(auto_summary_ == "normal"){
        sim_config_.auto_summary_state = SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_NORMAL;
    }else if(auto_summary_ == "verbose"){
        sim_config_.auto_summary_state = SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_VERBOSE;
    }else{
        // Note: This should have been caught in parse()
        sparta_assert(0, "Unknown auto-summary value: '" << auto_summary_
                    << "'. This should have been caught during parsing");
    }

    show_tree_ = vm_.count("show-tree") > 0;
    show_parameters_ = vm_.count("show-parameters") > 0;
    show_ports_ = vm_.count("show-ports") > 0;
    show_counters_ = vm_.count("show-counters") > 0 || vm_.count("show-stats");
    show_clocks_ = vm_.count("show-clocks") > 0;
    show_notifications_ = vm_.count("show-notifications") > 0;
    show_loggers_ = vm_.count("show-loggers") > 0;
    // help-tree
    show_tree_ |= vm_.count("help-tree") > 0;
    no_run_mode_ |= vm_.count("help-tree") > 0;
    // help-parameters
    show_parameters_ |= vm_.count("help-parameters") > 0;
    no_run_mode_ |= vm_.count("help-parameters") > 0;
    // help-ports
    show_ports_ |= vm_.count("help-ports") > 0;
    no_run_mode_ |= vm_.count("help-ports") > 0;
    // help-counters
    show_counters_ |= vm_.count("help-counters") > 0 || vm_.count("help-stats");
    no_run_mode_ |= vm_.count("help-counters") > 0;
    // help-notifications
    show_notifications_ |= vm_.count("help-notifications") > 0;
    no_run_mode_ |= vm_.count("help-notifications") > 0;
    // help-loggers
    show_loggers_ |= vm_.count("help-loggers") > 0;
    no_run_mode_ |= vm_.count("help-loggers") > 0;
    // help-clocks
    show_clocks_ |= vm_.count("help-clocks") > 0;
    no_run_mode_ |= vm_.count("help-clocks") > 0;
    // help-pevents
    show_pevents_ |= vm_.count("help-pevents") > 0;
    no_run_mode_ |= vm_.count("help-pevents") > 0;

    show_hidden_ = vm_.count("show-hidden") > 0;
    if(show_hidden_){
      show_tree_= true; // Turns on show-tree
    }
    disable_colors_ = vm_.count("no-colors") > 0;
    if (disable_colors_)
    {
        sparta::color::ColorScheme::getDefaultScheme().enabled(false); // turn off colors on the default color scheme.
    }

    use_pyshell_                        = vm_.count("python-shell") > 0;
    sim_config_.show_dag                = vm_.count("show-dag") > 0;
    sim_config_.warn_stderr             = vm_.count("no-warn-stderr") == 0;
    sim_config_.verbose_cfg             = vm_.count("verbose-config") > 0;
    sim_config_.verbose_report_triggers = vm_.count("verbose-report-triggers") > 0;
    sim_config_.debug_sim               = vm_.count("debug-sim") > 0;
    sim_config_.report_on_error         = vm_.count("report-on-error") > 0;
    sim_config_.reports                 = reports;

    //pevents
    run_pevents_ = (vm_.count("pevents-at") > 0) | (vm_.count("pevents") > 0) | (vm_.count("verbose-pevents") > 0);

    bool show_options = vm_.count("show-options") > 0;
    if(show_options){
        // Print out parameters if allowed. Do not configure within this block

        std::cout << "Command-line Options:" << std::endl;
        std::cout << "  architecture:    [\n";
        sim_config_.printArchConfigurations(std::cout);
        std::cout << "    ]" << std::endl;

        std::cout << "  configuration(s):    [\n";
        sim_config_.printGenericConfigurations(std::cout);
        std::cout << "    ]" << std::endl;
        std::cout << "  logging taps(s):     [\n";
        for(const log::TapDescVec::value_type& t : sim_config_.getTaps()) {
            std::cout << "    " << t.stringize() << '\n';
        }
        std::cout << "    ]" << std::endl;
        std::cout << "  reports (s):         [\n";
        for(ReportDescVec::value_type& r : reports) {
            std::cout << "    " << r.stringize() << '\n';
        }

        std::cout << "    ]" << std::endl;
        std::cout << "  run-time:            " << run_time_cycles_ << " on clock: " << runtime_clock_ << std::endl;
        std::cout << "  warnings file:       \"" << sim_config_.warnings_file << '"' << std::endl;
        std::cout << "  final config out:    \"" << sim_config_.getFinalConfigFile() << '"' << std::endl;
        std::cout << "  power config out:    \"" << power_config_file_ << '"' << std::endl;
        std::cout << "  no-warn-stderr:      " << std::boolalpha << !sim_config_.warn_stderr << std::endl;
        std::cout << "  verbose-params:      " << std::boolalpha << sim_config_.verbose_cfg << std::endl;
        std::cout << "  debug-sim:           " << std::boolalpha << sim_config_.debug_sim << std::endl;
        std::cout << "  report-on-error:     " << std::boolalpha << sim_config_.report_on_error << std::endl;
        std::cout << std::endl;
        std::cout << "  show-tree:           " << std::boolalpha << show_tree_ << std::endl;
        std::cout << "  show-parameters:     " << std::boolalpha << show_parameters_ << std::endl;
        std::cout << "  show-ports:          " << std::boolalpha << show_ports_ << std::endl;
        std::cout << "  show-counters/stats: " << std::boolalpha << show_counters_ << std::endl;
        std::cout << "  show-clocks:         " << std::boolalpha << show_clocks_ << std::endl;
        std::cout << "  show-pevents:        " << std::boolalpha << show_pevents_ << std::endl;
        std::cout << "  show-notifications:  " << std::boolalpha << show_notifications_ << std::endl;
        std::cout << "  show-loggers:        " << std::boolalpha << show_loggers_ << std::endl;
        std::cout << "  no-colors:           " << std::boolalpha << disable_colors_ << std::endl;
        if(show_hidden_ == true){
            std::cout << " (show-hidden on)";
        }
        std::cout << std::endl;
        std::cout << "  show-dag:            " << std::boolalpha << sim_config_.show_dag << std::endl;
        std::cout << "  python-shell:        " << std::boolalpha << use_pyshell_;
        #ifndef SPARTA_PYTHON_SUPPORT
        std::cout << " (disabled at compile)";
        #endif
        std::cout << std::endl;

        // Print out parameters related to Pipeline Collection.
        bool collecting = false;
        if(sim_config_.pipeline_collection_file_prefix != NoPipelineCollectionStr){
            collecting = true;
        }

        //print out some stuff about the pipeline collections run status.
        std::cout << "  pipeline-collection: " << std::boolalpha << collecting << std::endl;
        if(collecting){
            std::cout << "  output dir:          " << sim_config_.pipeline_collection_file_prefix << std::endl;
            std::cout << "  pipeline heartbeat:  " << pipeline_heartbeat_ << std::endl;
        }
    }

    is_parsed_ = true;

    err_code = 0;
    return true;
}


void CommandLineSimulator::populateSimulation(Simulation* sim)
{
    std::cout << "\nSetting up Simulation Content..." << std::endl;
    if(!isParsed()){
        throw SpartaException("Cannot setup simulation before parsing command line");
    }

    try{
        populateSimulation_(sim);
    }catch(...){
        sim->dumpDebugContentIfAllowed(std::current_exception());
        throw;
    }
}

void CommandLineSimulator::populateSimulation_(Simulation* sim)
{
    if(!sim){
        throw SpartaException("Attempted to populate CommandLineSimulator with null simulator");
    }

    if(isSetup()){
        throw SpartaException("Cannot setup the simulation more than once");
    }

    // Convert heartbeat command line string to int
    uint32_t heartbeat;
    try{
        size_t end_pos;
        heartbeat = utils::smartLexicalCast<uint32_t>(pipeline_heartbeat_, end_pos);
    }catch (SpartaException const&){
        throw SpartaException("HEARTBEAT for pipeline collection must be an integer value and a multiple of 100 > 0, not \"")
            << pipeline_heartbeat_ << "\"";
    }

    if(heartbeat != 0 && heartbeat % 100 != 0){
        throw SpartaException("HEARTBEAT for pipeline collection must be a multiple of 100 > 0, not \"")
            << heartbeat << "\"";
    }

    // Pevent
    if(run_pevents_) {
        pevent_trigger_.reset(new sparta::trigger::PeventTrigger(sim->getRoot()));
    }

    for (const auto & def_file : report_descriptor_def_files_) {
        app::ReportDescVec descriptors;
        if (report_yaml_placeholder_replacements_.empty() &&
            report_specific_yaml_placeholder_replacements_.empty()) {
            //Read in the yaml file as-is
            descriptors = app::createDescriptorsFromFile(
                def_file, sim->getRoot());
        } else {
            auto iter = report_specific_yaml_placeholder_replacements_.find(def_file);
            if (iter != report_specific_yaml_placeholder_replacements_.end()) {
                if (!report_yaml_placeholder_replacements_.empty()) {
                  throw SpartaException("You cannot specify YAML replacements with:\n") <<
                      "    --report <desc.yaml> <replacements.yaml>\n"
                      "                      **AND**\n"
                      "    --report-yaml-replacements key1 val1 key2 val2...\n"
                      "At the same time. You must choose only one of the two syntaxes.";
                }
                descriptors = app::createDescriptorsFromFileWithPlaceholderReplacements(
                    def_file, sim->getRoot(), iter->second);
            } else {
                //Read in the yaml file, replace all the placeholders with
                //their respective value, and create report descriptors from
                //the final yaml contents after replacement
                descriptors = app::createDescriptorsFromFileWithPlaceholderReplacements(
                    def_file, sim->getRoot(), report_yaml_placeholder_replacements_);
            }
        }
        sim_config_.reports.insert(sim_config_.reports.end(), descriptors.begin(), descriptors.end());
    }

    sim_config_.copyTreeNodeExtensionsFromArchAndConfigPTrees();

    //The simdb feature is enabled by default unless it was explicitly
    //disabled at the command line. The reports will go to the pwd and
    //to the database at the same time, and at the end of simulation
    //the SimDB reports are exported to the filesystem, and the two
    //formatted report files are compared.
    //
    //This slows down simulation overall since we're capturing twice
    //the amount of report data / metadata, but this is to ensure the
    //new database is working for all scenarios while the database
    //backend gets some bake time. But we'll leave it here for a little
    //while so downstream SPARTA clients can revert to legacy reporting
    //infrastructure if they really need to.
    if (!feature_config_.isFeatureValueSet("simdb")) {
        feature_config_.setFeatureValue("simdb", 0);
    }
    sim->setFeatureConfig(&feature_config_);

    // Configure the simulator itself (not its content)
    sim->configure(argc_,
                   argv_,
                   &sim_config_,
                   use_pyshell_);

    // Show list of resources
    if(false == no_show_config_){
        std::cout << "Resources:" << std::endl;
        std::cout << "  " << sim->getResourceSet()->renderResources(false) << std::endl;
    }

    try{
        if(show_tree_){
            std::cout << "\nPre-processed UnboundParameterTree:" << std::endl;
            sim_config_.getUnboundParameterTree().recursPrint(std::cout);
        }

        // Construction phases. Typically, these are invoked by a startup script
        sim->buildTree();
        if(show_tree_){
            std::cout << "\nBuilt Tree:" << std::endl;
            std::cout << sim->getRoot()->renderSubtree(-1,
                                                       true,
                                                       false,
                                                       !show_hidden_);
        }

        sim->configureTree();
        if(show_tree_){
            std::cout << "\nConfigured Tree:" << std::endl;
            std::cout << sim->getRoot()->renderSubtree(-1,
                                                       true,
                                                       false,
                                                       !show_hidden_);
        }

        if(show_parameters_){
            auto filter_parameters = [](const TreeNode* n) -> bool {
                return dynamic_cast<const ParameterBase*>(n) != nullptr;
            };
            std::cout << "\nParameters (After Configuration):" << std::endl;
            std::cout << sim->getRoot()->renderSubtree(-1,
                                                       true,
                                                       false,
                                                       !show_hidden_,
                                                       filter_parameters);
        }

        // If we are reading a final config. Assert that we actually loaded a final config.
        std::vector<sparta::TreeNode*> children;
        sim->getMetaParamRoot()->findChildren("params.is_final_config", children);
        sparta_assert (children.size() > 0, "Sparta should have made a default meta.params.is_final_config.");
        sparta::Parameter<bool>* is_final_p = dynamic_cast<sparta::Parameter<bool>*>(children.at(0));
        sparta_assert(is_final_p);
        if (read_final_config_ != "")
        {
            bool val = is_final_p->getValue();
            if (val != true)
            {
                std::cerr << "Cannot load final config from \" " << read_final_config_ << "\"" << std::endl;
                std::cerr << "Final configs must have the meta.params.is_final_config = true" << std::endl;
                throw sparta::SpartaException("Invalid final config, meta.params.is_final_config equals FALSE");
            }
        }
        else
        {
            is_final_p->ignore();
        }

        sim->finalizeTree();

        // Store final config file(s) after finalization so that all dynamic parameters are built
        //! \todo Print configuration if finalizeTree fails with exception then rethrow
        if(final_config_file_ != ""){
            sparta::ConfigEmitter::YAML param_out_(final_config_file_,
                                                 false); // Hide descriptions
            param_out_.addParameters(sim->getRoot()->getSearchScope(), sim_config_.verbose_cfg);

        }

        // Store final config file(s) after finalization so that all dynamic parameters are built
        //! \todo Print configuration if finalizeTree fails with exception then rethrow
        if(power_config_file_ != ""){
            sparta::ConfigEmitter::YAML param_out_(power_config_file_,
                                                 false); // Hide descriptions

            //param_out_.addPowerParameters(sim->getRoot()->getSearchScope(), sim_config_.verbose_cfg);
            param_out_.addParameters(sim->getRoot()->getSearchScope(), sim_config_.verbose_cfg, true);
        }

        if(final_config_file_verbose_ != ""){
            sparta::ConfigEmitter::YAML param_out_(final_config_file_verbose_,
                                                 true); // Show descriptions
            param_out_.addParameters(sim->getRoot()->getSearchScope(), sim_config_.verbose_cfg);
        }

        if(sim_config_.pipeline_collection_file_prefix != NoPipelineCollectionStr)
        {
            pipeline_collection_triggerable_.reset(new PipelineTrigger(sim_config_.pipeline_collection_file_prefix,
                                                                       pipeline_enabled_node_names_,
                                                                       heartbeat,
                                                                       sim->getRootClock(),
                                                                       sim->getRoot()));

            // If pipeline collection is turned on begin writing an info file
            // about the simulation.
            info_out_.reset(new sparta::InformationWriter(sim_config_.pipeline_collection_file_prefix+"simulation.info"));
            info_out_->write("Pipeline Collection files generated from simulator ");
            info_out_->write(sim->getSimName());
            info_out_->write("\n\nSimulation started at: ");
            info_out_->writeLine(sparta::TimeManager::getTimeManager().getLocalTime());
        }

        // Finalize the pevent controller now that the tree is built.
        pevent_controller_.finalize(sim->getRoot());
        if(sim_config_.trigger_on_type == SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE)
        {
            // since we know the debug-on trigger is not used, just
            // invoke the pevent triggerable now.
            if(run_pevents_) {
                pevent_trigger_->go();
            }

            // Start pipeline collection now (if enabled).  This must
            // be enabled on the first cycle, not earlier to ensure
            // the tree is complete.
            if(pipeline_collection_triggerable_) {
                pipeline_trigger_.reset(new trigger::Trigger("turn_on_collection_now", sim->getRootClock()));
                pipeline_trigger_->addTriggeredObject(pipeline_collection_triggerable_.get());
                pipeline_trigger_->setTriggerStartAbsolute(sim->getRootClock(), 1);
            }
        }
        else if(run_pevents_ || pipeline_collection_triggerable_)
        {
            debug_trigger_.reset(new trigger::Trigger("debug_on_trigger", sim->getRootClock()));
            if(run_pevents_) {
                debug_trigger_->addTriggeredObject(pevent_trigger_.get());
            }

            // Pipeline trigger
            if(pipeline_collection_triggerable_) {
                debug_trigger_->addTriggeredObject(pipeline_collection_triggerable_.get());
            }

            // Enable the trigger
            switch(sim_config_.trigger_on_type)
            {
            case SimulationConfiguration::TriggerSource::TRIGGER_ON_CYCLE:
                {
                    // Assume the root clock
                    sparta::Clock * trigger_clk = sim->getRootClock();
                    if(!sim_config_.trigger_clock.empty()) {
                        // Find the given clock
                        std::vector<TreeNode*> results;
                        trigger_clk->findChildren(sim_config_.trigger_clock, results);
                        if(results.empty()) {
                            throw SpartaException("Cannot find clock '" + sim_config_.trigger_clock + "' for debug-on");
                        }
                        if(results.size() > 1) {
                            throw SpartaException("Found multiple clocks named '" + sim_config_.trigger_clock + "' for debug-on; please be more specific");
                        }
                        trigger_clk = dynamic_cast<sparta::Clock *>(results[0]);
                    }
                    debug_trigger_->setTriggerStartAbsolute(trigger_clk, sim_config_.trigger_on_value);
                }
                break;
            case SimulationConfiguration::TriggerSource::TRIGGER_ON_INSTRUCTION:
                debug_trigger_->setTriggerStartAbsolute(sim->findSemanticCounter(Simulation::CSEM_INSTRUCTIONS),
                                                        sim_config_.trigger_on_value);
                break;
            default:
                sparta_assert(!"Unknown tigger on type");
                break;
            }
        }

        if(show_tree_){
            std::cout << "\nFinalized Tree" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_);
        }

        // Finalize framework before run (e.g. scheduler)
        // This should not be needed once the code in here is moved into the Simulation class
        sim->finalizeFramework();

        if(run_time_cycles_ != sparta::Scheduler::INDEFINITE) {
            // Convert the run_time_cycles_ to ticks
            sparta::Clock * runtime_clk = sim->getRootClock();
            std::vector<TreeNode*> results;
            runtime_clk->findChildren(runtime_clock_, results);
            if(results.empty()) {
                throw SpartaException("Cannot find clock '" + runtime_clock_ + "' for debug-on");
            }
            if(results.size() > 1) {
                throw SpartaException("Found multiple clocks named '" + runtime_clock_ + "' for debug-on; please be more specific");
            }
            runtime_clk = dynamic_cast<sparta::Clock *>(results[0]);
            sparta_assert(runtime_clk != nullptr);
            run_time_ticks_ = runtime_clk->getTick(run_time_cycles_);
        }
        // Show ports
        if(show_ports_){
            auto filter_ports = [](const TreeNode* n) -> bool {
                return dynamic_cast<const Port*>(n) != nullptr;
            };
            std::cout << "\nPorts (After Finalization):" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_,
                                                                         filter_ports);
        }
        // Show Counters
        if(show_counters_){
            auto filter_counters = [](const TreeNode* n) -> bool {
                return (dynamic_cast<const Counter*>(n) != nullptr
                        || dynamic_cast<const ReadOnlyCounter*>(n) != nullptr
                        || dynamic_cast<const StatisticDef*>(n) != nullptr);
            };
            std::cout << "\nCounters (After Finalization):" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_,
                                                                         filter_counters);
        }
        // Show Clocks
        if(show_clocks_){
            auto filter_clocks = [](const TreeNode* n) -> bool {
                return (dynamic_cast<const Clock*>(n) != nullptr);
            };
            std::cout << "\nClocks (After Finalization):" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_,
                                                                         filter_clocks);
        }
        // Show pevents
        if (show_pevents_) {
            std::cout << "\nPevents (After Finalization): " << std::endl;
            pevent_controller_.printEventNames(std::cout, sim->getRoot());
        }
        // Show notifications
        if(show_notifications_){
            auto filter_notis = [](const TreeNode* n) -> bool {
                return (dynamic_cast<const NotificationSourceBase*>(n) != nullptr
                        && dynamic_cast<const log::MessageSource*>(n) == nullptr);
            };
            std::cout << "\nNotifications (After Finalization):" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_,
                                                                         filter_notis);
        }

        // Show loggers
        if(show_loggers_){
            auto filter_loggers = [](const TreeNode* n) -> bool {
                return dynamic_cast<const log::MessageSource*>(n) != nullptr;
            };
            std::cout << "\nLoggers (After Finalization):" << std::endl;
            std::cout << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                         true,
                                                                         false,
                                                                         !show_hidden_,
                                                                         filter_loggers);
        }

    // Catch SpartaExceptions because something went wrong with the tree or
    // parameters or ports. Allow plain old exceptions to propogate so they are
    // easier to debug?
    }catch(SpartaException& ex){
        std::cerr << SPARTA_CMDLINE_COLOR_ERROR "Error setting up simulator because of an exception:\n"
                  << ex.what()
                  << SPARTA_CMDLINE_COLOR_NORMAL;
        if(show_tree_){
            std::cerr << "Dumping device tree..."
                << std::endl
                << sim->getRoot()->getSearchScope()->renderSubtree(-1,
                                                                   true,
                                                                   false,
                                                                   !show_hidden_);
        }else{
            std::cerr << "\nTo display the device tree here, run with --show-tree" << std::endl;
        }

        std::cerr << "\n\n" SPARTA_CMDLINE_COLOR_ERROR "Rethrowing..." SPARTA_CMDLINE_COLOR_NORMAL << std::endl;
        // In interactive simulation, we would try and enter a "debug mode" and
        // allow user to wander without running anything.
        throw;
    }

    is_setup_ = true;
}

void CommandLineSimulator::runSimulator(Simulation* sim)
{
    std::cout << "Preparing to run..." << std::endl;

    try{
        runSimulator_(sim);
    }catch(...){
        sim->dumpDebugContentIfAllowed(std::current_exception());
        throw;
    }
}

void CommandLineSimulator::runSimulator_(Simulation* sim)
{
    if(!sim){
        throw SpartaException("Attempted to populate CommandLineSimulator with null simulator");
    }

    if(!isSetup()){
        throw SpartaException("Cannot attempt to run simulator before the CommandLineSimulator is "
                            "set up");
    }

    if(no_run_mode_){
        // Exit if no-run mode
        std::cout << "User specified --no-run or another command with \"no-run\" semantics. "
                     "Skipping run" << std::endl;
        return;
    }

    if(sim_config_.debug_sim){
        // Logging destinations used
        std::cout << "\nLogging destinations used:" << std::endl;
        sparta::log::DestinationManager::dumpDestinations(std::cout, true);

        // Tree-type makeup
        std::cout << "\nTree Type Mix:" << std::endl;
        sim->getRoot()->dumpTypeMix(std::cout);
    }

    if(run_time_ticks_ > 0 || use_pyshell_){
        try{
            sim->run(run_time_ticks_);
        }catch(...){
            if(pipeline_collection_triggerable_) {
                pipeline_collection_triggerable_->stop();
                info_out_->write("Simulation aborted at: ");
                info_out_->writeLine(sparta::TimeManager::getTimeManager().getLocalTime());
            }

            // In interactive simulation, we would try and enter a "debug mode" and
            // allow user to wander without running anything.
            throw;
        }
    } // if(run_time_ > 0)

    if(pipeline_collection_triggerable_)
    {
        pipeline_collection_triggerable_->stop();

         // Write the end time of the simulation.
        info_out_->write("Simulation ended at: ");
        info_out_->writeLine(sparta::TimeManager::getTimeManager().getLocalTime());
        sparta::InformationWriter& outputter = *(info_out_.get());
        outputter << "Heartbeat interval: " << pipeline_heartbeat_ << " ticks" << "\n";
    }

    if(show_tree_){
        std::cout << "\nTree After Running" << std::endl
                  << sim->getRoot()->renderSubtree(-1, true, false, !show_hidden_)
                  << std::endl;
    }
}

void CommandLineSimulator::postProcess(Simulation* sim)
{
    try{
        postProcess_(sim);
    }catch(...){
        sim->dumpDebugContentIfAllowed(std::current_exception());
        throw;
    }
}

void CommandLineSimulator::postProcess_(Simulation* sim)
{
    auto simdb = GET_DB_FOR_COMPONENT(Stats, sim);

    if (simdb) {
        auto feature_cfg = sim->getFeatureConfiguration();
        if (IsFeatureValueEnabled(feature_cfg, "simdb-verify")) {
            std::string simdb_fname = simdb->getDatabaseFile();
            const std::string simdb_src_fname = simdb_fname;
            simdb_fname = bfs::path(simdb_fname).filename().string();

            bfs::path cwd = bfs::current_path();
            const std::string simdb_dest_dir =
                cwd.string() + "/" + db::ReportVerifier::getVerifResultsDir();

            const std::string simdb_dest_fname = simdb_dest_dir + "/" + simdb_fname;
            boost::system::error_code err;
            bfs::copy_file(simdb_src_fname, simdb_dest_fname, err);
            if (err) {
                std::cout << "  [simdb] Warning: The 'simdb-verify' post processing step "
                          << "encountered and trapped a boost::filesystem error: \""
                          << err.message() << "\"" << std::endl;
            }
        }
    }

    sim->postProcessingLastCall();
}

void CommandLineSimulator::printUsageHelp_() const
{
    std::cout << std::endl << usage_ << std::endl;
}

void CommandLineSimulator::printOptionsHelp_(uint32_t level) const
{
    std::cout << sparta_opts_.getOptionsLevelUpTo(level) << std::endl
              << param_opts_.getOptionsLevelUpTo(level) << std::endl
              << run_time_opts_.getOptionsLevelUpTo(level) << std::endl
              << log_opts_.getOptionsLevelUpTo(level) << std::endl
              << pipeout_opts_.getOptionsLevelUpTo(level) << std::endl
              << debug_opts_.getOptionsLevelUpTo(level) << std::endl
              << report_opts_.getOptionsLevelUpTo(level) << std::endl
              << simdb_opts_.getOptionsLevelUpTo(level) << std::endl
              << app_opts_.getOptionsLevelUpTo(level) << std::endl;

    if(0 == level){
        std::cout << advanced_opts_.getOptionsLevelUpTo(level) << std::endl;
    }
}

void CommandLineSimulator::showVerboseHelp_() const
{
    printUsageHelp_();
    printOptionsHelp_(MultiDetailOptions::VERBOSE);
    showConfigHelp();
    showLoggingHelp();
    showReportsHelp();
    std::cout << "\nTips:\n  \"--help-topic topics\" will display specific help sections for more concise help" << std::endl;
}

void CommandLineSimulator::showBriefHelp_() const
{
    printUsageHelp_();
    printOptionsHelp_(MultiDetailOptions::BRIEF);
}

void CommandLineSimulator::showHelpTopics_() const
{
    std::cout << "All --help-topic topics:\n"
              << "  topics     Show this message\n"
              << "  all        Show general verbose help (--help)\n"
              << "  brief      Show general brief help (--help-brief) \n"
              << "  parameters Show help on simulator configuration\n"
              << "  logging    Show help on logging\n"
              << "  reporting  Show help on creating reports\n"
              << "  pipeout    Show help on pipeline collection\n"
              << std::endl;
}

bool CommandLineSimulator::openALFAndFindPipelineNodes_(const std::string & alf_filename)
{
    const bool all_good = true;
    std::ifstream alf_file(alf_filename);
    if(!alf_file) {
        // not good
        return !all_good;
    }

    //
    // The format for the ALF is pretty simple.  It's key/value pairs
    // on a single line.  What we're looking for is of this pattern:
    //     LocationString: top.core0.blah.blee
    //
    std::string a_line;
    std::set<std::string> nodes_to_limit;
    while(std::getline(alf_file, a_line)) {
        std::vector<std::string> the_parts;
        utils::tokenize_on_whitespace(a_line, the_parts);
        if(the_parts.empty()) {
            continue;
        }
        if(the_parts[0] == "LocationString:")
        {
            std::string node = the_parts[1];
            std::vector<std::string> node_parts;
            utils::tokenize(node, node_parts, ".");

            //
            // What we're looking for is this pattern:
            //   top.core0.alu0.scheduler_queue.scheduler_queue0
            //   top.core0.alu0.scheduler_queue.scheduler_queue1 ...
            // and truncate it to 'top.core0.alu0.scheduler_queue'.
            //
            // This grabs more than what the ALF might use, but it's a
            // little cleaner
            //
            if(node_parts.size() > 2) {
                const uint32_t last_node = node_parts.size() - 1;
                const uint32_t second_to_last_node = last_node - 1;
                if(node_parts[last_node].find(node_parts[second_to_last_node]) != std::string::npos) {
                    node_parts.pop_back();
                }
            }

            std::stringstream new_node_name_stream;
            std::copy(node_parts.begin(), node_parts.end(),
                      std::ostream_iterator<std::string>(new_node_name_stream, "."));

            // Remove the last '.'
            std::string new_node_name = new_node_name_stream.str();
            new_node_name.pop_back();
            pipeline_enabled_node_names_.insert(new_node_name);
        }
    }
    return all_good;
}

    } // namespace app
} // namespace sparta
