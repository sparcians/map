
#include "sparta/app/CommandLineSimulator.hpp"

#include <filesystem>
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file CommandLineSimulator_test.cpp
 * \brief Test for CommandLineSimulator app infrastructure
 */

TEST_INIT

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;


/*!
 * \brief Simulator example
 */
class MySimulator : public sparta::app::Simulation
{
public:

    MySimulator(const std::string& name, sparta::Scheduler * scheduler)
      : sparta::app::Simulation(name, scheduler)
    {;}

    ~MySimulator()
    {
        getRoot()->enterTeardown(); // Allow deletion of nodes without error now
    }

    void buildTree_() override
    {;}

    void configureTree_() override
    {;}

    void bindTree_() override
    {;}

    // Do nothing for this dummy simulator
    void runControlLoop_(uint64_t run_time) override
    {
        (void) run_time;
    }
};

int main(int argc, char **argv)
{
    // Defaults for command line simulator
    sparta::app::DefaultValues DEFAULTS;
#ifndef __APPLE__
    std::filesystem::path binary_dir = std::filesystem::canonical("/proc/self/exe" );
    std::filesystem::path cmd_dir = binary_dir.parent_path().parent_path();
    auto default_arch_dir = (cmd_dir / std::filesystem::path("parameters/arch")).string();
#else
    // BROKEN!
    auto default_arch_dir = "parameters/arch";
#endif
    DEFAULTS.arch_search_dirs = {"archs",
                                 "deep_archs",
                                 "other_archs",
                                 default_arch_dir}; // Where --arch will be resolved by default

    const char USAGE[] = "example usage";
    sparta::app::CommandLineSimulator cls(USAGE,
                                        DEFAULTS);

#ifdef TEST_DISABLED_BT_SIGNALS
    cls.getSimulationConfiguration().signal_mode =
         sparta::app::SimulationConfiguration::SignalMode::DISABLE_BACKTRACE_SIGNALS;
#endif
    std::cout << "Arch search path: " << cls.getSimulationConfiguration().getArchSearchPath() << std::endl;
    std::cout << "Configu search path: " << cls.getSimulationConfiguration().getConfigSearchPath() << std::endl;

    // Parse command line options and configure simulator
    int err_code = 0;
    EXPECT_NOTHROW(EXPECT_EQUAL(cls.parse(argc, argv, err_code), true));

    sparta::SimulationInfo::getInstance() = sparta::SimulationInfo("command_line_test",
                                                               argc,
                                                               argv,
                                                               "2.3.4.5",  // Simulator version
                                                               "127abc:sparta", // Reproduction
                                                               {});
    EXPECT_TRUE(sparta::SimulationInfo::getInstance().sparta_version != std::string("unknown"));
    EXPECT_TRUE(sparta::SimulationInfo::getInstance().sparta_version != std::string(""));
    std::cout << "SPARTA VERSION: " << sparta::SimulationInfo::getInstance().sparta_version << std::endl;

    // Create the simulator
    try{
        sparta::Scheduler scheduler;
        MySimulator sim("mysim", &scheduler);
        cls.populateSimulation(&sim);
        cls.runSimulator(&sim);
#ifdef TEST_DISABLED_BT_SIGNALS
        abort();
#endif
        cls.postProcess(&sim);

    }catch(...){
        // We could still handle or log the exception here
        throw;
    }


    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
