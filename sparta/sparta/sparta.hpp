// <sparta.hpp> -*- C++ -*-


#ifndef __SPARTA_H__
#define __SPARTA_H__

/*!
 * \file sparta.hpp
 * \brief This is _not_ a global include file.  Used for documentation and const globals.
 */

/*!
 * \brief Sparta namespace containing most Sparta classes
 */
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/statistics/StatisticSet.hpp"

/*!
 * \brief All symbols required that must be defined for a Sparta application.
 * \deprecated SPARTA_SYMBOLS no longer has a use. All sparta symbols are
 * instantiated in sparta.cpp
 */
#define SPARTA_SYMBOLS

// __SPARTA_H__
#endif


/*!
\mainpage Sparta

The framework is documented from several perspectives.<br>
<h3>Online Documentation</h3>
-# \ref modeling <br>
Model development using sparta (include Getting Started)<br>&nbsp;
-# \ref end_user <br>
Simulator End-User Documentation for Sparta CLI Simulators<br>&nbsp;
-# \ref application <br>
Simulator front-end integration with sparta<br>&nbsp;
-# \ref client_apis <br>
Interfaces for inspecting a Sparta simulator programatically<br>&nbsp;
-# \ref formats <br>
File formats used by Sparta<br>&nbsp;
-# \ref tools <br>
Sparta-Related Tools<br>&nbsp;
-# \ref framework_dev <br>
Documentation for Sparta Developers<br>&nbsp;
-# \ref best_practices <br>
Suggested best practices for using Sparta<br>&nbsp;
-# \ref q_and_a <br>
Questions and Answers (not necessarily frequently asked)

*/

/*!
\page tools Analysis Tools
 */

/*!
\page end_user Sparta Command Line Interface End-User Guide
\tableofcontents
<h4><i>For end-users of the Sparta simulator CLI</i></h4>

This page details the usage, configuration, inputs, and outputs of <b>"sparta-based CLI(command line
interface)"</b>. This term is used to refer to a simulator that uses the sparta::app framework
(sparta::app::CommandLineSimulator and sparta::app::Simulation) to initialize and configure the
simulator from the command line. If a simulator application is not using this part of the Sparta
framework, very little of this end-user guide is applicable to an application. Some of these
features will still be available interally to the simulation framework, but they may be exposed to
the end-user in a different manner.

The terms <b>"sparta simulator"</b> and <b>"sparta-instrumented simulator"</b> differ in that they refer
only to simulators that expose a sparta tree with instrumentation such as counter, statistics, and
notifications. Such simulators may or may not be driven by a sparta CLI.

For the purpose of this page, 'user' means an invidual or script who invokes a simulator through its
command line interface or needs to work with Sparta simulator output

---
\section invocation 1 Simulator Invocation
The Sparta command line consists of a number of generic options built into the sparta application
framework as well as application-specific commands that pertain to a specific simulator.

In general, the Sparta application framework attempts to provide a large set of generic commands
without making any assumptions about the underlying device being simulated. The only assumptions
made are that the device operates on one or more clock domains with regular frequencies and that
there is some 'tick' (sparta::Scheduler::getCurrentTick) unit which can be used as a unit of absolute
time in which inputs and outputs can be expressed. The tick is typically 1 picosecond, but may also
be the least-common multiple of all these clock periods. All clock periods will be integer multiples
of the tick period (in terms of simulated time)

\note In the future, time-based commands will be specified in terms of a specific clock
domain.

\subsection io_policies 1.1 I/O Policies

\par A. No Hidden I/O
As policy, the Sparta application framework will not read any input files that are not explicitly
specified on the command line or indirectly by configuration files specified on
the command line. The Sparta application framework generally does not write any output files unless
explicitly requested. If any files are automatically written by Sparta, those filenames will always be
configurable and disable-able through the command line. For a given simulator, the Sparta application
framework's behavior will dependent only on the given command line. There should be no unexpected
effects from seemingly unrelated files or environment variables.

\par
<b>NOTE:</b> The only cases of Sparta writing files which were not requested are debug dumps. These
files will be written if the simulator exits with an error (and the --debug-dump policy
option allows it). The name of this file is typically.
~~~~
error-dump.dbg
~~~~
The \--debug-dump-filename option controls this filename.

Similarly, Sparta will eventually write snapshot pipeout files on error. The pipeout file prefix will
be configurable

\par B. Full Output Control
The user should never be required to guess output filenames. All output files are configurable on
the command line or through parameters in configuration files that are specified on the command
line. The user may not have full control

\par C. Output Error Detection
All output files opened from within Sparta are expected to detect file write errors and throw
exceptions on failed writes (e.g. when a disk quota is reached). Similarly, failed heap allocations
are expected to throw exceptions, though some objects which suppress these exceptions (e.g.
stringstream) may cause such errors to go undetected in the short term.

Eventually, the simulation may be able to suspend itself from within a failed memory allocation or
bad file write.

\par
<b style='font-color:$ff0000;'>WARNING:</b> Specific simulator applications may violate these policies, but are strongly
encouraged not to.

\subsection sparta_cmds 1.2 Sparta Basic Command-Line Options

The most useful of all commands are the help commands. Even if this document is out of date, full
(albiet abridged) documentation will be available through the --help-verbose command line flag. The
-h flag shows a limited set of the most common options with very brief descriptions. The get
detailed help on all commands, use
~~~~
simulator --help | less
~~~~
or
~~~~
simulator --help-topic topics
~~~~

A number of other built-in commands are listed in later sections.

\par
<b>NOTE:</b> In the future, a man-page may be created for the Sparta application framework. A
pagination system could be built into the Sparta application framework to make browing the built-in
documentation even easier.

\par
<b style='font-color:$ff0000;'>WARNING:</b> Some sparta command line options have variable parameters
such as \--report. The final
optional argument, FORMAT, is a string describing a format. If this option immediately preceeds a
positional argument (e.g. trace file name) and the user did not specify a FORMAT argument, then
the sparta cli will try and consume that positional argument as a FORMAT. If it is a recognized as a
valid value of FORMAT, then sparta will interpret it as a format, If not, it will be interpreted as a
positional argument. The opposite problem can also occur, where a FORMAT argument is misspelled,
causing the cli to interpret it as a positional argument. To avoid this problem, one can ensure that
variadic command line options such as \--report are not the last named option on the command-line.
The be even more explicit, the \-- token can be set to indicate the termination of a command-line
option argument list.
For example, if a positional argument named html (which is also a valid value for FORMAT) is
needed on the command line but you don't actually want to specify a FORMAT, use:
~~~~
simulator --report top myreport.yaml report.txt -- html
~~~~
This would end up being equivalent to
~~~~
simulator --report top myreport.yaml report.txt txt html
~~~~
Here, a report defined by myreport.yaml is written to report.txt with plaintext formatting. An
application-specific positional argument named html is also consumed by the whatever simulator
application is being run. The sparta CLI does not care about 'html' in this command line.
<br/>
It would probably be a mistake to use the command line:
~~~~
# Poor choice of filename or format
simulator --report top myreport.yaml report.txt html
~~~~
The result of this would be saving a report to report.txt as html markup instead of plaintext.
See \ref report_gen for more details on report generation

\subsection app_cmds 1.3 Application-Specific Commands
Simulator command-lines can have any number of application-specific commands. Refer to that
simulator's documentation for details.

Examples of some typical simulator-specific commands are instruction-count limits, version-printing,
showing additional detailed help pages, and specifying trace files. Positional arguments are <b>always</b> application-specific.

Extending the sparta CLI to add application-specific events is straightforward and requires boost program_options.

\subsection sparta_advanced_cmds 1.4 Sparta Advanced commands

\todo Write this section

\subsection sim_dbg_cmds 1.5 Sparta Simulation Debug commands

The sparta CLI provides a few options that help debug the CLI and the Sparta simulation framework.

Usage             | Behavior
------            | -------------
\--debug-sim      | Turn on simulator framework debugging
\--show-options   | Show all options parsed from the command line
\--verbose-config | Sets all configuration file readers and emitters to verbose mode for easier debugging

---
\section ctrl_cfg 2 Control and Configuration

\subsection ctrl_cfg_parameters 2.1 Parameters

Sparta simulations are configured using parameters, which can be specified on the command line
individually or using YAML configuration files.

\code
$simulator -p top.core0.params.foo value
$simulator -c my_conf.yaml
$simulator -n top.core0 my_core_conf.yaml
\endcode
<br/>

Usage                 | Alternate           | Behavior
------                | ---------           | -------------
\-p  PATTERN VAL      | \--parameter        | Specify an individual parameter value. Multiple parameters can be identified using '*' and '?' glob-like wildcards. Example: "\--parameter top.core0.params.foo value"
\-c  FILENAME         | \--config-file      | Specify a YAML config file to load at the top of the simulator device tree. Example: "--config-file config.yaml" This is effectively the same as \--node-config-file top params.yaml
\-n  PATTERN FILENAME | \--node-config-file | Specify a YAML config file to load at aspecific node (or nodes using '*' and '?' glob-like wildcards) in the device tree. Example: "\--node-config-file top.core0 core0_params.yaml"

Use of the \-p option is straightforward. Using \-c and \-n require YAML-based sparta parameter
configuration files, whose format is described in detail in \ref param_format .

\par 2.1.1 Listing Parameters
</b>Most</b> of the available parameters in the simulation can be seen by using
\code
$simulator --write-final-config FILENAME
\endcode
to write the simulator's full configuration file to a file immediately after the simulation is fully
constructed (implying no more changes to configuration) but before it begins running. The output of
this feature can be used as an input configuration file.
This is currently the recommended way of enumerating the available parameters and generating
configuration-file templates.
<br/>
To generate a configuration file with some helpful documentation as in-line comments:
\code
$simulator --write-final-config-verbose FILENAME
\endcode
<br/>
In both these cases, the output written to ''FILENAME'' can be taken as-is or modified and then used
as an input file to a \-c or \-n command-line argument (see above).
<br/>
This feature is used in several ways.
\li To ensure that user-specified parameter value are actually affecting the final configuration
\li Listing <b>most</b> available parameters
\li Reproducing a prior run based on its configuration

\par 2.1.2 Virtual (Unbound) Parameters
Some parameters may not be exposed by simulator in the final-config-file or dumps of the device tree
(see \--show-tree). In certain cases, a simulator may need to use parameters (for determining
topology) which never actually exist as sparta::Parameter in the simulation. These are referred to as
virtual or unbound parameters.<br/><br/>
<div style='background-color:#ffc0c0; padding:4px; border:2px dashed #e06060;'>Only simulation
parameters that exist as sparta::Parameter
nodes will be written as part of the final configurations. Today, virtual parameters that are not
part of the concrete device tree finalized before running will not be written to a final-config
file. Simulator-specific documentation should thoroughly describe any parameter-space not covered by
the device tree</div><br/>
A SpartaException will be thrown at the end of tree finalization if any virtual parameters remain
unread. This ensures that all user parameters are consumed by the simulator in some way.

\subsection ctrl_cfg_architecture 2.2 Selecting Architectures
Sparta configuration supports the` concept of architecture configuration baselines. This allows users
to load configuration files that override the defaults of chosen parameters hard-coded in the
simulator source code. Unlike typical configuration files or command line parameter specifications,
selecting an architecture updates both the default and the value of any specified parameter such that
it will show up as having a default value the final configuration of the simulator
(--write-final-config) is inspected.


Usage                  | Function
------                 | ---------
\--arch ARCH           | Searches in \--arch-search-dir for a configuration file matching the given name "name" or "name.yaml" or "name.yml" or "name/name.yaml" or "name/name.yml"
\--arch-search-dir DIR | Absolute path or relative path (to cwd) dictating where the simulator should look for \--arch names to resolve them to actual configuration files


The default values for both of these options are simulator specific. The default architecture search
dir is listed in the \--arch-search-dir command help string.

Afer resolving an \--arch name to a config file, that configuration file is listed in the simulator
output during simulator setup to show exactly what configuraration files were applied to what parts
of the simulated tree and whether they were applyed as architectural baseline configuration or normal
configuration.


\subsection numeric_constants 2.3 Numeric Constants
Lexical casting of numeric literals in sparta is smart. Values being assigned to integer parameters
through command line options or configuration files in the simulator can use prefixes to specify
radix and suffixes to specify multipliers.

For example, 10b will be interpreted as 10000000000 (10 billion)

\code
$simulator -p top.core0.params.numeric_parameters 10b
\endcode

Note that this can be done only on parameters which EXPECT AN INTEGER. This includes any
command-line options or configuration files dealing with [u]intXX_t-typed parameters. Only parts of
the simulation which expect an integer will use this smart parsing mechanism. This cannot yet be
used in statistic expressions because these expressions operate on doubles at all times.

The full set of features includes
-# Suffixes
  -# SI decimal (power of 10)
    -# k/m/g/t/p/
  -# ISO/IEC 8000 (power of 2):
    -# ki/mi/gi/bi/ti/pi
  -# Case insensitive
-# Fractional values (if followed by a large enough suffix)
  -# e.g. "0.5b" => 500m
  -# Fractional value always parsed as decimal
  -# As long as (fraction * suffix) yields a whole number, anything is allowed
    -# OK:  5.123k => 5123
    -# ERROR:  5.1234k => 5123.4
-# Radix Prefixes
  -# 0xN..., 0N...
  -# 0bN... now supported for binary
  -# Case insensitive
-# Separators in “, _\\t\\n” are ignored (note that there is a space in this list)
  -# e.g. "5,000 000" => 5 million
  -# Not ignored between 2-character prefixes and suffixes
  -# If the number includes spaces and is entered on the command line, ensure that it is handled as a single token by adding quotes
-# Numbers can be strung together much like they are spoken
  -# e.g. "10b500k" => 10 billion, five hundred thousand => 10,000,500,000
  -# Each value encountered is simply added together, so you could do these out of order
  -# Any values after the first cannot have prefixes
  -# Any values after the first are always parsed as decimal
  -# Any negative sign must be at the beginning of the string, affecting the entire number.
    -# This is not an expression, it’s a literal.

These suffixes (case insensitive) can be added have the following meanings

Suffix  | Multiplier
------- | --------
K       | 10^3
M       | 10^6
G       | 10^9
B       | 10^9
T       | 10^12
P       | 10^15
Ki      | 2^10
Mi      | 2^20
Gi      | 2^30
Bi      | 2^30
Ti      | 2^40
Pi      | 2^50

Additional Notes:
-# For numeric constants with hex prefix, ‘b’ is treated as a digit, not a suffix. Use ‘g’ instead
-# Fractional values after a decimal point are always parsed in decimal, regardless of prefix on number left of decimal
  -# 0x1.1k is parsed as 0x1 + (decimal 0.1 * 1000)
-# Negative numbers are still supported
-# Added better detection of overflowing values
  -# Parameter types of uint32_t, for example, will error if they encounter larger values than MAX_UINT32


\todo Write this section
  -# Configuration
  -# Traces
  -# Run Control
  -# Notifications
  -# Inspecting configuration
    -# Showing the tree
    -# Writing configuration

---
\section output 3 Simulator Output
The sparta CLI supports a number of output mechanisms for any sparta-instrumented simulator.

\subsection auto_summary 3.1 Automatic Summary

After a successful run, an automatic summary of all known counters and statistics in the simulation
device tree will be written to stdout.

\par
<b>NOTE:</b> This is the most obvious output of the simulation, but is by no means
the totality of a sparta simulation's output capability.

By default, this looks something like:
\verbatim
  top
    top.foo
      top.foo.bar
          stat_x                                             = 0
          stat_y                                             = 12324
          stat_Z                                             = 3.2491
      top.foo.biz
          stat_a                                             = 67
\endverbatim

If configured to be verbose, the automatic summary looks something like:
\verbatim
  top
    top.foo
      top.foo.bar
          stat_x                                             = 0       # Number of x's that happend while doing
                                                                       # q while in state r or s but not t
          stat_y                                             = 12324   # Time foo.bar did y
          stat_Z                                             = 3.2491  # Value of z. Some of these comments can
                                                                       # get really long and may wrap multiple
                                                                       # times beacuse someone made them so very
                                                                       # very long.
      top.foo.biz
          stat_a                                             = 67      # Short desciption
\endverbatim

This behavior can be controlled using the --auto-summary command line option. Valid usages are:
Usage                   | Behavior
------                  | -------------
\--auto-summary off     | Do not write summary
\--auto-summary on      | Write summary to stdout
\--auto-summary normal  | Write summary to stdout (same as on)
\--auto-summary verbose | Write verbose summary to stdout including descriptions

\par
<b>NOTE:</b> If you want the automatic summary sent to a file instead of stdout, use to the
\--report-all option, which Sparta's automatic summary uses internally. The automatic summary can be
disabled with \--auto-summary=off

---

\subsection report_gen 3.2 Report Generation

\li \ref report_def_format

The Sparta Report system is capable of collecting counters and statistics from the simulation device
tree and printing their names and values to an output file or stream in any of a variety of formats.
This is the principal means of extracting quantitative data from a simulation.

\par
<b>NOTE:</b>The automatic summary (\ref auto_summary) generated by default uses this same mechanism
internally (though it is not subject to some report-configuration options [e.g. \--report-updat-ns]
that user-defined reports are).

\par 3.2.1 Counters and Statistics
A sparta simulation tree will contain two types of objects which can be part of a reports.
-# Counters are large interger values, usually monotonically increasing (e.g. number of instructions
retired) They are internally represented as uint64_t.
-# Stiatistcs are expressions refering to counters or other statistics. The simulator has many
statistics built in (e.g. average instructions per cycle) which are useful. Users can also define
custom reports which contain arbitrary statistics. Since statistics only depend on publicly visible
counters, their values could always be computed in post-processing.

\par
Both counters and statistics objects are always found within a "stats" object in the Sparta device
tree.

\par
<b>NOTE:</b> In future versions, reports will be able to contain numeric parameter values as content
and statistical expressions will be able to use numeric parameter values (and possibly elements and
attributes of contaners) as variables in the expression.

\par 3.2.2 Report Creation

Other than the automatic summary, all reports must be explicitly created on the command line. The
\--report and \--report-all options configure a new report or reports.
Usage                               | Behavior
------                              | -------------
\--report PATTERN DEF OUT [FMT]     | Create one or more reports based on the report definition file DEF at all nodes matching PATTERN and write the end-of-simulation result to OUT using the optionally-specified format FMT. If no format is given, infers it from the file extension. Use \--help for more details about this command. See both the \--report command details and the "Reports" section of the help output. <em>DEF</em> may be specified as "@" (no quotes necessary) to direct the simulator to autopopulate the report instead of using a definition file.
\--report-all OUT [FMT]             | Create one or more reports containing all counters and statistics in the simulation and write the end-of-simulation result to OUT using the optionally-specified format FMT. If no format is given, infers it from the file extension. Use \--help for more details about this command. See both the \--report-all command details and the "Reports" section of the help output

Report definition files are a restricted subset of the YAML files with special semantics for
YAML dictionaries based on context in the file. See \ref report_def_format

Often, this is used to place a simple report on the top-level node in the simulation tree
\verbatim
simulation --report top myreport.yaml out.txt
\endverbatim

Node paths in myreport.yaml for the above example would be specified relative to "top".

Often, a global scope is desired so that Sparta scheduler statistics can be used (e.g. ticks) or just
to allow fully qualified paths. This can be done using the "_global" keyword.

\verbatim
simulation --report _global myreport.yaml out.txt
\endverbatim

Node paths in this report definition would be fully qualified and begin with "top." or "scheduler."

Each report directive can created multiple reports if the PATTERN contains multiple wildcards.
For example:
\verbatim
simulation --report top.nodeX.* @ out%i.csv csv
\endverbatim

It is generally a bad idea to direct mutliple reports to the same output file as the result is
undefined and the files could be overwritten. If using a wildcard in the <em>PATTERN</em> variable
in the \--report command, it is usually necessary to use either of the \%i (index) or \%l (location)
variables in the output file name. The following variables are supported (From
\ref sparta::app::computeOutputFilename).

Wildcard   | Value
--------   | -----
\%l        | location (lower case L)
\%i        | index of substitution for wild-card in <em>PATTERN</em> (0-based). Based on construction order of found nodes
\%p        | process ID
\%t        | timestamp
\%s        | simulator name

When using a variable in the destination, Sparta will list the instantiations both at the start of
simulation and at the end.
\verbatim
Running...
  Placing report on node top.nodeX.nodeY for: Report "@" applied at "top.nodeX.*" -> "out0.csv" (format=csv)
  Placing report on node top.nodeX.nodeZ for: Report "@" applied at "top.nodeX.*" -> "out1.csv" (format=csv)

... later ...

  [out] Wrote Final Report Report "@" applied at "top.nodeX.*" -> "out%i.csv" (format=csv) (updated 13 times):
    Report instantiated at top.nodeX.nodeY, updated to "out0.csv"
    Report instantiated at top.nodeX.nodeZ, updated to "out1.csv"
  2 reports written
\endverbatim

\par 3.2.3 Report Periodicity & Warmup

There are several modifiers to the behavior of the reports created. <b><em>These will eventually be
deprecated and replaced with a more robust and flexible control system that can apply to individual
reports</em></b>

Usage                                      | Behavior
------                                     | -------------
\--report-warmup-count INSTRUCTIONS        | Does not begin any report (including builtin reports such as the automatic summary) until <em>INSTRUCTIONS</em> instrutions have elapsed based on whatever counter the simulator has identified as having the 'instruction count' semantic. See sparta::app::Simulation::CounterSemantic
\--report-update-ns NANOSECONDS            | Periodically update all reports every <em>NANOSECONDS</em> written with formatters that support updating (see \--help-topic reporting for information). CSV supports this at the least. This does <b>not</b> affect the automatic summary report. Exclusive to other \--report-update-* options.
\--report-update-cycles [CLOCK] CYCLES     | Periodically update all reports every <em>CYCLES</em> cycles on the clock named <em>CLOCK</em> (optional) written with formatters that support updating (see \--help-topic reporting for information). CSV supports this at the least. This does <b>not</b> affect the automatic summary report. Exclusive to other \--report-update-* options.
\--report-update-counter COUNTER COUNT     | Periodically update all reports every <em>COUNT</em> units for a counter located in the tree at path <em>COUNTER</em> (e.g. top.core0.foo.stats.bar) written with formatters that support updating (see \--help-topic reporting for information). This option guarantees one update to each applicable report for each multiple of <em>COUNT</em> reached by the counter, even if the counter is incremented as a coarse granularity such that it skips multiple instances of that target count period in a single cycle. The extra updates will show 0-deltas for all counters. CSV supports this at the least. This does <b>not</b> affect the automatic summary report. Exclusive to other \--report-update-* options.

When using repeating reports, be sure that the report formatter actually supports updating. Some
formatters do not.

When the simulator writes its final reports, it will also indicate how many times each
user-specified report has been updated. For example:
\verbatim
simulation <other arguments> --report-update-ns 1000 --report top.nodeX.nodeY @ out.csv csv
\endverbatim
May generate:
\verbatim
  [out] Wrote Final Report Report "@" applied at "top.nodeX.nodeY" -> "out.csv" (format=csv) (updated 14 times):
\endverbatim

To periodically report based on a counter value such as intruction retierd, the following could be
used in a simulator with the appropriate counter.
\verbatim
simulation <other arguments> --report-update-counter top.core0.retire.stats.num_insts_retired 1000 --report top.nodeX.nodeY @ out.csv csv
\endverbatim

When writing a report that is periodically updated, it is useful to create a report definition file
that includes a clock cycle counter as the first item in the report definition. Then, the report
output will include that clock's value in the first column (in the case of CSV). This looks like:
\code
# Report definition with a cycle counter as the first stat
content:
    top.core0:
            "cycles" : "core0 cycles"
    # Additional stats & subreports
\endcode

After generating a periodic report in the CSV format, try plotting with the Sparta csv report plotter
in sparta/tools/plot_csv_report.py

\todo Complete this section

\par 3.2.4 Report Output Formatters

The list of available report output formats are available at \ref report_out_format . Refer to this
page for notes and details. Use <b><pre>"--help-topic reporting"</pre></b> to get information
about report formatsinteractively from a Sparta simulator

\par 3.2.5 Parsing and Extension

\todo Complete this section
    -# Parsing
    -# Formats/extending

\subsection msg_logging 3.3 Message Logging
For more details about the modeling side of logging, see \ref logging

"Logging" in sparta refers to a plaintext logging system for informational and diagnostic messages.
Sparta includes a mechanism for generating textual messages that can be configurably directed to
various output files in variou formats to generate a textual trace of the state or events inside
particular components of a simulation.


Usage                     | Behavior
------                    | ------
\--warn-file FILENAME     | Specifies which file to which warnings should be directed (independent of \--no-warn-stderr)
\--no-warn-stderr         | If set, prevents logging messages of the "error" category to the stderr stream.(independent of \--warn-file)

\par 3.3.1 Control

Command                                    | Functionality
-----------                                | ------
\-l / \--log PATTERN CATEGORY DESTINATION  | Creates a logging "tap" on the node(s) described by PATTERN. These taps observe log messages emitted at or below these nodes in the Sparta tree when the messages' categories match CATEGORY. If CATEGORY is "", all message categories match. ALl log output received through this tap is routed to DESTINATION, which is formatted based on the file extension. See the <b>Logging Formats</b> below.

\par 3.3.2 Logging Formats

The <em>DESTINTION</em> field of the \--log option directs the log messages from that log tap to a
specific destination. These destinations are formatted based on their file extension (for now).
Using 1 or 2 as a destination file directs the log output to stdout or stderr respectively.

Format          | File Extension   | Description
------          | ------           | ------
Basic (stdout)  | 1                | Contains message origin, category, and content
Basic (stderr)  | 2                | Contains message origin, category, and content
Basic (file)    | *.log.basic      | Contains message origin, category, and content
Verbose (file)  | *.log.verbose    | Contains all message meta-data
Raw (file)      | *.log.raw        | Contains no message meta-data
Default (file)  | (any other )     | Contains most message meta-data excluding thread and message sequence number

Except <b>raw</b> output, each logger output places its content on a single line, beginning with a
an opening '{', followed by some fileds describing the log messages, usualy including a timestamp,
origin, and category, followed by a closing '}' and then the log message itself. This generally
makes these log messages easily parsable.

All current logging formats can be seen near the end of the help text generated by \--help or by
the \--help-logging command.

\par 3.3.3 Parsing Output
\todo Write this section

\subsection notification_logging 3.4 Notification Logging

\par
This feature is not yet implemented

\subsection perf_events 3.5 Performance Events

\par
This feature is not yet implemented

\subsection pipeline_collection 3.6 Pipeline Collection

Pipeline collection captures a per-cycle trace of 'transactions' flowing through specially
instrumented stations (e.g. buffer elements, queues, etc.) throughout the simulator when enabled.
This data can be visualized in the Argos (\ref argos) viewer with customizable layouts to display
and navigate pipeline snapshots and time-based pipeline crawls.

This data is written to a set of files having a common, user-specified prefix. Support for
collection requires participation on the part of each model.

These files include a clock listing, a map of device tree locations to indices, a transaction data
binary, a time-index, and a simulation info file

\todo Write this section

See also \ref pipeout_format

\par 3.6.1 Collection Control

Often, pipeline collection introduces too much performance and disk-space overhead to leave on for a
multi-million cycle simulation. It becomes necessary to selectively enable collection after a
certain amount of progress has been made in the simulation. Pipeline collection (and log taps) can
be controlled with the \--debug-on family of options.
Usage                               | Behavior
------                              | -------------
\--debug-on [clock] CYCLE           | Defers pipeline collection and user-specified logging until cyclye=CYCLE on optional clock=path.to.clock.name
\--debug-on-icount ICOUNT           | Defers pipeline collection and user-specified logging until the instruction count has reached ICOUNT. Each simualtor defines its own instruction counter through app::Simulation::findSemanticCounter

\par
<b>WARNING:</b> This also currently controls all user-specified logging taps (-l,\--log) as well.

\par
<b>WARNING:</b> This command will soon be removed and replaced with separate, fine-grained controls
for the time period if pipeline collection, reports, and logging taps. This new control will also
support triggering based on counters

\par
<b>NOTE:</b> The simulation may also generate a 1-tick instantaneous pipeline file if an exception
occurs while running. This may or may not be the same file specified on the command line with -z.
If a pipeline dump is created, the debug dump (\ref debug_dump) will contain the name of the
pipeline file.

\subsection debug_dump 3.7 Post-Run Debug dumps
When the Sparta application framework encounters an exception during running or post-run validation,
it attempts to dump the debug state. This behavior can be controlled to always dump or never dump
using the --debug-dump command line option. Valid usages are
Usage                | Behavior
------               | -------------
\--debug-dump always | Always dump
\--debug-dump never  | Never dump
\--debug-dump error  | (default) Dump on run exception or post-run validation exception

During this dump, the simulator will write information about itself, about the Sparta Scheduler, the
exception, the device tree, the backtrace of the exception (if exception is a SpartaException) and
then every known resource will be asked to write its debug state to a file. During this procedure
all exceptions are suppressed and a note about any suppressed exceptions will be found in the dump.

When a debug dump occurs, the simulator will write a message such as:
\verbatim
  [out] Debug state written to "error-dump.dbg"
\endverbatim

If a post-run debug dump occurs, the output file used for this dump can be explicitly controlled
with the \--debug-dump-filenamet argument.
Usage                                    | Behavior
------                                   | -------------
\--debug-dump-filename FILENAME          | Save to FILENAME. If "", auto-generates filename
(omitted)                                | Auto-generate timestamped filename

\par
<b>NOTE:</b> Only exceptions are handled by this mechanism. Signals to not currently cause debug
dumps.

\par
<b>NOTE:</b> Support for debug dumps during other phases of the simulator (e.g. initialation,
teardown) may be added later

The debug dump file contains a section for each resource in the simulation that writes any debug
data to the output stream when given the chance. The file structure will look something like this
\code
================================================================================
Device tree:
================================================================================
_Sparta_global_node_ : <_Sparta_global_node_> {builtin}
+-top : <top (root)>
| +-foo : <top.foo>
| | +-fiz : <top.foo.fiz>
| | +-buz : <top.foo.buz>
<etc...>

top.foo.fiz
==============================================
debug info...
debug info...
debug info...
==============================================

top.foo.buz
==============================================
debug info...
debug info...
debug info...
==============================================
\endcode

This output contains some ANSI color escape sequences that can look strange if viewed as plaintext.
To see the colors represented by these sequences, either
\code
cat dumpfile
\endcode
or
\code
less -R dumpfile
\endcode

\par
<b>NOTE:</b> The format of this file is subject to change. It is not meant to be parsed.

\par
<b>NOTE:</b> In future versions the Sparta CLI may respond to SIGTERM, SIGSTOP/SIGCONT, and SIGQUIT may be
handled

\subsection backtraces 3.4 Backtraces
When the Sparta application framework encounters a fatal signal in the following list
 - SIGSEGV
 - SIGFPE
 - SIGILL
 - SIGABRT
 - SIGBUS

The simulator will attempt to print a bactrace to stderr and exit with EXIT_FAILURE (from cstdlib).
No debug dump is currently written for these signals. Backtraces are also written to the
error-dump.dbg (see \ref debug_dump) file when exiting the simulation due to an unhandled Exception.
Other signals may eventually be handled similarly.

\subsection tree_inspection 3.8 Device Tree Inspection
The device tree constructed by the simulator is visible in its entirity to a user who requests it.
These are highly verbose options, but give a clear picture of the content of the simulator. When
specifying parameters or creating manual report definitions, this is one way to view the
structure of the simulator.

Usage                       | Behavior
------                      | -------------
\--show-tree                | Show the entire simulation device tree between each phase of simulator startup and continues as usual
\--show-parameters          | Show all parameters in the device tree after configuration is complete and continues as usual
\--show-ports               | Show all ports in the device tree after the tree is fully bound and continues as usual
\--show-counters            | Show all counters and statistics in the device tree after the tree is fully bound and continues as usual
\--show-notifications       | Show all notification sources in the device tree after the tree is fully bound and continues as usual
\--show-loggers             | Show all log message sources in the device tree after the tree is fully bound and continues as usual
\--show-dag                 | Show the Event DAG (directed acyclic graph) and continues as usual
&nbsp;                      | &nbsp;
\--help-tree                | Same as \--show-tree \--no-run
\--help-parameters          | Same as \--show-parameters \--no-run
\--help-ports               | Same as \--show-ports \--no-run
\--help-counters            | Same as \--show-counters \--no-run
\--help-notifications       | Same as \--show-notifications \--no-run
\--help-loggers             | Same as \--show-loggers \--no-run
&nbsp;                      | &nbsp;
\--help-topic verbose       | Shows verbose help then exits
\--help-topic brief         | Shows brief help then exits
\--help-topic logging       | Shows help topic on logging then exits
\--help-topic reporting     | Shows help topic on logging then exits
\--help-topic topics        | Shows all help topics
\--help-topic pipeout       | Shows all help topics
\--help-topic parameter     | Shows all help topics

\par
<b>NOTE:</b> In future versions these options will support the printing of a specific subtree
instead of the entire device tree.

---
\section run_with_debugger 4 Running with a debugger
\subsection run_with_dbg_gdb GDB
GDB 4.7 is capable of debugging the sparta infrastructure and handles the GNU ISO C++11 standard
library.

The Sparta simulation framework catches and rethrows exceptions internally in order to
provide debug dumps, perform proper cleanup, and potentially preserve state for user inspection once
an interactive shell is built for sparta simulators. GDB breaks on uncaught exceptions by default,
which is not helpful for sparta. It is more effective to break on Exception throws as seen below.

\verbatim
gdb --args simulator
...
(gdb) catch throw
(gdb) run
\endverbatim

Alternatively, one can set a breakpoint on the sparta::SpartaException constructor to stop execution at
a point very close to an exception being thrown.

\verbatim
(gdb) break 'sparta::SpartaException::SpartaException()'
\endverbatim

This default construtor for SpartaException is always invoked (through delegation) regardless of how
the exception is constructed, so it will reliably be hit for every SpartaException (or subclass) that
is <b>constructed</b>.

Be sure to use other run-time debugging tools available, such as \ref msg_logging, \ref pipeline_collection,
\ref debug_dump, and \ref backtraces.

\subsection run_with_dbg_other Other Debuggers
Other debuggers such as Totalview have been used to debug sparta-based simulators. However, C++11 STL
support in this debugger is limited and types like std::shared_ptr from the GNU ISO standard library
can cause crashes in this debugger

---
\section data_proc_vis 4 Post-processing and Visualization
\subsection argos 4.1 Pipeline viewer (Argos)

Argos visualizes pipeline data generated from a simulator if that simulator supports sparta pipeline
collection. See \ref pipeline for instructions on using pipeline collection.

Argos is a free-form visualization tool for showing pipeline snapshots and crawls in custom
layouts. Development is ongoing.

Future editions of Argos will aim to provide more dashboard-like functionality with the ability to
show counters, statistics, and histograms from the simulation in addition to pipeline state.

*/

/*!
\page modeling Sparta for Simulator Development

-# \ref getting_started
-# \ref trees
-# \ref resources
-# \ref config
-# Instrumentation
  -# \ref logging
  -# \ref pipeout
  -# \ref stats
  -# \ref notification_generation
-# \ref communication
-# Modeling Components
  -# \ref timed_primitives
  -# \ref register_set
  -# \ref memory_objects
  -# \ref cache_lib
-# \ref core_example
-# \ref unit_testing
-# \ref errors_assertions
 */


/*!
\page getting_started Getting Started
\tableofcontents

\section linux_gcc Linux (gcc)
\subsection Prerequesites
- See Sparta's README.md for more information
\subsection boost_loc Boost Location
The absolute path to the prefix of a boost installation must be defined. This path must contain:
- lib directory containing libboost_*
- include directory containing boost subdirectory with all installed boost header files

\code
$ export BOOST=/absolute/path/to/boost_prefix
\endcode

\subsection easymake_loc EasyMake Location
The absolute path to easymake must be defined for Sparta:
\code
$ export EASYMAKE_DIR=/absolute/path/to/easymake
\endcode

\subsection build_sparta Building Sparta
Change directories into the sparta project base and make
\code
~/sparta/$ make
\endcode
And optionally
\code
~/sparta/$ make debug
\endcode

Internally, this uses the easymake infrastructure found at $(EASYMAKE_DIR) to build and relies on
Sparta files as wel as the locally installed boost found at the prefix $(BOOST).

Building sparta produces several things:
- libsparta.a containing the core sparta code
- transactiondb.so & transactiondb2.so containing Python bindings to a library capable of reading
pipeline-collection files generated by sparta pipeline collection

Typically, a built sparta should be run through its tests to ensure that the build worked
\code
~/sparta/$ make regress
\endcode

There are many tests which can be run in parallel, so enabling make to do this can save time. For a
4-core machine, try:
\code
~/sparta/$ make -j4 regress
\endcode

Be aware that this can interleave output from each test written to stderr, so if make fails, you may
need to re-run without parallel-build (-j) enabled to clearly see the failure and its context

\subsection Makefile
The simulator or test being built shoulve have a makefile that includes vars.mk from sparta.
Typically, the top of the makefile will contain:
\code
# Required for sparta's vars.mk
Sparta_BASE := /path/to/sparta/project

include $(Sparta_BASE)/vars.mk
\endcode

\subsection includes Sparta Header Files
All sparta header files can be found within the 'sparta' subdirectory of the sparta project. Sparta headers
expect to look eachother up by their path including this 'sparta' directory (e.g.
sparta/log/MessageSource.h). Therefore, your gcc compile lines must have an -I flag pointing to
the sparta project base.

\code
-I$(Sparta_BASE)
\endcode

\subsection linking Sparta Libraries
The vars.mk file provided by sparta to define some variables required to refer to some libraries in
the final link line.

The following must be present in any final link lines. The variables in this line are defined in
sparta's vars.mk
\code
-lsparta $(REQUIRED_Sparta_LIBS) -L$(BOOST_LIBDIR) -L$(Sparta_LIBDIR)
\endcode

Additionally, sparta is built with the following gcc flags. It is recommended that these also be used
for prejects dependant on sparta as they rely on sparta header files.
\code
-std=c++11 -Wall -Wextra -Winline -Werror -Winit-self -Wno-unused-function -Wno-inline
\endcode

The following flags are required to prevent warnings in boost from being treated
as errors
\code
-Wno-sequence-point
\endcode

\subsection building Making
Making using a custom Makefile that contains the above flags should result in no errors. Be careful
that shared and static libraries appear only once on each gcc command.

\subsection example Example Model
Sparta is distributed with an example model that uses sparta. This is a simple core with a dummy
pipeline that uses easymake to help define its makefiles. This application can be used as a starting
point for new simulators or tests.

\code
~/sparta/$ cd example/CoreModel
~/sparta/example/CoreModel/$ makec -j4
\endcode

Then, depending on your platform, the binary can be run with a line similar to this:
\code
~/sparta/example/CoreModel/$ ./bld-Linux_x86_64-gcc4.7/sparta_core_example -r1000 --report-all 1
\endcode

\subsection build_doc Documentation
You seem to already be reading the documentation, but to generate new doxygen pages for the C++ and
Python code in sparta, cd into the doc direcoty int the sparta project and run doxygen
\code
~/sparta/$ cd doc
~/sparta/doc$ doxygen
\endcode

<br/><br/>
\section windows_msvc Windows (MSVC 2012)
\subsection Prerequesites
\warning Sparta does not yet support Windows
 */

/*!
\page resources Resource Creation
\todo Write this section

\page pipeout Creating Collectable Stations for Pipeout-Generation
\todo Write this section
See \ref pipeout_format

\page stats Creating Counters and Statistics
\tableofcontents
\section ctr_stat_ref Reference
\li TBD

Sparta can expose numeric information from within the system through Counters and Statistics

\section ctr_stat_future
In the future, Sparta will be able to expose more information to reports and clients more easily
\li Eventually, a Sparta Python shell will allow direct query of counters and instantiation of statistics an reports

\todo Complete this section

\page timed_primitives Timed Primitives

\page register_set Registers (functional)
\tableofcontents
\section register_set RegisterSet
See \ref sparta::RegisterSet
\todo Write this section
\section Register
See \ref sparta::Register
\todo Write this section
\section Fields
See \ref sparta::Register::field
\todo Write this section

\page memory_objects Memory Objest (functional)
\todo Write this section

\page cache_lib Cache Library (functional)
\todo Write this section

\page unit_testing Unit Testing
\todo Write this section

\page errors_assertions Exceptions and Assertions
\todo Write this section
 */


/*!
\page application Simulator Application Level

The sparta simulator application-level infrastructure exists within the sparta:app namespace. This
information is useful for anyone creating a simulator command-line user interface or tool as well
as anyone running, configurating, and extracting data from a sparta-enable simulator.

-# \ref common_cmdline
-# integration with sparta app
-# \ref param_format
-# Report formatters
-# Log formatters
-# Checkpointing
-# Python shell
-# Run control
-# Remote Interfaces
 */

/*!
\page client_apis Sparta Client APIs

These aspects of sparta allow inspection and interaction with a sparta-enabled simulator by an outside
C++ entity. In general, any entity in the device tree can be located given a pointer to the root of
the tree. Nodes can then be dynamically cast to the appropriate class (e.g. counter, parameter,
register, etc.)

-# \ref trees
-# \ref tree_navigation
-# \ref taps
-# \ref notification_observation
 */

/*!
\page common_cmdline Command Command Line Interface
Sparta provides a number of application-frontend classes in the sparta::app namespace. The essential
goal of these classes is to make the creation of a sparta-based simulation quick, simple, and
consistent.

\section cmdline_sim CommandLineSimulator
sparta::app::CommandLineSimulator provides a command-line parser and help-text generator based on
boost::program_options. This interface dozens of options common to all sparta-based simulations and
allows simulator-specific options to be added from outside of sparta as needed.

Among all features provided by this class (in conjunction with sparta::app::Simulation, the most
useful are command-line-based logging configuration and simulator parameter-setting (configuration).

\subsection cmdline_log_cfg Logging Configuration
Logging output configured through the classes in the sparta::app namespace apply as the simulator
starting up, while running, and when tearing down.

<br/>To log all warning messages from the entire device tree (top or below) to stdout (1).<br />
\verbatim
$ ./sim -l top warning 1
\endverbatim

<br/>To log all warning messages from core0 and core1 to newly-created file "cores.log"<br/>
<i>Note that this assumes nodes called core0 and core1 as children of the 'top' root node</i>.<br />
\verbatim
$ ./sim -l top.core0 warning cores.log -l top.core1 warning cores.log
\endverbatim

<br/>To log all messages of <b>any category</b> from core0 to stdout (1) and all warnings from the entire
simulated tree to stderr (2)<br/>
<i>Note that this assumes nodes called core0 and core1 as children of the 'top' root node</i>.<br />
\verbatim
$ ./sim -l top.core0 "" cores.log -l top warning 2
\endverbatim

<br/>Note that the logging system is smart about routing multiple overlapping trees to the same output
such that any message can only be written to a particular file exactly once. In this example,
the warnings from the entire simulated tree will be written to cores.log and all messages from core1
(which includes warnings) will be written to cores.log. However, each warning message from core1
will be seen exactly 1 time in cores.log<br/>
<i>Note that this assumes nodes called core0 and core1 as children of the 'top' root node</i>.<br />
\verbatim
$ ./sim -l top warning cores.log -l top.core1 "" cores.log
\endverbatim

\subsection cmdline_sim_cfg Command Line Simulation Configuration
\verbatim
$ ./sim -c myconfiguration.yaml
\endverbatim

\section example_cmdline_help_out Example Output
The following is example output from CommandLineSimulator when the --help option has been
set. This was generated from a sparta-based model on Jan 10, 2013.
\verbatim
General Options:
  -h [ --help ]                         show this help message
  --help-brief                          show brief help message
  -r [ --run-time ] RUNTIME             Run length of simulation
  --warn-file FILENAME                  Filename to which warnings from the
                                        simulator will be logged. This file
                                        will be overwritten
  --no-warn-stderr                      Do not write warnings from the
                                        simulator to stderr. Unset by default
  --show-tree                           Show the device tree during all stages
                                        of construction excluding hidden nodes.
                                        This also enables printing of the tree
                                        when an exception is printed
  --show-parameters                     Show all device tree Parameters after
                                        configuration excluding hidden nodes.
                                        Shown in a separate tree printout from
                                        all other --show-* parameters.
                                        See related: --write-final-config
  --show-ports                          Show all device tree Ports after
                                        finalization. Shown in a separate tree
                                        printout from all other --show-*
                                        parameters
  --show-counters                       Show the device tree Counters,
                                        Statistics, and other instrumentation
                                        after finalization. Shown in a separate
                                        tree printout from all other --show-*
                                        parameters
  --show-notifications                  Show the device tree notifications
                                        after finalization excluding hidden
                                        nodes and Logger MessageSource nodes.
                                        Shown in a separate tree printout from
                                        all other --show-* parameters
  --show-loggers                        Show the device tree logger
                                        MessageSource nodes after finalization.
                                          Shown in a separate tree printout
                                        from all other --show-* parameters
  --show-dag                            Show the dag tree just prior to running
                                        simulation
  --write-final-config FILENAME         Write the final configuration of the
                                        device tree to the specified file
                                        before running the simulation
  --write-final-config-verbose FILENAME Write the final configuration of the
                                        device tree to the specified file
                                        before running the simulation. The
                                        output will include parameter
                                        descriptions and extra whitespace for
                                        readability
  -p [ --parameter ] PATTERN VAL        Specify an individual parameter value.
                                        Multiple parameters can be identified
                                        using '*' and '?' glob-like wildcards.
                                        Example: --parameter
                                        top.core0.params.foo value
  -c [ --config-file ] FILENAME         Specify a YAML config file to load at
                                        the top of the simulator device tree.
                                        Example: "--config-file config.yaml"
                                        This is effectively the same as
                                        --node-config-file top params.yaml
  -n [ --node-config-file ] PATTERN FILENAME
                                        Specify a YAML config file to load at a
                                        specific node (or nodes using '*' and
                                        '?' glob-like wildcards) in the device
                                        tree.
                                        Example: "--node-config-file top.core0
                                        core0_params.yaml"
  -z [ --pipeline-collection ] OUTPUTPATH
                                        Run pipeline collection on this
                                        simulation, and dump the output files
                                        to OUTPUTPATH. OUTPUTPATH can be a
                                        prefix such as myfiles_ for the
                                        pipeline files and may be a directory
                                        Example: "--pipeline-collection
                                        data/test1_"
                                        Note: Any directories in this path must
                                        already exist.

  --heartbeat HEARTBEAT                 The interval in ticks at which index
                                        pointers will be written to file during
                                        pipeline collection. The heartbeat also
                                        represents the longest life duration of
                                        lingering transactions. Transactions
                                        with a life span longer than the
                                        heartbeat will be finalized and then
                                        restarted with a new start time. Must
                                        be a multiple of 100 for efficient
                                        reading by Argos. Large values will
                                        reduce responsiveness of Argos when
                                        jumping to different areas of the file
                                        and loading.
                                        Default = 5000 ticks.

  -l [ --log ] PATTERN CATEGORY DEST    Specify a node in the simulator device
                                        tree at the node described by PATTERN
                                        (or nodes using '*' and '?' glob
                                        wildcards) on which to place place a
                                        log-message tap (observer) that watches
                                        for messages having the category
                                        CATEGORY. Matching messages from those
                                        node's subtree are written to the
                                        filename in DEST. DEST may also be '1'
                                        to refer to stdout and '2' to refer to
                                        cerr. Any number of taps can be added
                                        anywhere in the device tree. An error
                                        is generated if PATTERN does not refer
                                        to a 1 or more nodes. Use --help for
                                        more details
                                        Example: "--log top.core0 warning
                                        core0_warnings.log"
  --report PATTERN DEF_FILE DEST [FORMAT]
                                        Specify a node in the simulator device
                                        tree at the node described by PATTERN
                                        (or nodes using '*' and '?' glob
                                        wildcards) at which generate a
                                        statistical report that examines the
                                        set of statistics based on the Report
                                        definition file DEF_FILE. At the end of
                                        simulation, the content of this report
                                        (or reports, if PATTERN refers to
                                        multiple nodes) is written to the file
                                        specified by DEST. DEST may also be  to
                                        refer to stdout and 2 to refer to
                                        stderr. Any number of reports can be
                                        added anywhere in the device tree.An
                                        error is generated rror generated if
                                        PATTERN does not refer to 1 or more
                                        nodes. FORMAT can be used to specify
                                        the format. See the report options
                                        section with --help for more
                                        details about formats.
                                        Example: "--report top.core0
                                        core_stats.yaml core_stats txt"
                                        Example: "--report top.core*
                                        core_stats.yaml core_stats.%l"
                                        Example: "--report top.core*
                                        core_stats.yaml core_stats"
  --report-all DEST [FORMAT]            Generates a single report on the global
                                        simulation tree containing all counters
                                        and statistics below it. This report is
                                        written to the file specified by DEST
                                        using the format specified by FORMAT
                                        (if supplied). Otherwise, the format is
                                        inferred from DEST. DEST may be a
                                        filename or 1 to refer to stdout and 2
                                        to refer to stderr. See the report
                                        options setcion with --help for
                                        more details.This option can be used
                                        multiple times and does not interfere
                                        with --report.
                                        Example: "--report-all core_stats.txt"
                                        Example: "--report-all output_file
                                        html"
                                        Example: "--report-all 1"
                                        Attaches a single report containing
                                        everything below the global simulation
                                        tree and writes the output to
                                        destination
  --debug-on DEBUG_ON_TICK
                                        Delay the recording of useful
                                        information starting until a specified
                                        simulator tick. This includes any
                                        user-configured pipeline collecion or
                                        logging (builtin logging of warnings to
                                        stderr is always enabled). Note that
                                        this is just a delay, logging and
                                        pipeline collection must be explicitly
                                        enabled.
                                        WARNING: The DEBUG_ON_TICK may only be
                                        partly included. It is dependent upon
                                        when the scheduler fires. It is
                                        recommended to schedule a few ticks
                                        before your desired area.
                                        Example: --debug-on 5002
                                        --pipeline-collection PREFIX_ --log top
                                        debug 1
                                        begins pipeline collection to PREFIX_
                                        and logging to stdout at some point
                                        within tick 5002 and will include all
                                        of tick 5003

Application-Specific Options:
  --version                        produce version message
  -i [ --instruction-limit ] LIMIT Limit the simulation to retiring a specific
                                   number of instructions. 0 (default) means no
                                   limit. If -r is also specified, the first
                                   limit reached ends the simulation
  --add-trace TRACEFILE            Specifies a tracefile to run

Advanced Options:
  --show-hidden         Show hidden nodes in the tree printout (--show-tree).
                        Implicitly turns on --show-tree
  --verbose-config      Display verbose messages when parsing any files (e.g.
                        parameters, report definitions,  etc.). This is not a
                        generic verbose simulation option.
  --show-options        Show the options parsed from the command line
  --debug-sim           Turns on simulator-framework debugging output. This is
                        unrelated to general debug logging

Logging:

  The "--log" DEST parameter can be "1" to refer to stdout, "2" to refer to
  stderr, or a filename which can contain any extension shown below for a
  particular type of formatting:

  ".log.basic" -> basic formatter. Contains message origin, category, and
  content
  ".log.verbose" -> verbose formatter. Contains all message meta-data
  ".log.raw" -> verbose formatter. Contains no message meta-data
  (default) -> Moderate information formatting. Contains most message meta-data
  excluding thread and message sequence.

  Note that parameters and configuration files specified by the -c (global
config file), -n (node config file), and -p (parameter value) options are
applied in the left-to-right order on the command line, overwriting any previous
values.

Reports:

  The "--report" PATTERN parameter can refer to any number of nodes in the
  device tree. For each node referenced, a new Report will be created and
  appended to the file specified by DEST for that report. If these reports
  should be written to different files, variables can be used in the destination
  filename to differentiate:
    %l => Location in device tree of report instantiation
    %i => Index of report instantiation
    %p => Host process ID
    %t => Timestamp
    %s => Simulator name

  Additionaly, the DEST parameter can be a filename or "1", referring to stdout,
  or "2", referring to stderr

  The optional report FORMAT parameter must be omitted or "txt" in this version.
Only plaintext output is supported

\endverbatim
 */

/*!
\page taps Programmatic Observation of Log Messages

 */


/*!
\page config Simulator Configuration

Sparta includes a 'parameter' mechanism for configuring (and querying the configuration of) a sparta
device tree both through C++ and configuration files (See \ref param_format).

\section config_goals System Goals
The Sparta configuration system exists to allow configuration of hierarchical simulator before running
a simulation and inspection (saving) of the final system configuration for the purpose of analysis
or run reproduction.

\section config_usage Simulator Subclass Configuration
The user-side configuration of a simulator is covered in \ref ctrl_cfg and \ref param_format

Further background information on configuration files, parameters, and tree construction phases is available
in this <TBD>,
which contains diagrams of Sparta tree elaboration.
The following information is the most current documentation on Sparta simulator construction.

\par Overview
Simulator initialization, at it's simplest, establishes an initial device tree (\ref trees) containing the parameters
available for a simulator which is then populated from user configuration files and command-line
parameters. Based on these parameters, various C++ resources (subclasses of sparta::Resouce) are
instantiated. These resources then add to the device tree some non-configurable objects such as
counters, statistics, registers, notification sources, memory interfaces,
logging message sources, and ports. At this time the tree is finalized (no more changes) and
simulation begins.

\par Phased Construction
The simulation setup is divided into several phases
\li building - Creating an initial topology of placeholder (sparta::ResourceTreeNodes) and other
TreeNodes to roughly define the topology
\li configuration - Applying user configuration to the tree established in the building phase
\li finalization - Walking through the configured placeholder tree and instantiating the underlying
resources based on the configuration applied to the tree in the previous phase
\li binding - Not a true phase, but after finalization the simulator can bind ports together between
its components. No changes to the tree may be made at this time
\li running - Running the simulation. No changes to the tree may be made at this time

\par Phased Construction Legacy/Limitations
Note: <i>These limitations have been (or will be) addressed by additional features: "Unbound Parameter Tree",
"Dynamically Created Parameter Sets", "Topology Files".</i>
Early in Sparta's development, these phases existed to keep the configuration process simple and allow
all user onfiguration to be written into to the simulator tree's sparta::Parameter nodes exactly once
(after building the initial tree) - eliminating the need for re-processing the configuration inputs
multiple times. If new parameters could be added tothe tree at any time, re-reading the input
configuration could be and expensive operation. This meant that all nodes in the device tree using
Sparta parameters would need to be specified before reading the configuration at all. The result was that
Sparta parameters could not be used to dictate how many instances of another component should be
constructed if that other component had its own Sparta parameters.

While this limitation forced the model owner to define their entire parameterized "topologies" in
C++ code, probably makes simulator initialization code maintainable and clearly outlines the
simulation hierarchy. It is also analogous to how "Topology Files" will work once implemented.
In early Sparta, this did introduces a substantial limitation in the form of disallowing sparta
parameters to be used to specify the overall simulator topology (e.g. how many cores to create, how
many of what units will exist in each core) and prevented resources from creating new parameterized
resource children without some challenging ResourceFactory code. Support for pattern-matching-based
parameter identification complicated the necessary optimization of compressing the set of input
parameters into an efficient tree structure. Initial requirements did not necessitate such this
feature, but support for topology definition through parameters has been added using the
"Unbound Parameter Tree" and "Dynamically Created Parameter Sets".

\par Unbound Parameter Tree
Recently, the unbound parameter tree was added to address the aforementioned strict initialization
ordering where the initial tree must be built to include all parameters and then configured (see sparta::ParameterTree).
This tree enables access to the user configuration input while
constructing the initial device tree in (sparta::app::Simulation:buildTree_) using an efficient
parameter-tree structure which handles pattern-based parameter paths and ensures each parameter is
consumed by code, even if not actually associated with a sparta::Parameter node in the final device tree.
<br/>
This feature is currently missing functionality (Nov 12, 2013). The missing functionality includes:
\li The ParameterTree is not capable of understanding configuration files or command line parameters
containing parent references (e.g. "x..y" or ".x"). This is mainly an inconvenience. If encountered
in a configuration file, generates a warning.

The unbound parameter tree is most useful during the build phase. Unbound parameters are read from a
configuration file before the building phase and can be accessed even before any nodes are created.
If a node foo with a parameter x is expected to be created later but required now (for topology),
it can be accessed if specified by the user.
\code
auto pn = n->getRoot()->getAs<sparta::RootTreeNode>()->getSimulator()->getUnboundParameterTree()->tryGet("top.foo.params.x");
if(pn){
    std::cout << "Got parameter Value for x = " << pn->getValue() << std::endl;
}
\endcode
This behavior is still experimental and under development. It should be improved soon.
\li The contents of the unbound parameter tree are not yet written as part of the the final
configuration output (see \--write-final-config). Therefore, unbound parameters may be missing when
trying to reproduce a simulation run using the final configuration output of that run. The best
practice for this issue is that all unbound parameters should correspond to sparta::Parameter nodes
by the time the simulation is finalized. At any time before finalization the simulator should simply
create new ParameterSet and Parameter Nodes matching the location of the unbound parameters consumed
earlier
\li The unbound parameter tree provides no method for lexical casting its content to a vector like a
sparta::Parameter node does. Interpreting a value from the unbound parameter tree as a vector must
currently be done manually.
\li No default values are provided by the unbound parameter tree. Therefore, building-phase code that
consumes unbound parameters must be made aware of the defaults for those parameters in case the user
does not specify that parmeter as input. This could be done by accessing a static variable which
defines a default value in the relevant ParamaeterSet declaration.

\par Initialization Phases
Most of the initialization phases are marked by a different virtual method within
sparta::app::Simulation, though some work is done in the subclass
constructor and in sparta::Resource subclass constructors. These phases are part of
sparta::PhasedObject, from which every sparta::TreeNode in the device tree inherits.

\par Phase 1. Resource Factory Instantiation
First, a number of sparta::ResourceFactory objects are registered with a sparta::app::Simulation. These
objects associate a resource name with a factory capable of instantiating that resource. For
example, a factory might be declared for instantiating a "core" object and a "lsu" object. This is
typically done within the constructor of a subclass of a sparta::app::Simulation.
<br/>
The intent of these objects a is to identify resource classes by a string name which can be
referenced by parameters specifying topology and eventually used by some sort of
topology-definition file one such a feature exists.

\par Phase 2. Build-Tree Phase
Within a subclass of sparta::app::Simulation, the sparta::app::Simulation::buildTree_ method allows the
subclass to define an initial device tree. The overall device tree topology must be established
at this point. This device tree should contain any number of sparta::ResourceTreeNode instances
constructed referring to the factories created during resource factory instantiation. When a
sparta::ResourceTreeNode is created, the sparta::ParameterSet subclass specified by the factory is also
constructed and attached to the tree as a child of the ResourceTreeNode called "params". This is
immediately available though the contained parameters have default values only - they are not read
from the input configuration until after the build phase (This will change later).
<br/>
With the unbound parameter tree feature (see above), parameters can be accessed before and
during initial tree constuction. This allows the simulator to consume user parameters not associated
with any sparta::Parameter node to determine topology. (Note: More convenient ways of specifying
topology such as topology files may be implemented later).
<br/>
Consuming parmeters from the unbound tree can be done from within sparta::app::Simulation::buildTree_ as follows:
\code
const auto& pt = getUnboundParameterTree();

{
    // Approach 1: Assume top.params.cluster_count exists. Throw if nonexistant
    uint32_t num_clusters = pt.get("top.params.cluster_count").getAs<uint32_t>();
}
{
    // Approach 2: Atempt to get top.params.cluster_count and use a default value if it does not exist
    auto ccn = pt.tryGet("top.params.cluster_count");
    uint32_t num_clusters = 1; // Default
    if(ccn){
        num_clusters = ccn->getAs<uint32_t>();
    }
}
\endcode
Note that all parameters in the unbound tree must be consumed or must eventually correspond to
sparta::Parameter nodes in the device tree one finalization is complete.
<br/>
The best practice for using a parameter form the unbound parameter tree which must be read in the
build phase is to eventually create a sparta::ParameterSet node with a sparta::Parameter corresponding
to the path read from the unbound parameter tree. In the above example, A ParameterSet would be
created as a child of the "top" node and it would contain a parameter called "cluster_count".
Because of the aforementioned limitations, this parameter is not automatically populated from user
input until after the build phase, but doing this still serves several important purposes.
\li It Makes the parameter visible to the end-user when inspecting the tree (\--show-tree or
interactively [when the Python shell is complete])
\li The parameter will be written out whenever \--write-final-config[-verbose] is used.
\li Eventually, the value will be read from this ParameterSet immediately instead of using the
unbound tree. The unbound tree's visibility to simulator subclasses will be deprecated at that point

\par
The unbound parameter tree cannot be altered by the simulator subclass at any time. It represents
external user configuration only. However, new default values for any sparta::Parameter nodes created
can be set during the build phase. Note that, input user configuration
may override any parameter later if said parameter is specified in the input user configuration. To
force-override user parameters, set the value of any sparta::Parameter node during the <b>configuration
phase</b> (see below)
\warning accessing these parameters provides no method for lexical casting to a vector. Interpreting
a value from the unbound parameter tree as a vector must currently be done by hand.

\par Phase 3. Configure Tree Phase
The configuration phase for simulator subclasses is performed in the virtual
sparta::app::Simulation::configureTree_ method. Immediately before this method is called,
the sparta::app::Simulation internally applies the input configuration to all Parameter nodes in the
device tree.
<br/>
At this point, user parameters can be overridden by the simulator itself. A common case of this is
where simulator-specific command line arguments are given which have the same semantics as some
parameter in the device tree. Because simulator-specific command-line options should generally
override user configuration input, these commands can override values in the parameter tree.

\par
In this example, A list of traces on the command line (processed earlier into a trace_filenames_
member) is iterated and one trace filename is assigned to a parameter in each core object. As a
result, the actual traces used in this simulation will always show up in the \--write-final-config
output, even if the user's input configuration is overridden. The run can then be reproduced based
on the final configuration as expected
\code
uint32_t i = 0;
for(const std::string& trace : trace_filenames_){
    // Find the parameter
    std::stringstream ss;
    ss << "core" << i;
    sparta::TreeNode* core_node = nullptr;
    try{
        core_node = getRoot()->getChild(ss.str());
    }catch(sparta::SpartaException& ex){
        throw sparta::SpartaException("Unable to find a core below top called \"") << ss.str()
              << "\". It is possible that too many traces were specified on the command line "
                 "such that they could not all be assigned to a core. Error encountered at trace"
              << i << ": " << trace;
    }
    // Get top.core<i>.params.trace_filename node. Throws if not found
    core_node->getChildAs<sparta::ParameterBase>("params.trace_filename")->setValueFromString(trace);
    ++i;
}
\endcode

\par
Configuration is an opportune time to create and attached clocks to the tree. This can be done
during buildTree, but must be done before the end of configuration to prevent resources from being
instantiated with no clock
\code
// Within configureTree_
sparta::Clock::Handle master_clock = getClockManager().getRoot();
core_clock_ = getClockManager().makeClock("core",
                                          master_clock,
                                          core_frequency_mhz_);

// for each core... {
    core_node->setClock(core_clock_.get());
}
\endcode

\par
Following configuration, all resources will be constructed and the tree will be finalized. This is
the last chance to alter the tree structure from within the simulation subclass

\par Phase 4. Finalize Tree Phase
There is no virtual method in sparta::app::Simulation for simulators to implement. This phase involves
Sparta walking the existing device tree and constructing all Resources as defined by the tree. For
each ResourceTreeNode encountered in the tree, Sparta will construct the resource through the
associated ResourceFactory using that ResourceTreeNode and its parameter set as arguments to the
Resource's constructor. Each resource can create new children nodes (e.g. sparta::Port,
sparta::CounterBase, sparta::StatisticDef, sparta::StatisiticSet, sparta::PortSet, sparta::log::MessageSource,
sparta::NotificationSource, and more.

\par
Resources can even create child ResourceTreeNodes at this time. Currently, the sparta::Parameters for
these ResourceTreeNodes constructed at finalization-time will not be automatically populated from
user configuration input. Instead, the parameters must be explicitly set. Eventually these
parameters will be automatically populated (see the "Unbound Parameter Tree" section above).
sparta::Parameter nodes created at this time will <b>not</b> show up in the final configuration output
until dynamic automatic population from input configuration is implemented for all Parameters

\par
During finalization, a resouces (in its constructor) cannot be sure if a neighbor or even a child
resource has been constructed yet. New nodes may still be added to the tree as finalization
continues and no assumptions should be made about resources initialization order. The only exception
to this rule is that parent nodes' resources will always be created before their childrens'
resources. Any references to other resource objects (such as exchanging pointers) should be done in
the startup handler (below). It is safe, however, to look at parent nodes (and all ancestors) and
their parameters (if any) for each resource as it is constructed at this point. This is because
those nodes must have been created for this a resource's node to exist.

\par Phase 5. Bind Tree Phase
After finalization, any remaining ports can be bound together in the
virtual sparta::app::Simulation::bindTree_ method. Binding is technically not a phase, just an action
that can take place after the tree is finalized and must be done before running. At this point, the
device tree is finalized, all resources are constructed, all nodes that will be present in the
running simulation exist, and no nodes may be added or destroyed.

\par
Ports should be bound together as per the desired simulation topology.
\code
sparta::bind(getRoot()->getChildAs<sparta::Port>("core0.ports.out_to_memory"),
           getRoot()->getChildAs<sparta::Port>("memory.ports.in_from_core0"));
\endcode

\par Phase 6a. Run Startup Handling
Immediately before running, the sparta::Scheduler invokes startup handlers. At this time, the tree is
guaranteed to be finalized with all resources instantiated. It is safe for all nodes to access any
other resource. Prior to this point, a resouces (in its constructor) cannot be sure if a neighbor or
even a child resource has been constructed.
\code
MyModel::MyModel(sparta::TreeNode * node,
                 const MyModelParameterSet * p)
{
    // Schedule startup handler
    node->getClock()->getScheduler()->scheduleStartupHandler (CREATE_Sparta_HANDLER (MyModel, startupHandler_));
}

void MyModel::startupHandler_ ()
{
    // Access children and sibling resources
    // Schedule initial events
    // Register for notifications, etc.
}
\endcode

\par Phase 6b. Run Phase
Running is not relevant to simulation initialization except that it comes after binding and no
modifications can be made the the device tree structure at run time. This also means that no
TreeNodes may be destroyed until the teardown phase

\par Phase 7. Teardown Phase
Prior to simulator shutdown, the entire device tree is marked as being in the teardown phase. When
destructing sparta::TreeNode objects, each will throw a sparta::SpartaException if not marked as being in
the teardown phase. The goal of this behavior is to prevent any user from accidentally destroying
TreeNodes at run-time or even construction time once they are added to a tree. Deleting nodes at
run-time can be challenging for Sparta (especially with a Python shell or other remote
clients) to handle. Because no legitimate reasons for supporting this have been proposed, destroying
nodes prior to teardown is prohibited with the exception of sparta::Counter and sparta::StatisticDef
where C++ move semantics can be used to swap nodes during construction in order to allow these nodes
to be instantiated within an vector without introducing additional pointer indirection in
performance-critical code.
sparta::app::Simulation  attempts to cleanly tear down by freeing all nodes allocated on the heap and
destructing any object on the simulator's stack. Sparta alwys intends to teardown with no memory leaks
so that any number of simulations can be run consecutively in the same process.

The sparta command line parameter \--show-tree/\--show-parameters (or \--help-tree/\--help-parameters)
can be used to show the values of all parameters after the build, configuration, and binding phases
of the construction process.


\section config_des_req Configuration System Design Requirements
For reference, a number of the requirements for the configuration system design are listed here.
-# Enable command-line configuration of a simulation tree
-# Support configuration-files to configure a simulation tree
   - Separate configuration files for each component in the simulation should be allowed by not
   required
-# Support inspection of all parameters at any time including support to save these parameter to
disk in such a way that they can be reloaded for reproducability
-# Make configuration communication (as opposed to run-time simulation data/timing) between
simulation components difficult in favor of the sparta configuration system.
   - This ensures tracability of configuration by exposing all parameters to the configuration
   system such that they may be queried and analyzed. Bugs related to direct C++ communication of
   parameters between components at configuration-time can be difficult to debug and extra code is
   required to capture these parameters to compare against other simulations.
-# Prevent modification to the set of parameters once the simulation run begins
-# Strongly type parameters to support C++ plain-old-datatypes as well as strings
-# Support parameter having vector types so that 1 parameter could be a list of values (e.g.
[1,2,3])
-# Require descriptions associated with every parameter
-# Define a resource as 1:1 association of a resource class and a parameter set to ensure that all
instances of that resource have the same parameters and are effectively interchangable.
-# Allow validators to be registered on individual parameters
 */

/*!
\page logging Textual Message Logging

Sparta includes a mechanism for generating textual messages that can be configurably directed to
various output files to generate a textual trace of the state or events inside a simulation.

This page details the modeling view of the logging system - that is, how log messages are emitted

\section logging_goals Logging System Goals
The Sparta logging feature exists to allow model and simulator owners to generate free-form messages
of a certain 'category' from a specific point within a \ref trees "device tree". Each log message
should be filterable by its category and origin by end-users of the simulator. Users should also
have the ability to redirect log messages to any number of output files including stdout/stderr
based on log message origin and category. This allows users to selectively log messages from
specific subsections of the simulated system at various levels. Thus, a single simulation run can
produce many log files containing any combination of messages depending on user configuration.

\section log_des_reqs Logging System Design Requirements
-# Associate Log messages with a single node in a \ref trees "device tree"
-# Associate Log messages with one or more category strings (e.g. "info", "debug", "warning")
-# Itentify, before simulator finalization, what log categories a simulation is capable of
generating
-# Allow the user-configurable routing of log messages generated by a simulation to a specific set
of files and/or standard streams based on the origin and category of each message.
   - This should never cause duplicate messages in the same file/stream
   - A client of the simulator should not be able to interfere with another's log observeration
-# Minimize performance cost of logging infrastructure when logging is disabled

\section logg_concept_usage Conceptual Usage
\subsection scoped_log Scoped Logging
Scoped logging refers to logging message originating at a specific node in the simulation's device
tree. This is the preferred means of logging as it allows log messages to be filtered by their
origin. Additionaly, models generating log messages can determine if anything is observing its
messages and avoid wasting time performing expensive string formatting if not

<h5>Scoped Logging Usage</h5>
-# A sparta::TreeNode must exist in a sparta device tree which will represent the context (origin) of
the log message. See \ref trees. This will typically be either a plain sparta::TreeNode or
sparta::ResourceTreeNode
-# A sparta::log::MessageSource must be constructed as a child node of the context above. This must be
done before the sparta finalization phase.
-# At any time, a message may be posted to this message source and if the logging infrastructure is
observing this notification sourece or its parent of any number of generations, then that message
will end up in a log file or standard stream.

Control of log message routing is described in the \ref log_capturing section


\subsection global_log Global Logging
Occasionally, a log message will be generated by some component that is not part of the simulator
proper. Though this is very rare, singletons and other globally scoped infrastructure may not allow
themselves to be integrated into a sparta device tree. Therefore it is possible to log through a
global node instead of a location within the device tree.

Control of log message routing is described in the \ref log_capturing section

\code
#include "sparta/log/MessageSource.hpp"
...
sparta::log::MessageSource::getGlobalWarn() << "global warning message";
sparta::log::MessageSource::getGlobalDebug() << "global debug message";
\endcode

\section log_capturing Capturing Log Messages
Control of how log messages are filtered and routed to destination log files/streams is covered (at
a low level) in the \ref taps section. However, the sparta::app::CommandLineSimulator and
sparta::app::Simulation classes assist in setting up log message observation, routing, and formatting
based on command line options or manually. The \ref common_cmdline section covers this.

\section log_usage_notes Usage Notes
-# Errors should generally not be captured in the log system

\section log_impl_notes Implementation Notes
-# The logging system is built on the sparta \ref notification_generation system.
 */


/*!
\page notification_generation Notification Generation
Allows a model to generate observable notifications having a developer-defined class as the payload
receieved by the observers.

Observing these notifications is described in \ref notification_observation
 */

/*!
\page notification_observation Notification Observation

Generating these notifications from within a model is described in \ref notification_generation
 */

/*!
\page perf_model_tutorial Timed Component Tutorial
 */

/*!
\page framework_dev Framework Development
\tableofcontents

This is intended for anyone modifying Sparta

\section changelogs Changelogs
The intent of sparta/ChangeLog is to document API-level changes and other breaking changes to sparta.
This file must be updated whenever changes are made which may break an API or change the behavior of
of the sparta framework in a way that can cause any clients of the sparta framework (including models
and interfaces) or any tools which consume sparta output to break. This includes API changes, API
semantic changes, file formats changes, and textual output formatting changes.

All changes (including API changes) should be included in version-control-system commit messages.
The ChangeLog is an additional summary of breaking changes in sparta.

At a component level, some API changes may be missing from the ChangeLog early in the project.

\section regression Regression Testing
All sparta components and multi-component assemblies should have tests in subdirectories of the tests
directory at the root of the repository. These tests should follow the style and conventions of
existing test and be added to the Makefile in the test directory.

\section meta_doc Meta Documentation

All Sparta features should be consistently documented. Whenever possible, hand-written documentation
should reference to Sparta C++ documentation using doxygen \\ref

\section content Page Content
Where appropriate, a Sparta feature (whether a single class or collection of features) should contain
documentation to justify its existance and current implementation.
\subsection desc Feature Description
This describes the feature in a few short sentances
\subsection goal Goals
This section describes the overal goal of the feature and what problem it attempts to solve. This
is effectively an existence rationale and needn't be exhaustive. Design documents for sparta are
expected to exist outside of this documentation
\subsection reqs Relevant Requirements
This is a brief list of requirements driving the design of the feature. This list should give the
reader a sense of why the feature behaves the way it does.
\subsection concept Conceptual Usage
A high-level overview of how to developer or user should use this feature
\subsection examples Example Usage
This should contain example C++/Python code as appropriate
\subsection references References
This should contain a list of links (\\ref) to other related features or components

\section doxygen convention Doxygen Convention

\subsection code_conv Code Documentation Convention
C++ and Python code in sparta should be completely documented with block-style doxygen comments
(i.e. /&amp; &amp;/). All namespaces, classes, enums, typedefs, constants, and functions must be documented
regardless of scope. Whenever possible, all parameters and pre/post conditions of each function
should be described. All files should have a \\file doxygen entry.
Code comments should not extend past character column 80 except for preformatted content such as
example code.

\subsection text_conv Texual Documentation Convention
The doxygen pages automatically generated from namespace and classes related to each Sparta
component should contain detailed explanations of how to use that component's features.
However, some concepts involve multiple components which requires standalone documentation pages to
be written. These are mainly use-case-based (e.g. \ref logging How to Log).
These manually-written doxygen pages can be found in source code and text files within sparta and
should be left-aligned at column 0 and should contain no lines that extend past character column 100
except for preformatted content such as example code.
 */

/*!
\page q_and_a Q&A
Questions and Answers (not necessarily frequently-asked)
\todo Complete this page
 */

/*!
\page formats Sparta File formats

-# \ref param_format
-# \ref report_def_format
-# \ref report_out_format
-# \ref pipeout_format
-# \ref checkpoint_format
-# \ref log_out_format

\page param_format Parameter/Configuration Format (.cfg,.yaml)
This page describes the grammar and usage of a sparta parameter file.

\todo Complete this page

Configuration files are a subset of YAML (<a href="http://www.yaml.org/spec/1.2/spec.html">spec v1.2</a>)
used to assign values to parameters in a Sparta device Tree
(\ref trees). The format is simple: a typical YAML file consists of nested YAML maps which describe
how the device tree is traversed to assign parameters. Each key within these maps represents a
relative path in the device tree. Each value can be another map (implying descent depeer into the
tree) or a value to assign to the location indicated by the key. These leav values are either a
scalar (e.g. string, integer) or a sequence (of strings, integers, other vectors, etc.)

Configuration files are typically applied at the global namespace in the device tree (above the
"top" object).

This example shows a very simple usage of parameter files. Note that block-style maps in YAML are
indicated by "key: value" syntax and that the value can be an additional, nested map.

\code
# YAML comment. Lost during interpretation
top:
    a:
        params:
            param1: 1
            param2: foo
\endcode

This tree assigns values "1" and "foo" to the sparta::Parameter nodes located at "top.a.params.param1"
and "top.a.params.param2" respectively. It is up to the sparta::Parameter objets to interpret the as
a native type instead of a string (if necessary).

Note that multiple levels in the tree can be specified in one mapping key. The previous
configuration file can be rewritten as:

\code
top.a.params:
    param1: 1
    param2: foo
\endcode

For sparta::Parameter's which are vectors (or nested vectors), YAML in-line sequence syntax can be used
to represent the value.

\code
top.a.params:
    one_dimension_vec_param: [1,2,3,4,5]  # This can be read by a 1-dimensional vector parameter
    two_dimension_vec_param: [[1],[2,3,4]]  # This can be read by a 2-dimensional vector parameter
\endcode

It is also important that YAML keys and values cannot begin with '*' or '?' and must not contain
'#', ':', '{', or '}' characters without putting the entire string in quotes.

\section param_format_nesting Nesting

It is often useful to nest configuration files. For example if a simualtor contains repeating
hierarchies, one configuration file can be used for each level. Using the reserved "<b>include</b>"
key allows a configuration file to specify that another configuration file should be applied at that
context. At this time, the second configuration file is expected to be a relative path to the
currently-parsed config file.

Consider
\code
# top_a.yaml
top:
    a.params:
        a_param_1: 1
        a_param_2: 2
    b:
      include: b.yaml # Applies b.yaml configuration in this context (top.b)
\endcode

\code
# b.yaml
# To be applied at top.b
params:
    b_param_1: 1 # Assigns "1" to top.b.params.b_param_1
    b_param_2: 2
\endcode

Note that "#include" or "include" can be used as the key. If the former is used, double-quotes are
required to prevent it from being interpreted as YAML comment.

\section param_format_attributes Parameter Assignment Attributes and Optional Parameters

Attributes can be assigned to parameters specified in configuration files that
dictate how those parameters values are applied.

Following normal configuration file syntax, parameter-assignment attributes can be attached to a
parameter using a value "<ATTRIBUTE>" (where ATTRIUTE is some meaningful attribute name) as if it
were a typical value being assigned to a parameter. These attribute names are <b>case-sensitive</b>.
\code
top.foo.params:
  myparam: <ATTRIBUTE>
\endcode

Parameter paths which include wildcards are welcome. Additional value-assignments of parameters
following the attribute assignment may modify or remove the attribute depending on the specific
attribute.

<b>Optional Parameters</b><br/>
A parameter specified in a configuration file can be modified so that the simulator suppresses the
error that would otherwise occur if the node referred to did not actually exist in the simulated
device tree.

By assigning "<OPTIONAL>" as a value for the chosen parameter(s), the user prevent errors if
that/those parameters are missing from the simulation tree.

\code
top.foo.params:
  param_that_does_not_exist: 12345
  param_that_does_not_exist: <OPTIONAL>
\endcode

With this, a user can inherit from other configuration files even if those files
contain parameters which do not always exist or do not ever exist in the simulated
tree for which the inheriting configuration file is intended. For example, if a configuration file
for version 1 of some system included a subcomponent that version 2 did not have, version 2 could
still inherit from configuration files for version 1 by marking certain inapplicable parameters
from the version 1 file as optional.
\code
top:
  core*:
    version_1_component:
      params:
        "*": <OPTIONAL>
\endcode

This can also be used to generalize parameter files by assigning values using overly-broad paths
using wildcards and and then mark a few exceptions to the the pattern as optional.

\section param_format_examples Examples

This example shows a variety of ways parameters can be set in a configuration file:
\code
"// Sparta cfg file comment": "value of comment“ # Eventually, comments like these may be reprocuded in config file output a Sparta simulator
"//a.params.param1": 1 # Interpreted as commented line
"//": "this is a test device tree configuration file"
top:
    a:
        params:
            param1: 12
top.a:
    "b":
    {
        "params.param1": 56,
        "params": {
            "#include" : "test_other.yaml"
        }
    }
    params:
        "param5":   [1.0, 1.1, 2, 3, 5.5]
        "param6": "0xdeadbeef"
        param7: "070"
        param8: [0xa1, 0xb2, 0xc3]

# This is a comment that will be lost
"top.a.params.param2": 34
"top.a.params.param3": [5,6,7,8]
top.a.params.param9 : string with spaces
"// block comment":
{
    # This is all ignored because the key associated with this mapping begins with "//"
    "a.paramsnonexistant_param": false,
    "b": {
        params.nonexistant_param": false
    }
}
\endcode

It is up to the user to identify the types of each parameter. This can most easily done by
automatically generating a configuration file containing the simulator's default configuration and
then modifying this file. See \ref ctrl_cfg_parameters

\page report_def_format Report Definition Format (.rrep,.yaml)
\tableofcontents
This page describes the grammar and usage of a report definition file

Until this page is complete, additional (somewhat dated) documentation on reports can be found: TBD

\section report_def_overveiew Overview
Report definitions are YAML files which describe to the sparta simulation framework how to construct
the content of a report from a given context in a Sparta device tree. Specifically, the report
definition defines exactly what counters and statistics are added to a report and how they are named
in the report.

Report definitions do not contain information about report duration or context.

\par Important:
<b>A report definition can affect only the contents of the report that is instantiated based on that
definition. Reprot definitions have no impact on any instrumentation in the simulation and cannot
change the behavior of the simulation proper under any circumstance. It it does, it should be
considered a bug. Reports will never affect other reports either - they are entirely passive</b>

Report definitions do not directly dictate how or to what file the report is finally rendered.
Report definitions only modify report content, which has the sole purpose of observing the
simulation instrumentation and collecting results. The resposibility of rendering the report content
and any values collected to a file, files, or database(s) is left entirely to Report Formatters.
See \ref report_gen. Report formatters can obviously use the content of a report to determine
output. Report definitions can also contain some style hints which the report formatter may choose
to interpret (see \ref report_def_style)

In this way, the same report definition can be used to generate text, csv, python, json, and html
output.

\section report_def Implementation
This is implemented in \ref Report.cpp, specifically in the \ref sparta::ReportFileParserYAML class

\section report_def_structure Structure
The report definition is a YAML file consisting of nested dictionaries which specify scope in the
Sparta device tree on which the report is being constructed.

Report definitions respect the <a href="http://www.yaml.org/spec/1.2/spec.html">YAML 1.2
specification</a> though only a subset is used by the report definiton parser


These reports begin with some optional fields which are represented as YAML key-value pairs.
Comments in YAML are started with a '#' character. These can begin at any line and follow other text
on any line.
\code
name: MyReport # Name of report (optional)
author: Me # Author of the report (optional)
\endcode

Following these pairs usually comes the content section. A YAML dictionary key whose associated
value is yet another nested dictionary is said to be a 'section' or 'block' for the purposes of this
documentation when that key is a reserved word (e.g. content, subreport, autopopulate). All fields
in a report must be specified within a content block.


\code
content: # Begin a report content section. No more report meta-data below this point (except in subreports)
\endcode

<em>In the implementation of the YAML report definition parser, scope qualifiers and the content
section can be intermixed and the order is not really important as long as all report fields are
specified within a content section.
</em>

\subsection report_def_field Report Fields
To resolve amgiguity between the multiple meanings of "statistics", reports will be said to contain
a number of ordered, named "Fields" where each field will retrieve its current value from a counter,
statistic, or expression referencing the former and a number of simple (cmath) (1)
sparta::CounterBase, (2) StatisticDef. The name of each field is specified in the report as a string,
optionally containing \ref report_def_field_name_variables

These field names and expressions are part of the report only and have no impact on any
instrumentation in the simulation under any circumstances.

Field names within a report must be unique. However, \ref report_def_subreports can be used to get
around this restriction. \ref report_def_field_name_variables help accomplish this.

<em>The code in \ref sparta::Report refers to it's report fields as "statistics" because it makes sense
within the scope of that code. Fom an end-user perspective, it is less confusing toe call them
fields.</em>

Report fields can be added using a report definition using either \ref report_def_field_declaration
or \ref report_def_autopop.

\subsection report_def_example Example Report Definition
\code
# Example Report.
# Instantiate from global scope ("")
#
name: "Example Report"
style:
    decimal_places: 2
content:
    top: # Changes scope to TOP
        subreport:
            name: Automatic Summary
            style:
                show_descriptions: true
            content:
                autopopulate:
                    attributes: vis:summary
                    max_report_depth: 1
        subreport:
            name: Misc Stats
            content:
                core0.foo.stats.bar : BAR 0
                core1.foo.stats.bar : BAR 1
                core*.foo.stats.bin : BIN %1
                core0:
                    foo.stats:
                        buz : "BUZ 0"
\endcode

Assume a device tree which looks like this:
\verbatim
- top
  - core0
    - foo
      - stats
        - bar (statistic, SUMMARY visibility)
        - bin (statistic)
        - buz (statistic)
  - core1
    - foo
      - stats
        - bar (statistic, SUMMARY visibility)
        - bin (statistic)
        - buz (statistic)
\endverbatim

The report above would be called "Example Report" and every field in every subreport would be
formatted to 2 decimal places (see \ref report_def_style).

Note the "top:" line just within the highest content section. This is a scope qualifier (see
\ref report_def_scope_qual) which tells the report parser that any node names or node name patterns
nested within the dictionaries associated with that "top:" section will be resolved within the scope
of "top". For example, "core0.foo" would resolve to "top.core0.foo" within that block.

A subreport called "Automatic Summary" would be created and would be populated by all counters/stats
below the top-level "top" node which were created with "SUMMARY"
(sparta::InstrumentationNode::Visibility::VIS_SUMMARY) level visibility. See \ref report_def_autopop).
The fields added by autopopulation will be given unique names. This is typically accomplished by
creating a nested subreport for each node below the place where the autopopulation was performed
("top" in this case). However, because the max_report_depth was set to 1 for this autopopulate
block, only 1 level of subreports will be created based on the child nodes seen (core0 and core1 in
this case). Each with summary-level visibility (top.core0.foo.stats.bar and top.core1.foo.stats.bar
in this example) will be added to the appropriate subreports with names relative to that subreport.
Therefore the "Automatic Summary" subreport will contain 2 subreports each containing 1 field. So
following the end of the first subreport section, the report content is:
\verbatim
Report "Example Report"
  Subreport "Autmatic Summary"
    Subreport core0
      Field "foo.stats.bar" -> top.core0.foo.stats.bar
    Subreport core1
      Field "foo.stats.bar" -> top.core1.foo.stats.bar
\endverbatim

<b>Note that this is not a real rendering of the report, but just a depiction of the current
structure of the report. The actual rendering of the report is totally dependant on the report
output formatter used to render the report (sparta::report::format).</b>

Because the "show_descriptions" style was set, if this report were rendered with the html formatter
(or any other formatter that recognizes the show_descriptions style) then descriptions for each
field in the "Automatic Summary" section would be shown beside each report field

A second subreport of the "Example Report" would be created and called "Misc Stats".
This second subreport would contain 5 fields as specified in the content section of that report.

The first two stats come from the lines
\verbatim
                core0.foo.stats.bar : BAR 0
                core1.foo.stats.bar : BAR 1
\endverbatim
These are explicit \ref report_def_field_declarations in the form of a leaf YAML key-value pair.
Each of these lines creates a new field in the current report/subreport with the given name to the
right of the ':'. This field points to the node (counter/stat) or expression
(\ref report_def_expressions) on the left side. The node referenced on the left is resolved relative
to the current scope ("top") in this case. The field name can be omitted and replaced with "" to
indicate it is an unnamed field. Report output formatters handle rendering unnamed fields
differently.

The third field declaration:
\verbatim
                core*.foo.stats.bin : BIN %1
\endverbatim
adds 2 fields to the report. This declaration contains a <b>wildcard</b> in the <u>node location</u>
as well as a <b>variable</b> in the field <u>name</u>.

The wildcard in the node location indicates that when resolving this location to an actual node
within the current scope ("top" in this case), proceed with any child having that name. In this
example, "core*" could be substituted with "core0" and "core1". This is a very primitive glob-like
pattern matching language

Since both "core0" and "core1" will be matched, this line will result in the addition of a field for
"top.core0.foo.stats.bin" as well as "top.core1.foo.stats.bin". See \ref report_def_scope_wildcards
for more detail on these wildcards

The substitutions made when pattern matching "core*" to "core0" and "core1" are available to the
field name on the right of the ':'. "%1" refers to the first (most recent) subsitution on the
substitution stack for the current context. When "core*" is matched to "core0", "%1" refers to "0"
and when "core*" is matched to "core1", "%1" refers to "1". The field names of the two nodes added
as a result of this line are "BIN 0" and "BIN 1". See \ref report_def_field_name_variables
for more detail on field name variables.

The final few lines of this content section are just nested scope qualifiers.
\verbatim
                core0:
                    foo.stats:
                        buz : "BUZ 0"
\endverbatim
"core0" just changes the current scope for anything in the nested dictionary associated with it.
Since the scope enclosing this node is "top", the scope inside this section is "top.core0". The
following line changes the scope to "top.core0.foo.stats". The third line is a field definition line
which simply creates a field named "BUZ 0: which points to the node "top.core0.foo.stats.buz".

It should be obvious from these lines that the current scope is a stack, and when the dictionary
associated with each of these lines ends, the scope is set back to what it was before the dictionary
was started. Any lines with the same indention as "core0" after these few line would have the scope
of "top" because they are not within the "core0" scope qualifier's associated dictionary.

The final report contents after parsing this entire report definition are:
\verbatim
Report "Example Report"
  Subreport "Autmatic Summary"
    Subreport core0
      Field "foo.stats.bar" -> top.core0.foo.stats.bar
    Subreport core1
      Field "foo.stats.bar" -> top.core1.foo.stats.bar
  Subreport "Misc Stats"
    Field "BAR 0" -> core0.foo.stats.bar
    Field "BAR 1" -> core1.foo.stats.bar
    Field "BIN 0" -> core0.foo.stats.bin
    Field "BIN 1" -> core1.foo.stats.bin
    Field "BUZ 0" -> core0.foo.stats.buz
\endverbatim

\subsection report_def_field_declarations Field Declarations
Field declarations are leaf key-value pairs in YAML files within a content section but outside of
some other block (e.g. autopopulate). These pairs each add one or more fields in the report (See
\ref report_def_field) and dictate how those fields get their values whenever the report is
rendered.

A field has the following signature:
\code
value_expression : field_name
\endcode
<em>value_expression</em> indicates how the field gets its value. This can be a node location
relative to the current enclosing scope which may contain wildcards. Alternatively, this may be a
statistical expression (sparta::statistics::expression::grammar::ExpressionGrammar). When interpreting
a report definition, an attempt is made to interpret this as a node location. If the string is not
a properly formed node location string (alphanumeric with underscores and dot-separators) or if it
does not resolve to any nodes in the simulation's device tree then it will be interpreted as an
expression (See \ref report_def_expressions). If it is not a valid expression, an exception is
thrown.

<em>field_name</em> names the field. If the left side of a field declaration or any enclosing scope
node (see \ref report_def_scope_qual) contains wildcards, then this name should contain a variable
as explained in \ref report_def_field_name_variables.

See \ref report_def_example for some example usages

\subsection report_def_field_name_variables Field Name Variables
The wildcards contained in \ref report_def_scope_qual and \ref report_def_field_declarations node
paths allow a number of nodes having similar paths matching a given pattern to be added to a report
in a single line in the report definition. However, this functionality can often cause report field
name collisions. For example, the following line will always cause a report field name collision
(and cause an exception to be thrown) if there is more than one matching node.
\code
top.core*.stats.foo : foo_field
\endcode
The report being built may allow a field named "foo_field" to be added referring to
"top.core0.stats.foo". If the pattern above also matches another node, say "top.core1.stats.foo",
then it will attempt to add a field named "foo_field" to the report AGAIN for the next pattern
match. This will result in an exception being thrown.

To avoid such name collisions, variables can be used in the report field name. Consider the
following tree:
\verbatim
top
  - core0
    - stats
      - foo0
      - foo1
  - core1
    - stats
      - foo0
      - foo1
\endverbatim

And the following example report definition:
\verbatim
content:
  top:
    core*.stats:
      foo* : "My Core%1 Foo%2 Stat"
      # OR: foo* : "My Core%-2 Foo%-1 Stat"
\endverbatim

In this example, we see wildcards in a <b>scope qualifier</b> line ("core*.stats") and in a
<b>report field definition</b> node location. After evaluating the "core*.stats" line with the
example simulation tree shown above, the report definition interpreter will be tracking the contexts
{"top.core0.stats", "top.core1.stats"}. It will also be tracking a stack of substitutions which can
later be referenced by the report field name in variables. At this point, the stack of replacements
for each context being tracked looks like:
\verbatim
context "top.core0.stats" replacements_stack = ["0"]
context "top.core1.stats" replacements_stack = ["1"]
\endverbatim

When the node location "foo*" portion of the report field declaration line is encountered, the
interpreter evaluates the locations for each tracked context. The resulting set of contexts being
tracked is {"top.core0.stats.foo0", "top.core0.stats.foo1", "top.core1.stats.foo0",
"top.core1.stats.foo1"}. Each of these new contexts inherits the replacements stack from the context
from which the pattern was matched. This results in a new set of replacement stacks being tracked
\verbatim
context "top.core0.stats.foo0" replacements_stack = ["0", "0"] <- top of stack
context "top.core0.stats.foo1" replacements_stack = ["0", "1"] <- top of stack
context "top.core1.stats.foo0" replacements_stack = ["1", "0"] <- top of stack
context "top.core1.stats.foo1" replacements_stack = ["1", "1"] <- top of stack
\endverbatim

At this point, the interpreter found 4 nodes refering to the given report field declaration and must
create 4 report fields: one for each of the current contexts. Variables in the report field name can
refer to the contents of the replacements stack for the context for which each field is being added.

\%<em>X</em> refers to a position from the top of the replacements stack <em>X</em>-1. \%1 refers to
the top of the stack, \%2 to the second from the top, and so on. In this example, \%1 is the "foo"
number and \%2 is the "core" number. \%0 refers to the fully-qualified context. \%-<em>X</em>
indexes the replacements stack for the current context in reverse.

\%-1 refers to the least recent substitution made in the current context, \%-2  to the second least
recent and so on. Referring to replacements in this way is less flexible since a report definition
that uses these variables may be moved to a new scope or included (see \ref report_def_includes)
inside another report definition unexpectedly. If the containing report definition uses wildcards
to resolve its tree scope, it will change the values see in \%-<em>x</em> variables. Therefore,
this is discouraged.

Alternatively, one can totally omit the report field name as in:
\code
top.core*.stats.foo : ""
\endcode
This is generally less desirable as it relies on the report output formatter to display a useful
name when showing this field, which may not be the case depending on how the report is rendered.


\subsection report_def_subreports Subreports
A single report can contain multiple subreports to better organize its content.

\subsection report_def_reserved_nodes Reserved Nodes

\subsection report_def_scope_qual Node Scope Qualifiers
\subsection report_def_scope_wildcards scope Wildcards

Wildcards can be inserted into Node Scope qualifiers to simultaneously descend subtrees within the
sparta device tree. This is useful when there are mutliple instantiations of a simular component (e.g.
multiple cores). To see the same statistic across each core, one could supply a node location
containing a wildcard like so:
\code
top.core*.foo.bar.stats.mystat
\endcode

As long as this location was evaluated from the global scope (higher than top), it would find every
"mystat" matching this patttern. If the simulation had 12 core instances ("core0" through "core11")
which each had identical subtrees, this would find 12 instances of mystat.

If each "core*" in this hypothetical system contained different subtrees and some did NOT have a
"mystat" statistic as indicated by the path, the found set would contain fewer than 12 results.
<b>When interpreting a report definition file this is not a problem <u>as long as at least 1 node
can be found matching this pattern.</u></b>

These wildcards are part of a very limited glob-like pattern matching language. There is no limit to
the number of wildcards that can be used in a single string. The following wildcards are supported:

Wildcard  | Meaning
--------- | --------
\*        | Any number of characters
\+        | One or more characters
?         | Zero or One character

When evaluating an tree location with wildcards, the substitutions for each match are tracked.
These substitutions can be accessed through variables in report field declarations. See
\ref report_def_field_name_variables. Even the substitutions in enclosing scope qualifiers (on other
lines) are accessible.

\subsection report_def_expressions Statistical Expressions
Expressions can be used instead of a statistic/counter name when defining report fields (as in
\ref report_def_field). These are arithmetic expressions supporting most some operators and tokens:
+, -, *, /, **, (, ), and -unary. Thes expressions support references to other counters and stats,
a number of builtin constants, simulation variables, and functions of various arities. Other
counters/stats can be referenced relative to the current context in the report def just as simple
named counters/stats are referenced in basic report entries.

Constant         | Value
---------        | ------
c_pi             | boost::math::constants::pi<double>()
c_root_pi        | boost::math::constants::root_pi<double>()
c_root_half_pi   | boost::math::constants::root_half_pi<double>()
c_root_two_pi    | boost::math::constants::root_two_pi<double>()
c_root_ln_four   | boost::math::constants::root_ln_four<double>()
c_e              | boost::math::constants::e<double>()
c_half           | boost::math::constants::half<double>()
c_euler          | boost::math::constants::euler<double>()
c_root_two       | boost::math::constants::root_two<double>()
c_ln_two         | boost::math::constants::ln_two<double>()
c_ln_ln_two      | boost::math::constants::ln_ln_two<double>()
c_third          | boost::math::constants::third<double>()
c_twothirds      | boost::math::constants::twothirds<double>()
c_pi_minus_three | boost::math::constants::pi_minus_three<double>()
c_four_minus_pi  | boost::math::constants::four_minus_pi<double>()
c_nan            | NAN
c_inf            | INFINITY

Variable       | Value
---------      | ------
g_ticks        | (singleton) Scheduler ticks
g_seconds      | (singleton) Scheduler simulated seconds elapsed
g_milliseconds | (singleton) Scheduler simulated milliseconds elapsed
g_microseconds | (singleton) Scheduler simulated microseconds elapsed
g_nanoseconds  | (singleton) Scheduler simulated nanoseconds elapsed
g_picoseconds  | (singleton) Scheduler simulated picoseconds elapsed

Unary Function | Implementation
-------------- | --------------
abs(x)         | std::fabs(x) <em>(abs in stat expressions maps to fabs</em>)
fabs(x)        | std::fabs(x)
acos(x)        | std::acos(x)
asin(x)        | std::asin(x)
atan(x)        | std::atan(x)
ceil(x)        | std::ceil(x)
trunc(x)       | std::trunc(x)
round(x)       | std::round(x)
cos(x)         | std::cos(x)
cosh(x)        | std::cosh(x)
exp(x)         | std::exp(x)
exp2(x)        | std::exp2(x)
floor(x)       | std::floor(x)
ln(x)          | std::log(x)
log2(x)        | std::log2(x)
log10(x)       | std::log10(x)
sin(x)         | std::sin(x)
sinh(x)        | std::sinh(x)
sqrt(x)        | std::sqrt(x)
cbrt(x)        | std::cbrt(x)
tan(x)         | std::tan(x)
tanh(x)        | std::tanh(x)
isnan(x)       | std::isnan(x)
isinf(x)       | std::isinf(x)
signbit(x)     | std::signbit(x)
logb(x)        | std::logb(x)
erf(x)         | std::erf(x)
erfc(x)        | std::erfc(x)
lgamma(x)      | std::lgamma(x)
tgamma(x)      | std::tgamma(x)


Binary Function  | Implementation
---------------- | --------------
pow(a,b)         | std::pow(a, b)
atan2(a,b)       | std::atan2(a, b)
min(a,b)         | std::min<double>(a, b)
max(a,b)         | std::max<double>(a, b)
fmod(a,b)        | std::fmod(a, b)
remainder(a,b)   | std::remainder(a, b)
hypot(a,b)       | std::hypot(a, b)
ifnan(a,b)       | (std::isnan(a) or std::isinf(a)) ? b : a

Ternary Function | Implementation
---------------- | --------------
cond(a,b,c)      | a ? b : c

<b style='color:#ff0000;'>WARNING: Expressions inside a (YAML) report definition cannot begin with a '*' character unless fully
enclosed in double-quotes</b>.<br/>This is because a YAML scalar cannot begin with an asterisk

See sparta::statistics::expression::grammar::ExpressionGrammar for implementation of this expression grammar

\subsection report_def_includes Include Directives
\todo Write this section

\subsection report_def_style Style section
The style section of a report is a dictionary associated with a 'style' keyword <b>outside the
content section</b> of a report. The style section contains style hints that some output formatters
will interpret.

To see a full list of the style hints and default behavior, look at documentation for each report
output formatter in sparta::report::format.

A few of the availsble style options include
Style                | Effect  | Supported Output Formatter
------               | ------- | ---------------------------
decimal_places       | Number of digits after the decimal place for non-integer values | html, json
collapsible_children | When rendering HTML output, children can be dynamically collapsed via interactive javascript | html
num_stat_columns     | Number of statistic columns for HTML output. Can be used to make reports more dense| html
show_descriptions    | Show a description next to each report value in HTML output | html

\subsection report_def_autopop Autopopulation Blocks
Within a content section, the key "autopopulate" indicates that a number of fields will be added to
the report automatically based on some criteria.

Autopopulate can be used in two forms: as a single, concise key-value pair and as a nested
dictionary with multiple detailed options.

When used concisely, the autopopulate key is followed by a value that is a filter expression. This
simple filtering language filters nodes based on their visibility semantics. It is explained below.

\code
content:
    # other content
    # ...
    autopopulate: "!=vis:hidden && !=vis:summary"
\endcode

The more verbose usage
\code
content:
    # other content
    # ...
    autopopulate:
        attributes: vis:summary
        max_report_depth: 0  # Stops making subreports at depth N. 0 means no subreports
        max_recursion_depth: -1 # Never stop recursion of the sparta tree

\endcode

<h4>Tree Filtering Expressions</h4>
Tree filter expressions use a simple custom grammar for accepting or rejecting an instrumentation
node in a sparta tree based on its attributes and visibility semantics. See
\ref sparta::InstrumentationNode.

Instrumentation nodes have a visiblity value in the range of sparta::InstrumentationNode::VIS_HIDDEN
(0) to sparta::InstrumentationNode::VIS_MAX. A few common values in the range are contained in the
sparta::InstrumentationNode::Visibility enum.

<h4>Visibility Filtering</h4>
Tree filtering expressions can filter for this visibility level. To accept only nodes with
visibility of sparta::InstrumentationNode::VIS_NORMAL or higher, use:
\code
">=vis:normal"
\endcode

Visibility filtering is always in the form<br />
&nbsp;&nbsp;&nbsp;&nbsp;<em>\<visibility_comparison></em>vis:<em>\<visibility_value></em>

To require visibility be anything but sparta::InstrumentationNode::VIS_HIDDEN, use
\code
"!=vis:hidden"
\endcode

Visibility can also be an integer.
\code
"<vis:100"
\endcode

Grammar constants for visibility include (see sparta::InstrumentationNode::Visibility)
\li summary
\li normal
\li detail
\li support
\li hidden

Visibility Comparison Operators are (in no particular order):
\li "=="
\li ">="
\li "<="
\li "<"
\li ">"
\li "!="

The "==" comparison is implicitly used if no visibility comparison operator is chosen

<h4>Type filtering</h4>
Filtering can be performed based on node type attributes. For example, counters can be rejected.
\code
"!=type:counter"
\endcode

Type filtering is always in the form<br />
&nbsp;&nbsp;&nbsp;&nbsp;<em>\<type_comparison></em>type:<em>\<type_name></em>

Type Comparison Operators are (in no paticualr order):
\li "=="
\li "!="

The "==" comparison is implicitly used if no type comparison operator is chosen

Grammar constants or type include
\li statistic, statisticdef, stat, statdef
\li counter
\li parameter, param (<b>Currently unsupported in Reports</b>)
\li histogram (<b>Currently unsupported in Reports</b>)

<h4>Name and Tag filtering</h4>

Similarly to type and visiblity, nodes can be filtered by their local name and tag-set. These
attributes do not support comparison using relative operators (&lt;, &gt;, etc.). ==, != and regex
operators are supported. The 'regex' operator attempts to match a given regex pattern with the name
of name of the node or any tag of the node depending on how it is invoked.

Type Comparison Operators are (in no paticualr order):
\li "=="
\li "!="
\li "regex"

Some example expression to filter by a name might be
\code
"name:node_i_am_looking_for"
"!=name:name_of_an_inaccurate_counter"
"regex name:ctr_foo_.*" # Accept counters whose names have a ctr_foo_ prefix
\endcode

Tag filtering is similar to name filtering, but the comparison operators have
semantics that apply to the whole tag set. The truth table is
Name           | Required for  "true" evaluation
----------     | -------------------------------
"=="           | Any tag matches comparison string
"!="           | No tag matches comparison string
"regex"        | Any tag matches regular expression pattern

There is no !regex operator. Instead the inversion operators "!" and "not"
can be used after a regex operation. Refer to the next section.

Some example expression to filter by a tag might be
\code
"tag:power"
"!=name:dummy_.*"
"regex tag:power_.*" # Accept counters having tags beginning with "power_"
\endcode

<h4>Compound filtering</h4>

Visibility and type filtering can be combined in to the same expression with logical operators. Just
as in C, these operators are more loosely bound than any other operators (with a lower number
indicating looser binding)

One could filter for only statistics (not counters) which have "summary" level visibility, a tag
indicating they are 'power'-related stats, and a name that does not contain the string 'fiz'
\code
"vis:summary && type:stat && tag:power && not regex name:.*fiz.*"
\endcode

More complex filters can be created using parentheses. This expression accepts statistics with
"summary" visibility OR counters with "hidden visibility"
\code
"(vis:summary && type:stat) || (vis:hidden && type:counter)"
\endcode

<h4>Logical Operators include</h4>
<ol>
<li> "^^" </li>
<li> "||" </li>
<li> "&&" </li>
<li> "!" </li>
<li> "not" </li>
</ol>

<h4>Parentheses are, of course, supported.</h4>

The grammar is fully defined and implemented \ref sparta::tree::filter::grammar::Grammar

<em>Often, these expressions contain characters not accepted by YAML and must be written in quotes.</em>

\section report_def_var_name_ambiguity Field Declaration Path/Expression Ambiguity
Because some reserved words in the report definition grammar may be the same as nodes in the sparta
tree, ambiguity can be created.

By default, any report field names are assumed to be node names unless a node by that name does not
exist in the current scope. Then, the report definition parser attempts to interpret the field name
as an expression.

For example, the folling tree can present problems when trying to look at the cycles <b>variable</b>
 (not the top.cpu1.stats.cycles node) on the st0 or st1 nodes.
\verbatim
top
  - cpu0
  - cpu1
    - st0
    - st1
    - stats
      - cycles
\endverbatim

The following will only find the nodes "top.cpu1.stats.cycles" and add them to the report.
\code
content:
  top.mss.cpu*.st*:
    cycles: "%l"
\endcode

To use the "cycles" variable to get the number of cycles of the clock on the top.cpu1.st0 and st1
nodes, the following definition could be used.
\code
content:
  top.mss.cpu*.st?:
    cycles: "Clock for st%1 cpu%2"
\endcode

Resolving ambiguity of node names vs. statistic expression variables is not explicitly supported in
the language. One must be clever about either naming tree nodes more specifically or using wildcards
that specifically

<em>It is an eventual goal to add full regular expression support instead of glob-like pattern
matching. This should allow the user to define pattern-matching node-scope strings that eliminate
all ambiguity (if pattern matching must be used)</em>


\section report_def_directive_ref Report Definition Directive and Option Reference
Directive           | Report Definition File Context  | Semantic
----------          | ------------------------------- | ---------
name                | Immediate child of a subreport section or at the top-level of a report definition | Name of the report (for output formatters that display a title)
author              | Immediate child of a subreport section or at the top-level of a report definition | Author of the report (for output formatters that display an author)
style               | Immediate child of a subreport section or at the top-level of a report definition | Begins a style section where key-value pairs can be used to specify style. Styles are output-formatter-specific. See \ref report_def_style
content             | Immediate child of a subreport section or at the top-level of a report definition | Begins a content section. Parser is considered to be in this content section until it enters a subrerport or exits the dictionary associated with the 'content' key
subreport           | Within a 'content' section more recently than the nearest parent subreport section | Begins a subreport of the most recent subreport (or top-level report if no subreports specified). See \ref report_def_subreports. This should be considered as "ending" the current content section until this particular content section ends.
include             | Within a 'content' section more recently than the nearest parent subreport section | Includes another report definition file at the <b>current node context</b>
autopopulate        | Within a 'content' section more recently than nearest parent subreport section | Specifies autopopulation of report fields based on some filter expression and other options
attributes          | Immediately within an 'autopopulate' block | Specifies the attribute filter expression for autopopulation. See \ref report_def_autopop
max_recursion_depth | Immediately within an 'autopopulate' block | Specifies the maximum recursion depth when autopopulating. This prevents autopopulation from recursing any deeper than N children. If 0, only looks at node(s) indicated by current scope and never looks at children. Defaults to -1 (no recursion limit)
max_report_depth    | Immediately within an 'autopopulate' block | Specifies the depth of nested subreports to create. If 0, all fields will be added to the top level report. This may cause name collisions which cause errors when instantiating the report. Defualts to -1 which means no limit


\section report_def_lims Limitations of Report Definitions
\li Cannot specify absolute paths of objects. All report content is relative to the context within which the report is constructed. Repo
\li Does not allow depth-first traversal of report context specifiers (i.e. cannot visit all children of one substitution of a report context qualifier with a wildcard before moving on to the next substitution for that wildcard)
\li Cannot always resolve ambiguity between node names and variables in statistic expressions

\todo Complete This Page

\page report_out_format Report Output Formats
This page describes the various output formats of a sparta report

<em>For an up-to-date list, run a Sparta simulation with the flag:</em><b><pre>"--help-topic reporting"</pre></b>

\todo Complete this page
\section report_out_format_plaintext Plaintext (.txt, .text)
sparta::report::format::Text<br/>

Writes report output as plain-text output. This is human readable and regex-parsable.<br/>

<em>See formatter class implementation for more details.</em>
\section report_out_format_csv CSV (.csv)
sparta::report::format::CSV<br/>

Writes report output as comma-separated values with 1 or 2 header rows and then 1 or more data rows
depending on whether periodic data was collected (see \ref report_gen).<br/>

Consider plotting with sparta/tools/plot_csv_report.py

<em>See formatter class implementation for more details.</em>
\section report_out_format_basichtml BasicHTML (.html, .htm)
sparta::report::format::BasicHTML<br/>

Writes report output a hierarchical HTML page with nested tables. This format
respects many report style options.<br/>

<em>See formatter class implementation for more details.</em>
\section report_out_format_gnuplot Gnuplot (.gnuplot, .gplt)
sparta::report::format::Gnuplot<br/>

Writes report output in a format suitable for gnuplot<br/>

<em>See formatter class implementation for more details.</em>
\section report_out_format_pythondict PythonDict (.python, .python)
sparta::report::format::PythonDict<br/>

Writes report output in a Python dictionary format. This is a string representation of a dictionary
which can be read as a multi-line string from the file and "eval"ed to load. Alternatively, it can
be copied into python code.<br/>

<em>See formatter class implementation for more details.</em>
\section report_out_format_json JavascriptObject (.json)
sparta::report::format::JavascriptObject<br/>

Writes report output in the JSON format which can be read by a number of libraries.<br/>

<em>See formatter class implementation for more details.</em>

\page pipeout_format Pipeline Collection Format (index.bin, location.dat, record.bin, simulation.info, clock.dat)
This page describes the various file formats of pipeline collection databases
\todo Write this page
\page checkpoint_format Checkpointer Format
This page describes the file format used by the sparta fast checkpointing mechanism
\todo Write this page
\page log_out_format Logging Output formats (.log, .log.raw, .log.basic, log.verbose)
This page describes the various file formats with which log files can be written
\todo Write this page
 */
