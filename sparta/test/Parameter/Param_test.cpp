#include <iostream>
#include <memory>
#include <cstring>

#include "Device.hpp"

#include "sparta/sparta.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Printing.hpp"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/app/Simulation.hpp"

TEST_INIT;

// Test result constants
const uint32_t EXPECTED_NUM_PARAMS = 59;
const uint32_t EXPECTED_BOUND_TYPES = 14;

class ExampleSimulator : public sparta::app::Simulation{
public:
    ExampleSimulator(sparta::Scheduler & sched) :
        sparta::app::Simulation("Test_special_params", &sched){}

    virtual ~ExampleSimulator(){
        getRoot()->enterTeardown();
    }
private:
    void buildTree_() override{};
    void configureTree_() override{};
    void bindTree_() override{};

};

int main ()
{
    sparta::Scheduler scheduler;

    // Typical usage for simulator startup
    // 1. Allocate ParameterSetSubclass (with defaults)
    // 2. Assign (ownership) to Placeholder DeviceTreeNode
    // 4. Populate from config file(s)
    // 5. Populate manually (from C++ code)
    // 6. Validate params
    // 7. Construct Resource (Unit) with params (through factory)
    // 8. Delete ParameterSetSubclass
    //    a. after construction of Resource?
    //    b. during destruction of DeviceTreeNode

    // Instantiation of tree node, scheduler and clock.
    std::unique_ptr<ExampleSimulator> sim(new ExampleSimulator(scheduler));

    // Get root of device tree
    auto rtn = sim->getRoot();

    // Attach two children node to root
    sparta::TreeNode node_1(rtn, "node_1", "Left node of root");
    sparta::TreeNode node_2(rtn, "node_2", "Right node of root");

    // Specific Parameter Set for root node
    std::unique_ptr<SampleDevice::SampleDeviceParameterSet>
        sps(new SampleDevice::SampleDeviceParameterSet(rtn));

    // Specific Parameter Set for root left child
    std::unique_ptr<SampleDevice::SampleDeviceParameterSet>
        sps_left(new SampleDevice::SampleDeviceParameterSet(&node_1));

    // Specific Parameter Set for root right child
    std::unique_ptr<SampleDevice::SampleDeviceParameterSet>
        sps_right(new SampleDevice::SampleDeviceParameterSet(&node_2));

    sim->buildTree();
    sim->configureTree();

    // Generic Parameter Set
    sparta::ParameterSet* gps = sps.get();

    // ParameterSet members (generic)
    std::cout << "gps: " << gps->getName() << " " << gps->getNumParameters() << " params" << std::endl;
    EXPECT_TRUE(gps->getName() == "params");
    EXPECT_TRUE(gps->getNumParameters() == EXPECTED_NUM_PARAMS); // From SampleDeviceParameterSet class and base class(es)
    EXPECT_NOTHROW(gps->getParameter("length"));
    EXPECT_TRUE(gps->hasParameter("length"));

    // ParameterSet members (specific)
    std::cout << "sps: " << sps->getName() << " " << sps->getNumParameters() << " params" << std::endl;
    EXPECT_TRUE(sps->getName() == "params");
    EXPECT_NOTHROW(sps->getParameter("length"));
    EXPECT_TRUE(sps->hasParameter("length"));
    EXPECT_NOTHROW(sps->getParameter("dummy_locked_var"));
    EXPECT_TRUE(sps->hasParameter("dummy_locked_var"));
    EXPECT_TRUE(sps->getNumParameters() == EXPECTED_NUM_PARAMS); // From SampleDeviceParameterSet class and base class(es)

    // Don't got changing the structure of key value pairs without updating this test
    EXPECT_TRUE(sps->getNumBoundTypes() == EXPECTED_BOUND_TYPES);


    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Test locked parameter///////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    EXPECT_NOTHROW(sps->getParameter("dummy_locked_var"));
    EXPECT_TRUE(sps->hasParameter("dummy_locked_var"));
    EXPECT_NOTHROW(sps_left->getParameter("dummy_locked_var"));
    EXPECT_TRUE(sps_left->hasParameter("dummy_locked_var"));
    EXPECT_NOTHROW(sps_right->getParameter("dummy_locked_var"));
    EXPECT_TRUE(sps_right->hasParameter("dummy_locked_var"));

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing locked parameters of root
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 0u);
    std::cout << sps->dummy_locked_var << std::endl;
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps->dummy_locked_var, 0x03);    // This is a parameter read.
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 1u); // Should increment read count
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x0A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x0A);
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 1u);
    std::cout << (sps->dummy_locked_var == 0x0A) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps->dummy_locked_var, 0x0A); // Should increment read count during parameter read
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 3u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x0F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x0F);
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x1A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x1A);
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x18")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x18);
    EXPECT_EQUAL(sps->dummy_locked_var.getReadCount(), 1u);

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing locked parameters of root left child
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 0u);
    std::cout << sps_left->dummy_locked_var << std::endl;
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x03);    // This is a parameter read.
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 1u); // Should increment read count
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_locked_var.setValueFromString("0x0B")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x0B);
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 1u);
    std::cout << (sps_left->dummy_locked_var == 0x0B) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x0B); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 3u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_locked_var.setValueFromString("0x2F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x2F);
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_locked_var.setValueFromString("0x1D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x1D);
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_locked_var.setValueFromString("0x10")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x10);
    EXPECT_EQUAL(sps_left->dummy_locked_var.getReadCount(), 1u);

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing locked parameters of root right child
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 0u);
    std::cout << sps_right->dummy_locked_var << std::endl;
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x03);    // This is a parameter read.
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 1u); // Should increment read count
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_locked_var.setValueFromString("0x0FF")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x0FF);
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 1u);
    std::cout << (sps_right->dummy_locked_var == 0x0FF) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x0FF); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 3u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_locked_var.setValueFromString("0x3C")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x3C);
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_locked_var.setValueFromString("0x4E")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x4E);
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_locked_var.setValueFromString("0x18A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x18A);
    EXPECT_EQUAL(sps_right->dummy_locked_var.getReadCount(), 1u);

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Test volatile locked parameter//////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    EXPECT_NOTHROW(sps->getParameter("dummy_locked_var_2"));
    EXPECT_TRUE(sps->hasParameter("dummy_locked_var_2"));
    EXPECT_NOTHROW(sps_left->getParameter("dummy_locked_var_2"));
    EXPECT_TRUE(sps_left->hasParameter("dummy_locked_var_2"));
    EXPECT_NOTHROW(sps_right->getParameter("dummy_locked_var_2"));
    EXPECT_TRUE(sps_right->hasParameter("dummy_locked_var_2"));

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Test volatile locked parameters of root
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 0u);
    std::cout << sps->dummy_locked_var_2 << std::endl;
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x00);    // This is a parameter read.
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 1u); // Should increment read count
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x0B")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x0B);
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 1u); // Setting values resets read count.
    std::cout << (sps->dummy_locked_var_2 == 0x0B) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x0B); // Should increment read count during parameter read
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 3u);
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x2F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x2F);
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x16")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x16);
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x8A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x8A);
    EXPECT_EQUAL(sps->dummy_locked_var_2.getReadCount(), 1u);

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Test volatile locked parameters of root left child
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 0u);
    std::cout << sps_left->dummy_locked_var_2 << std::endl;
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x00);    // This is a parameter read.
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 1u); // Should increment read count
    EXPECT_NOTHROW(sps_left->dummy_locked_var_2.setValueFromString("0x1E")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x1E);
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 1u); // Setting values resets read count.
    std::cout << (sps_left->dummy_locked_var_2 == 0x1E) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x1E); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 3u);
    EXPECT_NOTHROW(sps_left->dummy_locked_var_2.setValueFromString("0x2A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x2A);
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps_left->dummy_locked_var_2.setValueFromString("0x16A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x16A);
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps_left->dummy_locked_var_2.setValueFromString("0x8AC")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x8AC);
    EXPECT_EQUAL(sps_left->dummy_locked_var_2.getReadCount(), 1u);

    // Test read and write of locked parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Test volatile locked parameters of root right child
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 0u);
    std::cout << sps_right->dummy_locked_var_2 << std::endl;
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x00);    // This is a parameter read.
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 1u); // Should increment read count
    EXPECT_NOTHROW(sps_right->dummy_locked_var_2.setValueFromString("0xCB")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0xCB);
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 1u); // Setting values resets read count.
    std::cout << (sps_right->dummy_locked_var_2 == 0xCB) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0xCB); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 3u);
    EXPECT_NOTHROW(sps_right->dummy_locked_var_2.setValueFromString("0x2FA")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x2FA);
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps_right->dummy_locked_var_2.setValueFromString("0x26A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x26A);
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 1u);
    EXPECT_NOTHROW(sps_right->dummy_locked_var_2.setValueFromString("0x8AA")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x8AA);
    EXPECT_EQUAL(sps_right->dummy_locked_var_2.getReadCount(), 1u);

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Test hidden parameter///////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    EXPECT_NOTHROW(sps->getParameter("dummy_hidden_var"));
    EXPECT_TRUE(sps->hasParameter("dummy_hidden_var"));
    EXPECT_NOTHROW(sps_left->getParameter("dummy_hidden_var"));
    EXPECT_TRUE(sps_left->hasParameter("dummy_hidden_var"));
    EXPECT_NOTHROW(sps_right->getParameter("dummy_hidden_var"));
    EXPECT_TRUE(sps_right->hasParameter("dummy_hidden_var"));

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 0u);
    std::cout << sps->dummy_hidden_var << std::endl;
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps->dummy_hidden_var, 0xA3);    // This is a parameter read.
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 1u); // Should increment read count
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var.setValueFromString("0x0A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x0A);
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 1u);
    std::cout << (sps->dummy_hidden_var == 0x0A) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x0A); // Should increment read count during parameter read
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 3u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var.setValueFromString("0x0F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x0F);
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var.setValueFromString("0x1A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x1A);
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var.setValueFromString("0x18")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x18);
    EXPECT_EQUAL(sps->dummy_hidden_var.getReadCount(), 1u);

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root left child
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 0u);
    std::cout << sps_left->dummy_hidden_var << std::endl;
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0xA3);    // This is a parameter read.
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 1u); // Should increment read count
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var.setValueFromString("0x0B")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x0B);
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 1u);
    std::cout << (sps_left->dummy_hidden_var == 0x0B) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x0B); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 3u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var.setValueFromString("0x2F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x2F);
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var.setValueFromString("0x1D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x1D);
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var.setValueFromString("0x10")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x10);
    EXPECT_EQUAL(sps_left->dummy_hidden_var.getReadCount(), 1u);

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root right child
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 0u);
    std::cout << sps_right->dummy_hidden_var << std::endl;
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0xA3);    // This is a parameter read.
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 1u); // Should increment read count
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var.setValueFromString("0x0FF")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x0FF);
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 1u);
    std::cout << (sps_right->dummy_hidden_var == 0x0FF) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 2u);
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x0FF); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 3u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var.setValueFromString("0x3C")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x3C);
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var.setValueFromString("0x4E")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x4E);
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var.setValueFromString("0x18A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x18A);
    EXPECT_EQUAL(sps_right->dummy_hidden_var.getReadCount(), 1u);

    // Test to see that hidden parameters show up in dump list of parameters before lockdown phase is applied
    auto sps_param_list = sps->dumpList();
    auto sps_left_param_list = sps_left->dumpList();
    auto sps_right_param_list = sps_right->dumpList();
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var_2"), std::string::npos);
    EXPECT_NOTEQUAL(sps_left_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_left_param_list.find("dummy_hidden_var_2"), std::string::npos);
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var_2"), std::string::npos);

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Test volatile hidden parameter//////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    EXPECT_NOTHROW(sps->getParameter("dummy_hidden_var_2"));
    EXPECT_TRUE(sps->hasParameter("dummy_hidden_var_2"));
    EXPECT_NOTHROW(sps_left->getParameter("dummy_hidden_var_2"));
    EXPECT_TRUE(sps_left->hasParameter("dummy_hidden_var_2"));
    EXPECT_NOTHROW(sps_right->getParameter("dummy_hidden_var_2"));
    EXPECT_TRUE(sps_right->hasParameter("dummy_hidden_var_2"));

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 0u);
    std::cout << sps->dummy_hidden_var_2 << std::endl;
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0xA4);    // This is a parameter read.
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 1u); // Should increment read count
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var_2.setValueFromString("0x0A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x0A);
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 1u);
    std::cout << (sps->dummy_hidden_var_2 == 0x0A) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x0A); // Should increment read count during parameter read
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 3u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var_2.setValueFromString("0x0F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x0F);
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var_2.setValueFromString("0x1A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x1A);
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 1u);
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var_2.setValueFromString("0x18")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x18);
    EXPECT_EQUAL(sps->dummy_hidden_var_2.getReadCount(), 1u);

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root left child
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 0u);
    std::cout << sps_left->dummy_hidden_var_2 << std::endl;
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0xA4);    // This is a parameter read.
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 1u); // Should increment read count
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var_2.setValueFromString("0x0B")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x0B);
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 1u);
    std::cout << (sps_left->dummy_hidden_var_2 == 0x0B) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x0B); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 3u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var_2.setValueFromString("0x2F")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x2F);
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var_2.setValueFromString("0x1D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x1D);
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 1u);
    sps_left->resetReadCounts();
    EXPECT_NOTHROW(sps_left->dummy_hidden_var_2.setValueFromString("0x10")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x10);
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2.getReadCount(), 1u);

    // Test read and write of hidden parameter. This will happen as many times as the modeler
    // wishes till the parameter lockdown phase is not applied.
    // Testing hidden parameters of root right child
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 0u);
    std::cout << sps_right->dummy_hidden_var_2 << std::endl;
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 0u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0xA4);    // This is a parameter read.
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 1u); // Should increment read count
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var_2.setValueFromString("0x0FF")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x0FF);
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 1u);
    std::cout << (sps_right->dummy_hidden_var_2 == 0x0FF) << std::endl; // Should increment read count during comparison
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 2u);
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x0FF); // Should increment read count during parameter read
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 3u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var_2.setValueFromString("0x3C")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x3C);
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var_2.setValueFromString("0x4E")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x4E);
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 1u);
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var_2.setValueFromString("0x18A")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x18A);
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2.getReadCount(), 1u);

    // Test to see that hidden parameters show up in dump list of parameters before lockdown phase is applied
    sps_param_list = sps->dumpList();
    sps_left_param_list = sps_left->dumpList();
    sps_right_param_list = sps_right->dumpList();
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var_2"), std::string::npos);
    EXPECT_NOTEQUAL(sps_left_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_left_param_list.find("dummy_hidden_var_2"), std::string::npos);
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var_2"), std::string::npos);

    // Important point to note:
    // Here the Simulation class is explicitly calling to lockdown all LOCKED_PARAMETERS and
    // and HIDDEN_PARAMETERS. After this phase, LOCKED_PARAMETERS cannot be overwritten anymore
    // and HIDDEN_PARAMETERS are also locked as well as hidden from printouts, dumps.
    // Point to note is this lock-down phase has no effect on regular parameters and they can still
    // be overwritten multiple times till Tree Finalize phase comes. Only if the ParameterSet has any
    // LOCKED/HIDDEN parameters, this phase would actually do something different. Otherwise, this
    // phase is a no-op.

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Tree Node 1 Lockdown////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    sim->getRoot()->getChild("node_1")->lockdownParameters();

    // Locked and volatile locked parameters of node_1 subtree cannot be set anymore after Lockdown phase
    EXPECT_THROW(sps_left->dummy_locked_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x10);
    EXPECT_THROW(sps_left->dummy_locked_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x8AC);

    // Hidden and volatile hidden parameters of node_1 subtree cannot be set anymore after Lockdown phase
    EXPECT_THROW(sps_left->dummy_hidden_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x10);
    EXPECT_THROW(sps_left->dummy_hidden_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x10);

    // Test to see that hidden parameters show up in dump list of parameters after lockdown phase is applied
    sps_left_param_list = sps_left->dumpList();
    EXPECT_EQUAL(sps_left_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_EQUAL(sps_left_param_list.find("dummy_hidden_var_2"), std::string::npos);
    // Since only node_1 has been locked, node_2 can still manipulate its locked parameters
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_locked_var.setValueFromString("0x1C")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x1C);
    EXPECT_NOTHROW(sps_right->dummy_locked_var_2.setValueFromString("0x8C")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x8C);

    // Since only node_1 has been locked, node_2 can still manipulate its hidden parameters
    sps_right->resetReadCounts();
    EXPECT_NOTHROW(sps_right->dummy_hidden_var.setValueFromString("0x11")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x11);
    EXPECT_NOTHROW(sps_right->dummy_hidden_var_2.setValueFromString("0x12")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x12);

    // Test to see that hidden node_2 parameters show up in dump list of parameters after lockdown phase is applied
    sps_right_param_list = sps_right->dumpList();
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_right_param_list.find("dummy_hidden_var_2"), std::string::npos);

    // Since only node_1 has been locked, root can still manipulate its locked parameters
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x1D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x1D);
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x8D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x8D);

    // Since only node_1 has been locked, root can still manipulate its hidden parameters
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_hidden_var.setValueFromString("0x1D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x1D);
    EXPECT_NOTHROW(sps->dummy_hidden_var_2.setValueFromString("0x8D")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x8D);

    // Test to see that hidden root parameters show up in dump list of parameters after lockdown phase is applied
    sps_param_list = sps->dumpList();
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var_2"), std::string::npos);

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Tree Node 2 Lockdown////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    sim->getRoot()->getChild("node_2")->lockdownParameters();

    // Locked and volatile locked parameters of node_1 subtree(locked) cannot be set
    EXPECT_THROW(sps_left->dummy_locked_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x10);
    EXPECT_THROW(sps_left->dummy_locked_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x8AC);

    // Hidden and volatile hidden parameters of node_1 subtree cannot be set anymore after Lockdown phase
    EXPECT_THROW(sps_left->dummy_hidden_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var, 0x10);
    EXPECT_THROW(sps_left->dummy_hidden_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_hidden_var_2, 0x10);

    // Test to see that hidden node_1 parameters show up in dump list of parameters after lockdown phase is applied
    sps_left_param_list = sps_left->dumpList();
    EXPECT_EQUAL(sps_left_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_EQUAL(sps_left_param_list.find("dummy_hidden_var_2"), std::string::npos);
    // Locked and volatile locked parameters of node_2 subtree cannot be set anymore after lockdown phase
    EXPECT_THROW(sps_right->dummy_locked_var.setValueFromString("0x8C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x1C);
    EXPECT_THROW(sps_right->dummy_locked_var_2.setValueFromString("0x28C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x8C);

    // Hidden and volatile hidden parameters of node_2 subtree cannot be set anymore after Lockdown phase
    EXPECT_THROW(sps_right->dummy_hidden_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var, 0x11);
    EXPECT_THROW(sps_right->dummy_hidden_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_hidden_var_2, 0x12);

    // Test to see that hidden parameters show up in dump list of parameters after lockdown phase is applied
    sps_right_param_list = sps_right->dumpList();
    EXPECT_EQUAL(sps_right_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_EQUAL(sps_right_param_list.find("dummy_hidden_var_2"), std::string::npos);

    // Since only node_1 and node_2 has been locked, root can still manipulate its locked parameters
    sps->resetReadCounts();
    EXPECT_NOTHROW(sps->dummy_locked_var.setValueFromString("0x1DE")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x1DE);
    EXPECT_NOTHROW(sps->dummy_locked_var_2.setValueFromString("0x8DA")); // Should allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x8DA);

    // Test to see that hidden root parameters show up in dump list of parameters after lockdown phase is applied
    sps_param_list = sps->dumpList();
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_NOTEQUAL(sps_param_list.find("dummy_hidden_var_2"), std::string::npos);

    /////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////Root Node Lockdown//////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////
    sim->getRoot()->lockdownParameters();

    // Locked and volatile locked parameters of node_1 subtree(locked) cannot be set
    EXPECT_THROW(sps_left->dummy_locked_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var, 0x10);
    EXPECT_THROW(sps_left->dummy_locked_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_left->dummy_locked_var_2, 0x8AC);

    // Locked and volatile locked parameters of node_2 subtree cannot be set anymore after lockdown phase
    EXPECT_THROW(sps_right->dummy_locked_var.setValueFromString("0x8C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var, 0x1C);
    EXPECT_THROW(sps_right->dummy_locked_var_2.setValueFromString("0x28C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps_right->dummy_locked_var_2, 0x8C);

    // Locked and volatile locked parameters of root can no longer be manipulated
    EXPECT_THROW(sps->dummy_locked_var.setValueFromString("0x1B")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var, 0x1DE);
    EXPECT_THROW(sps->dummy_locked_var_2.setValueFromString("0xAA")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_locked_var_2, 0x8DA);

    // Hidden and volatile hidden parameters of root subtree cannot be set anymore after Lockdown phase
    EXPECT_THROW(sps->dummy_hidden_var.setValueFromString("0x0C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var, 0x1D);
    EXPECT_THROW(sps->dummy_hidden_var_2.setValueFromString("0x7C")); // Should not allow pre-lockdown phase
    EXPECT_EQUAL(sps->dummy_hidden_var_2, 0x8D);

    // Test to see that hidden root parameters show up in dump list of parameters after lockdown phase is applied
    sps_param_list = sps->dumpList();
    EXPECT_EQUAL(sps_param_list.find("dummy_hidden_var"), std::string::npos);
    EXPECT_EQUAL(sps_param_list.find("dummy_hidden_var_2"), std::string::npos);

    // Regular parameters can still be configured till TREE_FINALIZE.
    sps->resetReadCounts();
    EXPECT_THROW(sps->verifyAllRead()); // None of them read
    EXPECT_EQUAL(sps->length.getReadCount(), 0u);
    sps->length.ignore(); // Increment read count
    EXPECT_TRUE(sps->length.isIgnored());
    EXPECT_EQUAL(sps->length.getReadCount(), 0u);
    EXPECT_EQUAL(sps->test_bool.getReadCount(), 0u);
    EXPECT_THROW(sps->verifyAllRead()); // Not all of them read
    EXPECT_EQUAL(sps->length.getReadCount(), 0u); // still one
    sps->ignoreAll();
    EXPECT_NOTHROW(sps->verifyAllRead());
    EXPECT_EQUAL(sps->length.getReadCount(), 0u);
    EXPECT_EQUAL(sps->length.isIgnored(), true);
    EXPECT_EQUAL(sps->test_bool.getReadCount(), 0u);
    EXPECT_EQUAL(sps->test_bool.isIgnored(), true);
    std::cout << sps->length << std::endl;
    std::cout << (sps->length == 10) << std::endl; // Should increment during comparison
    EXPECT_EQUAL(sps->length.getReadCount(), 1u); // Should not be incremented in cout above
    sps->length.getNumValues(); // Should not increment read count because this is scalar
    EXPECT_EQUAL(sps->length.getReadCount(), 1u); // Should not be incremented in cout above
    EXPECT_EQUAL(sps->test_boolvec.getReadCount(), 0u);
    sps->test_boolvec.getNumValues(); // SHOULD increment read count because this is a vector
    EXPECT_EQUAL(sps->test_boolvec.getReadCount(), 1u);
    EXPECT_EQUAL(sps->myenum.getReadCount(), 0u);
    EXPECT_EQUAL(sps->myenum.isIgnored(), true);

    // Individual parameters

    std::cout << sps->length << " "
              << sps->length.getName() << " "
              << sps->length.getDesc() << " "
              << sps->length.getDefault() << " "
              << sps->length.getTypeName() << " "
              << std::endl;

    // Look at type of structured parameters

    std::cout << sps->test_stringvecvec << " "
              << sps->test_stringvecvec.getName() << " "
              << sps->test_stringvecvec.getDesc() << " "
              << sps->test_stringvecvec.getDefault() << " "
              << sps->test_stringvecvec.getTypeName() << " "
              << std::endl;

    // Check dimensions of vector and non-vector types
    EXPECT_EQUAL(sps->test_stringvecvec.getDimensionality(), 2);
    EXPECT_EQUAL(sps->test_stringvecvec.getVectorSizeAt({}), 4);
    EXPECT_EQUAL(sps->test_stringvecvec.getVectorSizeAt({0}), 1);
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({0,0}), "1");
    EXPECT_THROW(sps->test_stringvecvec.getItemValueFromString({0,1})); // Too deep
    EXPECT_EQUAL(sps->test_stringvecvec.getVectorSizeAt({1}), 2);
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({1,0}), "2");
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({1,1}), "3");
    EXPECT_THROW(sps->test_stringvecvec.getItemValueFromString({1,2})); // Too deep
    EXPECT_EQUAL(sps->test_stringvecvec.getVectorSizeAt({2}), 3);
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({2,0}), "4");
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({2,1}), "5");
    EXPECT_EQUAL(sps->test_stringvecvec.getItemValueFromString({2,2}), "6");
    EXPECT_THROW(sps->test_stringvecvec.getItemValueFromString({2,3})); // Too deep
    EXPECT_EQUAL(sps->test_stringvecvec.getVectorSizeAt({3}), 0);
    EXPECT_THROW(sps->test_stringvecvec.getItemValueFromString({3,0})); // Too deep
    EXPECT_EQUAL(sps->length.getDimensionality(), 0);
    EXPECT_EQUAL(sps->test_boolvec.getDimensionality(), 1);
    EXPECT_EQUAL(sps->test_stringvec.getDimensionality(), 1);

    bool sca_bool = false;
    int32_t sca_int32 = 0;
    uint32_t sca_uint32 = 0;
    int64_t sca_int64 = 0;
    uint64_t sca_uint64 = 0;
    double sca_double = 0.0;
    std::string sca_string;
    std::string sca_charptr;

    std::vector<bool> vec_bool;
    std::vector<int32_t> vec_int32;
    std::vector<uint32_t> vec_uint32;
    std::vector<int64_t> vec_int64;
    std::vector<uint64_t> vec_uint64;
    std::vector<double> vec_double;
    std::vector<std::string> vec_string;

    bool tmp_bool = false;

    EXPECT_NOTHROW(sca_bool = sps->test_bool);
    EXPECT_TRUE(sca_bool == true);
    EXPECT_EQUAL(sps->test_bool, true);
    EXPECT_EQUAL(gps->getParameterValueAs<bool>("test_bool"), true);
    EXPECT_EQUAL(gps->getParameter("test_bool")->getValueAs<bool>(), true);
    EXPECT_THROW(gps->getParameter("test_bool")->getValueAs<uint32_t>());
    EXPECT_NOTHROW(sca_int32 = sps->test_int32);
    EXPECT_TRUE(sca_int32 == -1);

    EXPECT_EQUAL(sps->test_int8, 0xf);
    EXPECT_EQUAL(gps->getParameterValueAs<int8_t>("test_int8"), -1);
    EXPECT_EQUAL(gps->getParameter("test_int8")->getValueAs<int8_t>(), -1);
    EXPECT_EQUAL(sps->test_uint8, -1);
    EXPECT_EQUAL(gps->getParameterValueAs<uint8_t>("test_uint8"), -1);
    EXPECT_EQUAL(gps->getParameter("test_uint8")->getValueAs<uint8_t>(), -1);

    EXPECT_EQUAL(sps->test_int32, -1);
    EXPECT_EQUAL(gps->getParameterValueAs<int32_t>("test_int32"), -1);
    EXPECT_EQUAL(gps->getParameter("test_int32")->getValueAs<int32_t>(), -1);
    EXPECT_NOTHROW(sca_uint32 = sps->test_uint32);
    EXPECT_TRUE(sca_uint32 == 2);
    EXPECT_EQUAL(sps->test_uint32, 2);
    EXPECT_EQUAL(gps->getParameterValueAs<uint32_t>("test_uint32"), 2);
    EXPECT_EQUAL(gps->getParameter("test_uint32")->getValueAs<uint32_t>(), 2);
    EXPECT_NOTHROW(sca_int64 = sps->test_int64);
    EXPECT_TRUE(sca_int64 == -3);
    EXPECT_NOTHROW(sca_uint64 = sps->test_uint64);
    EXPECT_TRUE(sca_uint64 == 4);
    EXPECT_NOTHROW(sca_double = sps->test_double);
    EXPECT_TRUE(sca_double == 5.6);
    // TODO: Support string copy constrctor.
    EXPECT_NOTHROW(sca_string = sps->test_string);
    EXPECT_NOTHROW(sca_string = sps->test_string.getValue());
    EXPECT_TRUE(sca_string == "this is a test string");
    EXPECT_TRUE(sps->test_string == "this is a test string");
    EXPECT_NOTHROW((void)((std::string)sps->test_string == "this is a test string"));
    EXPECT_NOTHROW( EXPECT_EQUAL(gps->getParameterValueAs<std::string>("test_string"), "this is a test string") );
    EXPECT_THROW(gps->getParameterValueAs<std::string>("this does not exist and is an invalid name anyway"));
    EXPECT_THROW(gps->getParameterValueAs<uint32_t>("test_string"));

    EXPECT_NOTHROW(vec_bool = sps->test_boolvec.getValue());
    EXPECT_NOTHROW(vec_bool = sps->test_boolvec);
    EXPECT_TRUE(sps->test_boolvec == std::vector<bool>({false, false, true, true, false, true})); // Parameter Write after read
    EXPECT_NOTHROW(vec_bool = (std::vector<bool>)sps->test_boolvec);
    EXPECT_EQUAL(sps->myenum, MY_ENUM_DEFAULT);

    sps->myenum.setValueFromString("0"); // Parameter Write after read
    EXPECT_EQUAL(sps->myenum, MY_ENUM_DEFAULT);
    sps->myenum.setValueFromString("1");
    EXPECT_EQUAL(sps->myenum, MY_ENUM_OTHER);
    EXPECT_THROW(sps->myenum.setValueFromString("2"));
    EXPECT_EQUAL(sps->myenum.getTypeName(), "MyEnum");

    EXPECT_NOTHROW(vec_int32 = sps->test_int32vec.getValue());
    EXPECT_NOTHROW(vec_int32 = sps->test_int32vec);
    EXPECT_NOTHROW(vec_uint32 = sps->test_uint32vec.getValue());
    EXPECT_NOTHROW(vec_uint32 = sps->test_uint32vec);
    EXPECT_NOTHROW(vec_int64 = sps->test_int64vec.getValue());
    EXPECT_NOTHROW(vec_int64 = sps->test_int64vec);
    EXPECT_NOTHROW(vec_uint64 = sps->test_uint64vec.getValue());
    EXPECT_NOTHROW(vec_uint64 = sps->test_uint64vec);
    EXPECT_NOTHROW(vec_double = sps->test_doublevec.getValue());
    EXPECT_NOTHROW(vec_double = sps->test_doublevec);
    EXPECT_NOTHROW(vec_double = (std::vector<double>)sps->test_doublevec);
    EXPECT_NOTHROW(vec_string = sps->test_stringvec.getValue());
    EXPECT_NOTHROW(vec_string = sps->test_stringvec);
    EXPECT_NOTHROW(vec_string = (std::vector<std::string>)sps->test_stringvec);


    // Getting Parameters

    sparta::ParameterBase const * p = gps->getParameter("length");
    std::cout << *p << " "
              << p->getName() << " "
              << p->getDesc() << " "
              << p->getTypeName() << " "
              << std::endl;


    // Finding Parameters by pattern
    std::vector<sparta::ParameterBase*> result1;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("length", result1)), 1u);
    EXPECT_EQUAL(result1.size(), (size_t)1);
    std::cout << "result of search for \"length\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("lengt*", result1)), 1u);
    EXPECT_EQUAL(result1.size(), (size_t)1);
    std::cout << "result of search for \"lengt*\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("test_*", result1)), 17u);
    EXPECT_EQUAL(result1.size(), (size_t)15);
    std::cout << "result of search for \"test_*\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*st_*", result1)), 17u);
    EXPECT_EQUAL(result1.size(), (size_t)15);
    std::cout << "result of search for \"*st_*\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*st_*vec", result1)), 8u);
    EXPECT_EQUAL(result1.size(), (size_t)8);
    std::cout << "result of search for \"*st_*vec\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*64vec", result1)), 2u);
    EXPECT_EQUAL(result1.size(), (size_t)2);
    std::cout << "result of search for \"*64vec\": " << result1 << std::endl << std::endl;

    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*", result1)), (uint32_t)EXPECTED_NUM_PARAMS);
    EXPECT_EQUAL(result1.size(), (size_t)EXPECTED_NUM_PARAMS);
    std::cout << "result of search for \"*\": " << result1 << std::endl << std::endl;

    // Down into all params, then up from each, then search for "length"
    // Algorithm will find a "legnth" through each path, so set will be
    // N "length" parameters where N is the total number of parameters in
    // this parameter set.
    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*..length", result1)), (uint32_t)EXPECTED_NUM_PARAMS);
    EXPECT_EQUAL(result1.size(), (size_t)EXPECTED_NUM_PARAMS);
    std::cout << "result of search for \"*..length\": " << result1 << std::endl << std::endl;

    // Down into all params, then up from each, then search for test_*.
    // Result count should be total params multiplied by number of params
    // beginning with test_
    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*..test_*", result1)), (uint32_t)EXPECTED_NUM_PARAMS * 15);
    EXPECT_EQUAL(result1.size(), (size_t)EXPECTED_NUM_PARAMS * 15);
    std::cout << "result of search for \"*..test_*\": " << result1 << std::endl << std::endl;

    // Will find test_int64, test_uint64, test_int64vec, and test_uint64vec
    result1.clear();
    EXPECT_EQUAL((gps->findParameters("test_?int64*", result1)), (uint32_t)4);
    EXPECT_EQUAL(result1.size(), (size_t)4);
    std::cout << "result of search for \"test_?int64*\": " << result1 << std::endl << std::endl;

    // Will find test_uint64vec
    result1.clear();
    EXPECT_EQUAL((gps->findParameters("test_+int64+", result1)), (uint32_t)1);
    EXPECT_EQUAL(result1.size(), (size_t)1);
    std::cout << "result of search for \"test_+int64+\": " << result1 << std::endl << std::endl;

    // Will find test_uint64vec
    result1.clear();
    EXPECT_EQUAL((gps->findParameters("*st_uint64+", result1)), (uint32_t)1);
    EXPECT_EQUAL(result1.size(), (size_t)1);
    std::cout << "result of search for \"*st_uint64+\": " << result1 << std::endl << std::endl;


    // Scalar type modification

    tmp_bool = !sps->test_bool;
    sps->test_bool = tmp_bool; // Write after read

    sps->test_bool = !sps->test_bool;

    std::cout << sca_bool << std::endl;
    std::cout << sca_int32 << std::endl;
    std::cout << sca_uint32 << std::endl;
    std::cout << sca_int64 << std::endl;
    std::cout << sca_uint64 << std::endl;
    std::cout << sca_double << std::endl;
    std::cout << sca_string << std::endl;

    std::cout << sps->test_bool << std::endl;
    std::cout << sps->test_int32 << std::endl;
    std::cout << sps->test_uint32 << std::endl;
    std::cout << sps->test_int64 << std::endl;
    std::cout << sps->test_uint64 << std::endl;
    std::cout << sps->test_double << std::endl;
    std::cout << sps->test_string << std::endl;


    // Vector type printing

    std::cout << vec_bool << std::endl;
    std::cout << vec_int32 << std::endl;
    std::cout << vec_uint32 << std::endl;
    std::cout << vec_int64 << std::endl;
    std::cout << vec_uint64 << std::endl;
    std::cout << vec_double << std::endl;
    std::cout << vec_string << std::endl;

    std::cout << sps->test_boolvec << std::endl;
    std::cout << sps->test_int32vec << std::endl;
    std::cout << sps->test_uint32vec << std::endl;
    std::cout << sps->test_int64vec << std::endl;
    std::cout << sps->test_uint64vec << std::endl;
    std::cout << sps->test_doublevec << std::endl;
    std::cout << sps->test_stringvec << std::endl;

    // Check quoting of strings
    std::cout << "String quoting:" << std::endl;
    // Scalar string:
    std::cout << "Original: " << sps->test_string.getValueAsString() << std::endl;
    auto old = sps->test_string.setStringQuote("'");
    std::cout << "Quoted: " << sps->test_string.getValueAsString() << std::endl;
    sps->test_string.setStringQuote(old);
    std::cout << "Original (again): " << sps->test_string.getValueAsString() << std::endl;
    // Vector string:
    std::cout << "Original: " << sps->test_stringvec.getValueAsString() << std::endl;
    old = sps->test_stringvec.setStringQuote("'");
    std::cout << "Quoted: " << sps->test_stringvec.getValueAsString() << std::endl;
    sps->test_stringvec.setStringQuote(old);
    std::cout << "Original (again): " << sps->test_stringvec.getValueAsString() << std::endl;
    // Vector of vectors of strings:
    std::cout << "Original: " << sps->test_stringvecvec.getValueAsString() << std::endl;
    old = sps->test_stringvecvec.setStringQuote("%%");
    std::cout << "Quoted: " << sps->test_stringvecvec.getValueAsString() << std::endl;
    sps->test_stringvecvec.setStringQuote(old);
    std::cout << "Original (again): " << sps->test_stringvecvec.getValueAsString() << std::endl;


    // Introspection

    // Iterate all names (parameters)
    std::cout << "Names:";
    for(const std::string& n : gps->getNames()){
        std::cout << " " << n;
    }
    std::cout << std::endl << std::endl;


    // Iteration

    // Generally, iteration should only be done on the generic parameter set
    // since subclasses can hide iteration member [functions] like 'begin', 'end', etc.

    // Iterate with preincrement
    std::cout << "Params (preinc):";
    sparta::ParameterSet::iterator itr;
    for(itr = gps->begin(); itr != gps->end(); ++itr){
        std::cout << " " << *(*itr);
    }
    std::cout << std::endl << std::endl;

    // Iterate with postincrement
    std::cout << "Params (postinc):";
    sparta::ParameterSet::iterator itr2;
    for(itr2 = gps->begin(); itr2 != gps->end(); itr2++){
        std::cout << " " << *(*itr2);
    }
    std::cout << std::endl << std::endl;


    // No-Copying allowed
    // None of this is legal.

    /*SampleDevice::SampleDeviceParameterSet sps_copy("dummy");
    sps_copy = *sps;

    SampleDevice::SampleDeviceParameterSet sps_copy2(*sps);

    sparta::ParmeterSet gps_copy("dummy");
    gps_copy = *gps;

    sparta::ParameterSet sps_copy2(*gps);

    SampleDevice::SampleDeviceParameterSetWithCopyMethods sps_wcm;
    SampleDevice::SampleDeviceParameterSetWithCopyMethods sps_wcm_copy;
    sps_wcm_copy = sps_wcm;

    SampleDevice::SampleDeviceParameterSetWithCopyMethods sps_wcm_copy2(sps_wcm);
    */

    // Immediate (indpendent) Validation

    std::string sps_errs;
    EXPECT_TRUE(sps->validateIndependently(sps_errs));
    EXPECT_TRUE(sps_errs == "");
    std::cout << sps_errs << std::endl;

    std::string gps_errs;
    EXPECT_TRUE(gps->validateIndependently(gps_errs));
    EXPECT_TRUE(gps_errs == "");
    std::cout << gps_errs << std::endl;


    // Callback-based (dependent) Validation

    sps_errs = "";
    EXPECT_TRUE(sps->validateDependencies(0, sps_errs));
    EXPECT_TRUE(sps_errs == "");
    std::cout << sps_errs << std::endl;

    gps_errs = "";
    EXPECT_TRUE(gps->validateDependencies(0, gps_errs));
    EXPECT_TRUE(gps_errs == "");
    std::cout << gps_errs << std::endl;


    // Print out parameter sets

    std::cout << "Specific ParameterSet:" << std::endl << sps.get() << std::endl << sps->dumpList() << std::endl;
    std::cout << "General ParameterSet:" << std::endl << gps << std::endl << sps->dumpList() << std::endl;
    std::cout << "Specific ParameterSet for root left child:" << std::endl << sps_left.get() << std::endl << sps_left->dumpList() << std::endl;
    std::cout << "Speciifc ParameterSet for root right child:" << std::endl << sps_right.get() << std::endl << sps_right->dumpList() << std::endl;


    // Modify the parameters and look for callbacks
    EXPECT_TRUE(sps->ypsA_var1 == 1);
    EXPECT_TRUE(sps->xpsA_var2 == 2);
    sps->zpsA_var0 = 10;  // Writing zpsA_var0 will update paA val[1,2]
    EXPECT_EQUAL(sps->ypsA_var1, 5);
    EXPECT_EQUAL(sps->xpsA_var2, 6);

    sps->ypsA_var1 = 10;  // Writing zpsA_var1 will update paA val[1,2] (Write after read for both)
    EXPECT_EQUAL(sps->ypsA_var1, 10);
    EXPECT_EQUAL(sps->xpsA_var2, 8);

    // Create Units

    DeviceWithParams* d0 = 0;
    EXPECT_NOTHROW(d0 = createDevice("dev0", sps.get()));
    delete d0;

    DeviceWithParams* d1 = 0;
    EXPECT_NOTHROW(d1 = createDevice("dev1", dynamic_cast<SampleDevice::SampleDeviceParameterSet*>(gps)));
    delete d1;


    // Test ParameterBase::equals()
    sparta::ParameterBase *test_bool1 = sps->getParameter("test_bool");
    sparta::ParameterBase *test_bool2 = sps->getParameter("test_bool");
    EXPECT_TRUE(test_bool1->equals(*test_bool2));

    sparta::ParameterBase *test_uint32_1 = sps->getParameter("test_uint32");
    sparta::ParameterBase *test_uint32_2 = sps->getParameter("test_uint32");
    EXPECT_TRUE(test_uint32_1->equals(*test_uint32_2));

    sparta::ParameterBase *dummy00 = sps->getParameter("dummy00");
    sparta::ParameterBase *dummy01 = sps->getParameter("dummy01");
    EXPECT_FALSE(dummy00->equals(*dummy01));
    // Done

    sim->finalizeTree();
    sim.reset();

    REPORT_ERROR;

    return ERROR_CODE;
}
