
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

void testFeatureConfig()
{
    PRINT_ENTER_TEST

    sparta::app::FeatureConfiguration features;
    EXPECT_EQUAL(features.getFeatureValue("map_v3"), 0);

    features.setFeatureValue("map_v3", 2);
    EXPECT_EQUAL(features.getFeatureValue("map_v3"), 2);

    EXPECT_NOTEQUAL(features.getFeatureOptions("map_v3"), nullptr);
    features.setFeatureOptionsFromFile("map_v3", "sample_feat_opts.yaml");

    auto opts = features.getFeatureOptions("map_v3");
    EXPECT_NOTEQUAL(opts, nullptr);

    //The sample options yaml file we just applied has
    //values like this:
    //    foo: hello
    //    bar: 56.8
    //
    //Let's try a variety of getOptionValue<T>() calls,
    //including a few calls where we mix up the feature
    //option data type (foo is a double, bar is a string).

    //When we ask for a feature option that does not exist,
    //it should return the default value we pass in.
    std::string default_opt_str = opts->getOptionValue<std::string>(
        "nonexistent", "none");
    EXPECT_EQUAL(default_opt_str, "none");

    double default_opt_dbl = opts->getOptionValue<double>(
        "nonexistent", 4.6);
    EXPECT_EQUAL(default_opt_dbl, 4.6);

    //Asking for a named option which exists in the yaml
    //file should just return the value, either as a string
    //or as a double depending on the <T> data type.
    std::string custom_opt_str = opts->getOptionValue<std::string>(
        "foo", "none");
    EXPECT_EQUAL(custom_opt_str, "hello");

    double custom_opt_dbl = opts->getOptionValue<double>(
        "bar", 4.6);
    EXPECT_WITHIN_EPSILON(custom_opt_dbl, 56.8);

    //In this sample options file, "foo" was a string ("hello"),
    //so this call site <double> is not valid. It should return
    //the default double we pass in.
    default_opt_dbl = opts->getOptionValue<double>(
        "foo", 4.6);
    EXPECT_WITHIN_EPSILON(default_opt_dbl, 4.6);

    //However, even though the "bar" option *looks* like
    //a double (56.8) it is still picked up from the yaml
    //file as a string ("56.8"), and therefore asking for
    //the "bar" option as a string should return the option
    //value that was found in the file *as a string*.
    default_opt_str = opts->getOptionValue<std::string>(
        "bar", "hello");
    EXPECT_EQUAL(default_opt_str, "56.8");

    //Test one of the utility free functions for various types of
    //FeatureConfiguration pointers: raw, shared_ptr, unique_ptr
    {
        sparta::app::FeatureConfiguration * feature_cfg = nullptr;
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg = new sparta::app::FeatureConfiguration;
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg->setFeatureValue("map_v3", 5);
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 5));

        delete feature_cfg;
    }
    {
        std::shared_ptr<sparta::app::FeatureConfiguration> feature_cfg;
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg.reset(new sparta::app::FeatureConfiguration);
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg->setFeatureValue("map_v3", 5);
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 5));
    }
    {
        std::unique_ptr<sparta::app::FeatureConfiguration> feature_cfg;
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg.reset(new sparta::app::FeatureConfiguration);
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 0));

        feature_cfg->setFeatureValue("map_v3", 5);
        EXPECT_TRUE(sparta::IsFeatureValueEqualTo(feature_cfg, "map_v3", 5));
    }
}

int main(int argc, char **argv)
{
    testFeatureConfig();

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
