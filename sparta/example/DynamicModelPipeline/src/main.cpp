// <main.cpp> -*- C++ -*-


#include <iostream>

#include "ExampleSimulation.hpp" // Core model example simulator

#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/app/MultiDetailOptions.hpp"
#include "sparta/sparta.hpp"

// User-friendly usage that correspond with sparta::app::CommandLineSimulator
// options
const char USAGE[] =
    "Usage:\n"
    "    [-i insts] [-r RUNTIME] [--show-tree] [--show-dag]\n"
    "    [-p PATTERN VAL] [-c FILENAME]\n"
    "    [--node-config-file PATTERN FILENAME]\n"
    "    [-l PATTERN CATEGORY DEST]\n"
    "    [-h]\n"
    "\n";

constexpr char VERSION_VARNAME[] = "version"; //!< Name of option to show version


int main(int argc, char **argv)
{
    uint64_t ilimit = 0;
    uint32_t num_cores = 1;

    sparta::app::DefaultValues DEFAULTS;
    DEFAULTS.auto_summary_default = "on";

    sparta::SimulationInfo::getInstance() = sparta::SimulationInfo("sparta_core_example",
                                                               argc, argv, "", "", {});

    // try/catch block to ensure proper destruction of the cls/sim classes in
    // the event of an error
    try{
        // Helper class for parsing command line arguments, setting up the
        // simulator, and running the simulator. All of the things done by this
        // classs can be done manually if desired. Use the source for the
        // CommandLineSimulator class as a starting point
        sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
        auto& app_opts = cls.getApplicationOptions();
        app_opts.add_options()
            (VERSION_VARNAME,
             "produce version message",
             "produce version message") // Brief
            ("instruction-limit,i",
             sparta::app::named_value<uint64_t>("LIMIT", &ilimit)->default_value(ilimit),
             "Limit the simulation to retiring a specific number of instructions. 0 (default) "
             "means no limit. If -r is also specified, the first limit reached ends the simulation",
             "End simulation after a number of instructions. Note that if set to 0, this may be "
             "overridden by a node parameter within the simulator")
            ("num-cores",
             sparta::app::named_value<uint32_t>("CORES", &num_cores)->default_value(1),
             "The number of cores in simulation", "The number of cores in simulation")
            ("show-factories",
             "Show the registered factories");

        // Add any positional command-line options
        // po::positional_options_description& pos_opts = cls.getPositionalOptions();
        // (void)pos_opts;
        // pos_opts.add(TRACE_POS_VARNAME, -1); // example

        // Parse command line options and configure simulator
        int err_code = 0;
        if(!cls.parse(argc, argv, err_code)){
            return err_code; // Any errors already printed to cerr
        }

        bool show_factories = false;
        auto& vm = cls.getVariablesMap();
        if(vm.count("show-factories") != 0) {
            show_factories = true;
        }

        // Create the simulator
        sparta::Scheduler scheduler;
        ExampleSimulator sim(scheduler,
                             num_cores, // cores
                             ilimit,
                             show_factories); // run for ilimit instructions

        cls.populateSimulation(&sim);

        cls.runSimulator(&sim);

        cls.postProcess(&sim);

    }catch(...){
        // Could still handle or log the exception here
        throw;
    }
    return 0;
}
