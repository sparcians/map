

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "SpartaSystemCSimulator.hpp"

#define REPORT_DEFINE_GLOBALS // so sad...
#include "reporting.h"

int sc_main(int argc, char  *argv[])
{
    // Part of the SystemC initiator
    REPORT_ENABLE_ALL_REPORTING ();

    sparta::app::DefaultValues DEFAULTS;
    sparta::app::CommandLineSimulator cls("", DEFAULTS);
    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }
    sparta_assert(err_code == 0);

    // Enable/create the sparta simulator
    sparta::Scheduler sched;
    sparta_sim::SpartaSystemCSimulator sim(&sched);

    cls.populateSimulation(&sim);
    cls.runSimulator(&sim);
    //    sim.run(10000); // run up to 10K ns.  Can use sparta::Scheduler::INDEFINITE

    return 0;
}
