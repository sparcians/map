
/*!
  \page core_example Core Example Using Sparta

  The Sparta core example is located in sparta/example/CoreModel.

  ======================================================================
  \section example_core Example Core Layout

  The CoreExample is an example Sparta simulator that uses the Sparta
  framework to mimic a very rudimentary model of a simple out-of-order
  core.  The model does not have dependency checking nor actually
  renames any instructions.  The "ISA" is uses is a few random
  instructions pulled from the PowerPC ISA listed in a table in
  Fetch.cpp.

  The example touches on Sparta command line simulation, construction
  phasing, unit creation, port creation and binding, event creation,
  and data transfer.

  The ExampleCore has a simple pipeline:

  \dot
  digraph pipeline {
     rankdir=LR
     node [shape=record, fontname=Helvetica, fontsize=10];
     Fetch [ URL="\ref Fetch.hpp"];
     Decode [ URL="\ref Decode.hpp"];
     Rename [ URL="\ref Rename.hpp"];
     Dispatch [ URL="\ref Dispatch.hpp"];
     FPU [ URL="\ref Execute.hpp"];
     ALU0 [ URL="\ref Execute.hpp"];
     ALU1 [ URL="\ref Execute.hpp"];
     BR [ URL="\ref Execute.hpp"];
     LSU [ URL="\ref LSU.hpp"];
     BIU [ URL="\ref BIU.hpp"];
     MSS [ URL="\ref MSS.hpp"];
     ROB [ URL="\ref ROB.hpp"];
     Fetch -> Decode [arrowhead="open", style="solid", label="InstGroup", URL="\ref CoreTypes.hpp"];
     Decode -> Rename [arrowhead="open", style="solid", label="InstGroup", URL="\ref CoreTypes.hpp"];
     Rename -> Dispatch [arrowhead="open", style="solid", label="InstGroup", URL="\ref CoreTypes.hpp"];
     Dispatch -> ROB [arrowhead="open", style="solid", label="InstGroup", URL="\ref CoreTypes.hpp"];
     Dispatch -> FPU [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     Dispatch -> ALU0 [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     Dispatch -> ALU1 [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     Dispatch -> LSU  [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     Dispatch -> BR   [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     LSU      -> BIU  [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];
     BIU      -> MSS  [arrowhead="open", style="solid", label="Inst", URL="\ref ExampleInst.hpp"];

     Decode -> Fetch [arrowhead="open", style="dotted", label="credits"];
     Rename -> Decode [arrowhead="open", style="dotted", label="credits"];
     Dispatch -> Rename [arrowhead="open", style="dotted", label="credits"];
     ROB  -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     FPU  -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     ALU0 -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     ALU1 -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     BR   -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     LSU  -> Dispatch [arrowhead="open", style="dotted", label="credits"];
     BIU  -> LSU [arrowhead="open", style="dotted", label="ack"];
     MSS  -> BIU [arrowhead="open", style="dotted", label="ack"];
  }
  \enddot

  This heirarchy is built in
  core_example::ExampleSimulator::buildTree_ pulling resources and
  factories from core_example::CoreTopology_1.

  Instructions are created in `Fetch` and progression of these
  instructions is based on a credit system.  Credits flow from `ROB`
  through `Dispatch` though `Rename`, etc. in 0 time (dotted lines) while
  instructions flow from `Fetch` to `Decode` to `Rename`, etc (solid lines)
  in a delay of 1 cycle.  This flow is controlled based on the rule
  (adopted by this example) that all `DataInPorts<core_example::InstGroup>` take
  objects delayed by 1 cycle (set at instantiation time) and all
  credit `DataInPorts<uint32_t>` are assumed to be 0 cycle.  For example:

  \dot
  digraph fetch_decode {
      rankdir=LR
      labeljust="l"
      node [shape=record, fontname=Courier, fontsize=10];
      Fetch [ label="{\N\l|+ DataInPort\<CreditType\>  in_fetch_queue_credits, delay \=\= 0\l+ DataOutPort\<InstGroup\> out_fetch_queue_write, delay \=\= 0\l}"]
      Decode [ label="{\N\l|+ DataOutPort\<CreditType\> out_fetch_queue_credits, delay == 0\l+ DataInPort\<InstGroup\>   in_fetch_queue_write,    delay == 1\l}"]
      Fetch -> Decode [dir="both", arrowhead="open", arrowtail="open", style="solid"];
  }
  \enddot

  These ports are bound in core_example::ExampleSimulator::bindTree_,
  using the `port_connections` list defined in
  core_example::CoreTopology_1.  The ports themselves are instantiated
  in Fetch.cpp / Fetch.hpp and Decode.cpp / Decode.hpp.

  Without the delay of 1 cycle on the `in_fetch_queue_write`, we will come
  across a cycle in simulation:

  \code
  Step.Time
     1.0    Fetch receive credits
     2.0    Fetch sends new instructions on the out port
     3.0    Decode would receive new instructions and process
     4.0    Decode send credits back to Fetch
     5.0    Goto 1?? Oops! Infinite loop!
  \endcode

  Adding the cycle delay:

  \code
  Step.Time
     1.0    Fetch receive credits
     2.0    Fetch sends new instructions on the out port
          ... delay
     1.1    Decode would receive new instructions and process
     2.1    Decode send credits back to Fetch
     3.1    Fetch receive credits
     4.1    Fetch sends new instructions on the out port
          ... delay, etc, etc
  \endcode

  The entire core example follows this paradigm.

  ======================================================================
  \section Building

  To build the CoreExample, simply type `make` in the
  `example/CoreModel` directory.

  Refer to builtin help for up-to-date help and commands, but try running
  the model with the '-h' option.

  \code
  % ./sparta_core_example -h
  % ./sparta_core_example --verbose-help
  % ./sparta_core_example --help-topic reporting
  \endcode

  Most of the commands listed in the help are directly from the Sparta
  framework and not `sparta_core_example`.  These are "free" commands
  the framework provides (via sparta::app::CommandLineSimulator).  The
  modeler is _not_ required to use this class for simulation.

  ======================================================================
  \section Invocations

  \subsection run_config_core_example Running/Configuring

  Dump out the parameters:

  \code
  % ./sparta_core_example --no-run \
                          --write-final-config-verbose params.yaml
  \endcode

  Read in parameters from a file and/or change via the command line.
  Parameters are set _in the order_ in which they are read from the
  command line.

  \code
  % ./sparta_core_example -c params.yaml -p top.cpu.core?.fetch.params.num_to_fetch 2

    [in] Configuration: Node "" <- file: "params.yaml"
    [in] Configuration: Parameter "top.cpu.core?.fetch.params.num_to_fetch" <- value: "2"
  \endcode

  Run for 1 million cycles, incrementing a ctr every 1000 cycles.
  Consume the config file test.yaml and produce a final yaml file called
  final.yaml.  Also, dump the tree:

  \code
  % ./sparta_core_example -r 1M \
                       --show-tree -p top.core0.params.ctr_incr_period 1000 \
                       -c test.yaml --write-final-config final.yaml
  \endcode

  \subsection debugging_logging_core_example Debugging/Logging

  Enable a Decode "info" log for 1,000 cycles.

  \code
  % ./sparta_core_example -r 1K -l top.cpu.core?.decode info decode.out
  \endcode

  Enable a Decode and Fetch "info" log for 1,000 cycles to the same file:

  \code
  % ./sparta_core_example -r 1K -l top.cpu.core?.decode info fetch_decode.out \
                                -l top.cpu.core?.fetch  info fetch_decode.out
  \endcode

  Enable a Decode and Fetch "info" log for 1K cycles _after_ the
  first 100K cycles:

  \code
  % ./sparta_core_example --debug-on 100K -r 101K \
                       -l top.cpu.core?.decode info fetch_decode.out \
                       -l top.cpu.core?.fetch  info fetch_decode.out
  \endcode

  Enable a Decode and Fetch "info" log for 1K instructions _after_ the
  first 100K instructions:

  \code
  % ./sparta_core_example --debug-on-icount 100K -i 101K \
                       -l top.cpu.core?.decode info fetch_decode.out \
                       -l top.cpu.core?.fetch  info fetch_decode.out
  \endcode

  \subsection reporting_core_example Generating Reports

  Refer to \ref report_def_format for more details on Reports.

  Dump out a full, simple report report in html:

  \code
  % ./sparta_core_example -r 1000000 --report-all output.html html
  % ./sparta_core_example -r 1M --report-all output.html html  # Same command
  \endcode

  Dump out a full, simple report with rules.  The content is all
  inclusive of the entire simulation tree (the "" on the command line)

  \code
  % cat simple_stats.yaml
  content:
  subreport:
    name: AUTO
    content:
      autopopulate: true
  % ./sparta_core_example -r 1M --report "" simple_stats.yaml my_report.txt text
  \endcode

  Dump out subset, but simple report with rules.  The content is
  inclusive of the decode tree only for all cores:

  \code
  % cat simple_stats.yaml
  content:
  subreport:
    name: AUTO
    content:
      autopopulate: true
  % ./sparta_core_example -r 1M --report "top.cpu.core?.decode" simple_stats.yaml my_report.txt text
  \endcode

  Create a `ts_report.yaml` file to dump out a time-series CSV report
  (must be run in `example/CoreModel` directory for the
  `core_stats.yaml` file).  This is triggered on the stat
  `total_number_retired` reaching 3500 insts or greater.  It's trigged
  "on" only once.

  \code
  % cat > ts_report.yaml
  content:

  report:
    pattern:   top.cpu.core0
    def_file:  core_stats.yaml
    dest_file: core0.csv
    format:    csv
    trigger:
      start:   rob.stats.total_number_retired >= 3500
      update-time: 5 ns
   <ctrl-D>
   % ./sparta_core_example -r 1M --report ts_report.yaml
   \endcode

  \subsection pipeouts_core_example Generating Pipeouts

  Refer to
  https://github.com/sparcians/map/tree/master/helios/pipeViewer for
  directions on building/installing the MAP::Helios::Argos tools
  preferably using the Conda tools.

  Create a 10K instructions Pipeout:

  \code
  % pwd
  $HOME/map/sparta/build/example/CoreModel
  % ./sparta_core_example -i10K -z my_pipeout
  \endcode

  This will create a set of files for the Argos viewer:

  \code
  % ls -1 my_pipeout*
  my_pipeoutclock.dat
  my_pipeoutdata.dat
  my_pipeoutdisplay_format.dat
  my_pipeoutindex.bin
  my_pipeoutlocation.dat
  my_pipeoutmap.dat
  my_pipeoutrecord.bin
  my_pipeoutsimulation.info
  my_pipeoutstring_map.dat
  \endcode

  Launch the MAP::Helio::Argos viewer (this assumes the correct
  tools/libaries have been installed).  Note that the `cpu_layout.alf`
  file is NOT in the build directory of the CoreModel.

  \code
  % pwd
  $HOME/map/sparta/build/example/CoreModel
  % python3 $HOME/map/helios/pipeViewer/pipe_view/argos.py \
         -l $HOME/map/sparta/example/CoreModel/cpu_layout.alf \
         -d my_pipeout

  \endcode

  The view that pops up is a _single-cycle_ view of the CPU.

*/
