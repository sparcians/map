
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

  To build the CoreExample, simply type 'make' in the
  example/CoreModel directory.

  Refer to builtin help for up-to-date help and commands, but try running
  the model with the '-h' option.

  \code
  ./bld-Linux_x86_64-gcc4.7/sparta_core_example -h
  ./bld-Linux_x86_64-gcc4.7/sparta_core_example --verbose-help
  \endcode

  ======================================================================
  \section Invocations

  Run for 1 million cycles, incrementing a ctr every 1000 cycles.
  Consume the config file test.yaml and produce a final yaml file called
  final.yaml.  Also, dump the tree:

  \code
  ./bld-Linux_x86_64-gcc4.7/sparta_core_example -r 1M \
                       --show-tree -p top.core0.params.ctr_incr_period 1000 \
                       -c test.yaml --write-final-config final.yaml
  \endcode

  Dump out the parameters:

  \code
  ./bld-Linux_x86_64-gcc4.7/sparta_core_example --no-run \
                        --write-final-config-verbose params.yaml
  \endcode

  Dump out a report in html:

  \code
  ./bld-Linux_x86_64-gcc4.7/sparta_core_example -r 1000000 --report-all output.html html
  \endcode

  Example parameter reference
  \code
  -r1000000 (or -r 1M)        Runs for 1000000 'cycles'
  --show-tree                 shows the Sparta device tree at every step
  -p ...                      Sets the value of the parameter identified to 1000
  -c test.yaml                Loads test.yaml at the global space and applies its parameters
  --write-final-config ...    Writes to the selected file after tree finalization
  \endcode

*/
