
#include <iostream>
#include <map>
#include <functional>

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <boost/algorithm/string.hpp>

using sparta::TreeNode;
using sparta::PhasedObject;
using sparta::Parameter;
using sparta::ParameterSet;
using sparta::Scheduler;
using sparta::SimulationInfo;
using sparta::app::Simulation;
using sparta::SpartaException;

/*!
 * \file CommandLineSimulatorArgs_test.cpp
 * \brief Test for CommandLineSimulator argument parsing functionality
 */

TEST_INIT;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

//! Data structures that let unit tests pick and choose what simulation
//! phases they want to verify
using SimulatorVerificationCallback = std::function<void(const Simulation *)>;
using VerificationCallbacks = std::map<PhasedObject::TreePhase, SimulatorVerificationCallback>;

//! Example parameter set used to configure the example simulator below
class IntParameterSet : public ParameterSet
{
public:
    IntParameterSet(TreeNode * parent) :
        ParameterSet(parent),
        int_param_(new Parameter<uint32_t>(
            "foo", 0, "Example parameter"))
    {
        addParameter_(int_param_.get());
    }

private:
    std::unique_ptr<Parameter<uint32_t>> int_param_;
};

//! Dummy node class used together with IntParameterSet
class Baz : public TreeNode
{
public:
    Baz(TreeNode * parent,
        const std::string & desc) :
        TreeNode(parent, "baz_node", "BazGroup", 0, desc)
    {
        baz_.reset(new IntParameterSet(this));
    }

private:
    std::unique_ptr<IntParameterSet> baz_;
};

//! Simulator example
class MySimulator : public Simulation
{
public:

    MySimulator(const std::string& name, Scheduler * scheduler,
                const VerificationCallbacks & verifiers) :
        Simulation(name, scheduler),
        verification_callbacks_(verifiers)
    {;}

    ~MySimulator()
    {
        // Allow deletion of nodes without error now
        getRoot()->enterTeardown();
    }

private:
    void buildTree_() override
    {
        //Build a simple tree with the minimum tree nodes needed for basic
        //command line argument tests (parameters, arch/config, etc.)
        TreeNode * core_tn = new TreeNode(getRoot(), "core0", "Core 0 node");
        to_delete_.emplace_back(core_tn);

        TreeNode * baz = new Baz(core_tn, "Dummy parameter");
        to_delete_.emplace_back(baz);
    }

    void configureTree_() override
    {
        //Build phase is over. Give unit tests a chance to verify it went okay.
        auto iter = verification_callbacks_.find(PhasedObject::TREE_BUILDING);
        if (iter != verification_callbacks_.end()) {
            iter->second(this);
        }
    }

    void bindTree_() override
    {
        //Configuration phase is over. Give unit tests a chance to verify it went okay.
        auto iter = verification_callbacks_.find(PhasedObject::TREE_CONFIGURING);
        if (iter != verification_callbacks_.end()) {
            iter->second(this);
        }
    }

    // Do nothing for this dummy simulator, except allow unit tests to verify that
    // the tree is built/configured/bound correctly.
    void runControlLoop_(const uint64_t run_time) override
    {
        auto iter1 = verification_callbacks_.find(PhasedObject::TREE_FINALIZING);
        auto iter2 = verification_callbacks_.find(PhasedObject::TREE_FINALIZED);

        if (iter1 != verification_callbacks_.end() && iter2 != verification_callbacks_.end()) {
            throw SpartaException(
                "You cannot specify verification callbacks for TREE_FINALIZING and "
                "TREE_FINALIZED at the same time. Pick one.");
        } else if (iter1 != verification_callbacks_.end()) {
            iter1->second(this);
        } else if (iter2 != verification_callbacks_.end()) {
            iter2->second(this);
        }

        (void) run_time;
    }

    std::vector<std::unique_ptr<TreeNode>> to_delete_;
    const VerificationCallbacks verification_callbacks_;
};

//! Helper class that turns strings from the command line:
//!         { "--report", "report_def.yaml", "--config-file", "my_config.yaml" }
//!
//! Into the equivalent C-style argc/argv that you would get from main()
class CStyleArgs {
public:
    void createArgcArgvFromArgs(const std::vector<std::string> & args_kv_pairs) {
        //Put a pseudo executable name before the args
        args_kv_pairs_.push_back("test_executable_for_cmd_line_args");
        args_kv_pairs_.insert(args_kv_pairs_.end(), args_kv_pairs.begin(), args_kv_pairs.end());
        for (const auto & kv : args_kv_pairs_) {
            argv_vec_.push_back(const_cast<char*>(kv.c_str()));
        }
        argc_ = args_kv_pairs_.size();
    }

    int argc() const {
        return argc_;
    }

    char ** argv() {
        return &argv_vec_[0];
    }

private:
    int argc_ = 0;
    std::vector<char*> argv_vec_;
    std::vector<std::string> args_kv_pairs_;
};

//! Helper function to turn a single string such as:
//!         "--report report_def.yaml --config-file my_config.yaml"
//!
//! Into the equivalent C-style argc/argv that you would get from main()
CStyleArgs createCStyleArgsFromStdString(const std::string & args)
{
    //Turn this: "--config-file foo.yaml --report bar.yaml  "
    //into this: "--config-file foo.yaml --report bar.yaml"
    //
    //This is to prevent boost::split from getting confused below
    std::string trimmed_args(args);
    boost::trim(trimmed_args);

    std::vector<std::string> split_args;
    boost::split(split_args, trimmed_args, boost::is_any_of(" "));
    for (auto & arg : split_args) {
        boost::algorithm::trim(arg);
    }

    CStyleArgs parsed_args;
    parsed_args.createArgcArgvFromArgs(split_args);
    return parsed_args;
}

//! Run the example simulator with the given argument string,
//! passing in optional callbacks that give the test a chance
//! to verify simulation/configuration properties at various
//! points of the simulated run.
void runSimulatorWithCmdLineArgs(const std::string & cmd_line_args,
                                 const VerificationCallbacks & verifiers)
{
    const char USAGE[] = "example usage";
    sparta::app::CommandLineSimulator cls(USAGE);

    // Parse command line options and configure simulator
    int err_code = 0;
    CStyleArgs args = createCStyleArgsFromStdString(cmd_line_args);
    EXPECT_NOTHROW(EXPECT_EQUAL(cls.parse(args.argc(), args.argv(), err_code), true));

    // Create and run the simulator
    try {
        Scheduler scheduler;
        MySimulator sim("mysim", &scheduler, verifiers);
        cls.populateSimulation(&sim);
        cls.runSimulator(&sim);
        cls.postProcess(&sim);
    } catch (...) {
        throw;
    }
}

//! This unit test verifies that parameter values are assigned as expected
//! when any combination of:
//!     (1) --arch
//!     (2) --config-file
//!     (3) --parameter
//!
//! Are used at the command line. Depending on the ordering of these
//! arguments, you can get different parameter values. The rules for
//! how this is done is documented, and this test verifies that behavior.
void verifyArchConfigAndParamValuesProcessedInCorrectOrder()
{
    PRINT_ENTER_TEST

    struct Verifier {
        //Callback to verify the parameters were applied correctly.
        //This gets called after buildTree()
        void postBuildVerify(const Simulation * sim) const {
            if (expected_arch_val_.isValid()) {
                auto & atree = sim->getSimulationConfiguration()->getArchUnboundParameterTree();

                EXPECT_EQUAL(atree.get("top.core0.baz_node.params.foo").getAs<uint32_t>(),
                             expected_arch_val_.getValue());
            }
            if (expected_config_val_.isValid()) {
                auto & ptree = sim->getSimulationConfiguration()->getUnboundParameterTree();

                EXPECT_EQUAL(ptree.get("top.core0.baz_node.params.foo").getAs<uint32_t>(),
                             expected_config_val_.getValue());
            }
            if (expected_param_val_.isValid()) {
                auto & ptree = sim->getSimulationConfiguration()->getUnboundParameterTree();

                EXPECT_EQUAL(ptree.get("top.core0.baz_node.params.foo").getAs<uint32_t>(),
                             expected_param_val_.getValue());
            }
        }

        //Verification callback that occurs after configureTree()
        void postConfigureVerify(const Simulation * sim) const {
            (void) sim;
        }

        //Verification callback that occurs after bindTree()
        void postFinalizeVerify(const Simulation * sim) const {
            (void) sim;
        }

        //Give the verifier an expected value from an arch file
        void setExpectedValue_ARCH(const uint32_t expected_val) {
            expected_arch_val_ = expected_val;
        }

        //Give the verifier an expected value from a config file
        void setExpectedValue_CONFIG(const uint32_t expected_val) {
            expected_config_val_ = expected_val;
        }

        //Give the verifier an expected value from an individual parameter
        void setExpectedValue_PARAM(const uint32_t expected_val) {
            expected_param_val_ = expected_val;
        }

        //Reset the verifier to get ready for another test
        void clearExpectedValues() {
            expected_arch_val_.clearValid();
            expected_config_val_.clearValid();
            expected_param_val_.clearValid();
        }

    private:
        sparta::utils::ValidValue<uint32_t> expected_arch_val_;
        sparta::utils::ValidValue<uint32_t> expected_config_val_;
        sparta::utils::ValidValue<uint32_t> expected_param_val_;
    };

    Verifier verifier;
    VerificationCallbacks callbacks;

    callbacks[PhasedObject::TREE_BUILDING] =
        [&](const Simulation * sim) { verifier.postBuildVerify(sim); };
    callbacks[PhasedObject::TREE_CONFIGURING] =
        [&](const Simulation * sim) { verifier.postConfigureVerify(sim); };
    callbacks[PhasedObject::TREE_FINALIZED] =
        [&](const Simulation * sim) { verifier.postFinalizeVerify(sim); };

    //Let's try out some combinations of arch files, config files, and
    //individual parameter values:
    //
    //  default_arch.yaml               Value = 1
    //  default_config.yaml             Value = 3
    //  parameter                       Value = 16
    //
    //All three of these will try to apply their own competing value for
    //the parameter "top.core0.baz_node.params.foo" (uint32_t)
    //
    //Who will win?

    const uint32_t ArchValue   = 1;
    const uint32_t ConfigValue = 3;
    const uint32_t ParamValue  = 16;

    //Parameter value given by itself:
    //  Parameter wins by default
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("-p top.core0.baz_node.params.foo 16",
                                callbacks);

    //Arch file given by itself:
    //  Arch file wins by default
    verifier.clearExpectedValues();
    verifier.setExpectedValue_ARCH(ArchValue);
    runSimulatorWithCmdLineArgs("--arch-search-dir . --arch default_arch.yaml",
                                callbacks);

    //Config file given by itself:
    //  Config file wins by default
    verifier.clearExpectedValues();
    verifier.setExpectedValue_CONFIG(ConfigValue);
    runSimulatorWithCmdLineArgs("--config-file default_config.yaml",
                                callbacks);

    //Config file, and parameter value in that order:
    //  Parameter should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("--config-file default_config.yaml "
                                "-p top.core0.baz_node.params.foo 16",
                                callbacks);

    //Arch file, config file, parameter value all given in that order:
    //  Parameter should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("--arch-search-dir . --arch default_arch.yaml "
                                "--config-file default_config.yaml "
                                "-p top.core0.baz_node.params.foo 16",
                                callbacks);

    //Arch file, and config file given in that order:
    //  Config file should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_CONFIG(ConfigValue);
    runSimulatorWithCmdLineArgs("--arch-search-dir . --arch default_arch.yaml "
                                "--config-file default_config.yaml",
                                callbacks);

    //Config file, and arch file given in that order:
    //  Arch file should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_ARCH(ArchValue);
    runSimulatorWithCmdLineArgs("--config-file default_config.yaml "
                                "--arch-search-dir . --arch default_arch.yaml",
                                callbacks);

    //Arch file, and parameter value given in that order:
    //  Parameter should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("--arch-search-dir . --arch default_arch.yaml "
                                "-p top.core0.baz_node.params.foo 16",
                                callbacks);

    //Config file, and parameter value given in that order:
    //  Parameter should win
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("--config-file default_config.yaml "
                                "-p top.core0.baz_node.params.foo 16",
                                callbacks);

    //Parameter value, and config file given in that order:
    //  **PARAMETER** value should win. Individual parameter values
    //  explicitly provided by the user should always override the
    //  same parameter value(s) that may appear in an arch/config
    //  file.
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("-p top.core0.baz_node.params.foo 16 "
                                "--config-file default_config.yaml",
                                callbacks);

    //Parameter value, and arch file given in that order:
    //  **PARAMETER** value should win. Individual parameter values
    //  explicitly provided by the user should always override the
    //  same parameter value(s) that may appear in an arch/config
    //  file.
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("-p top.core0.baz_node.params.foo 16 "
                                "--arch-search-dir . --arch default_arch.yaml",
                                callbacks);

    //Parameter value, arch file, and config file given in that order:
    //  **PARAMETER** value should win. Individual parameter values
    //  explicitly provided by the user should always override the
    //  same parameter value(s) that may appear in an arch/config
    //  file.
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("-p top.core0.baz_node.params.foo 16 "
                                "--arch-search-dir . --arch default_arch.yaml "
                                "--config-file default_config.yaml",
                                callbacks);

    //Config file, parameter value, and arch file given in that order:
    //  **PARAMETER** value should win. Individual parameter values
    //  explicitly provided by the user should always override the
    //  same parameter value(s) that may appear in an arch/config
    //  file.
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(ParamValue);
    runSimulatorWithCmdLineArgs("--config-file default_config.yaml "
                                "-p top.core0.baz_node.params.foo 16 "
                                "--arch-search-dir . --arch default_arch.yaml",
                                callbacks);

    //Parameter value (16), config file, another parameter value (17),
    //and an arch file given in that order:
    //  ** SECOND parameter** value should win. While individual
    //  parameter values override arch/config, if there are multiple
    //  -p values given at the command line for the same parameter
    //  node, then the last such -p value should win.
    const uint32_t OverridingParamValue = 17;
    verifier.clearExpectedValues();
    verifier.setExpectedValue_PARAM(OverridingParamValue);
    runSimulatorWithCmdLineArgs("-p top.core0.baz_node.params.foo 16 "
                                "--config-file default_config.yaml "
                                "-p top.core0.baz_node.params.foo 17 "
                                "--arch-search-dir . --arch default_arch.yaml",
                                callbacks);
}

int main()
{
    // Disable the sleeper thread singleton so we can run many
    // small simulations in this one executable. Normally, doing
    // so would get the sleeper thread confused and it would assert
    // that something went wrong.
    sparta::SleeperThread::disableForever();

    verifyArchConfigAndParamValuesProcessedInCorrectOrder();

    REPORT_ERROR;
    return ERROR_CODE;
}
