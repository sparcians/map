#include <inttypes.h>
#include <iostream>
#include <sstream>
#include <exception>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"

/*!
 * \file TreeNodeDebug_test.cpp
 * \brief Test for TreeNode, parameters, and simple parsing of configuration files
 */

TEST_INIT;

//! Simple device which defines its own parameter set object.
class SimpleDevice : public sparta::Resource
{
public:

    static int num_simpledevices_torn_down;

    class ParameterSet; // Forward declared so our constructor can be at the top of this class def

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="SimpleDevice";

    SimpleDevice(sparta::TreeNode * node,
                 const SimpleDevice::ParameterSet * params) :
        sparta::Resource(node)
    {
        (void) params;
    }

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {
        }
    };

    void validatePostRun_(const sparta::PostRunValidationInfo&) const override {
        EXPECT_REACHED();
        throw sparta::SpartaException("exception form validatePostRun_");
    }

    void dumpDebugContent_(std::ostream& output) const override {
        output << "Some debug content\n";
        EXPECT_REACHED();
    }

    // Callback when tearing down
    void onStartingTeardown_() override {
        std::cout << "Starting Teardown of SimpleDevice x" << ++num_simpledevices_torn_down << std::endl;
        EXPECT_REACHED();
    }
};

int SimpleDevice::num_simpledevices_torn_down = 0;


bool runTest(bool validate_post_run,
             bool always_dump_debug,
             std::ostream& debug_out)
{
    { // Scope to this block
        sparta::ResourceFactory<SimpleDevice> fact; // ParameterSet figured out automatically

        sparta::Scheduler sched;
        sparta::Clock clk("clock", &sched);

        sparta::RootTreeNode top;
        top.setClock(&clk);
        sparta::ResourceTreeNode a(&top, "a", "A Node", &fact);
        sparta::ResourceTreeNode b(&top, "b", "B Node", &fact);

        // Print out the tree at different levels with different options

        std::cout << "The tree from the top: " << std::endl << top.renderSubtree(-1, true) << std::endl;

        std::exception_ptr eptr;
        bool run_successful = true;
        try{

            EXPECT_NOTHROW(top.enterConfiguring(););
            EXPECT_NOTHROW(top.enterFinalized(););

            std::cout << "Running test" << std::endl;

            if(validate_post_run){
                try{
                    top.validatePostRun();
                }catch(...){
                    eptr = std::current_exception(); // capture
                    std::cerr << "Exception during post-run validation as expected" << std::endl;
                    run_successful = false;
                }
            }
        }catch(...){
            if(run_successful == true){
                std::cerr << "Failed during configuration or run" << std::endl;
                eptr = std::current_exception(); // capture this instead
                run_successful = false;
            }
        }

        if(run_successful == false || always_dump_debug == true){
            EXPECT_NOTHROW(top.dumpDebugContent(debug_out)); // Suppressed internally
        }

        top.enterTeardown();

        EXPECT_EQUAL(top.isBuilt(), true);
        EXPECT_EQUAL(top.isConfigured(), true);
        EXPECT_EQUAL(top.isFinalizing(), false);
        EXPECT_EQUAL(top.isFinalized(), false);
        EXPECT_EQUAL(top.isTearingDown(), true);

        if(run_successful == false){
            // Final rethrow before exit

            EXPECT_REACHED();
            //if (eptr != std::exception_ptr()) {
            //    std::rethrow_exception(eptr);
            //}

            return false;
        }

        return true;

        // Done
    } // Destruction
}

int main()
{
    {
        std::stringstream out;
        EXPECT_EQUAL(runTest(false, false, out), true); // Do not validate post-run
        EXPECT_EQUAL(out.str(), "");
    }

    ENSURE_ALL_REACHED(1); // final rethrow in runTest
    EXPECT_EQUAL(SimpleDevice::num_simpledevices_torn_down, 2);

    {
        std::stringstream out;
        EXPECT_EQUAL(runTest(true, false, out), false); // Normal case. post-run validation catches an error
        EXPECT_TRUE(out.str().size() >= 200); // From nodes a & b
    }

    ENSURE_ALL_REACHED(4); // final rethrow in runTest
    EXPECT_EQUAL(SimpleDevice::num_simpledevices_torn_down, 4);

    {
        std::stringstream out;
        EXPECT_EQUAL(runTest(false, true, out), true); // Do not validate post-run
        EXPECT_TRUE(out.str().size() >= 200); // From nodes a & b
    }

    ENSURE_ALL_REACHED(4); // final rethrow in runTest
    EXPECT_EQUAL(SimpleDevice::num_simpledevices_torn_down, 6);


    // Diagnostic printing of all unfreed TreeNodes. A few are expected
    std::cout << "\nUnfreed TreeNodes (some globals expected)" << std::endl;
    std::cout << sparta::TreeNode::formatAllNodes() << std::endl;

    REPORT_ERROR;
}
