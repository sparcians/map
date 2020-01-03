

#include <iostream>
#include <inttypes.h>
#include <memory>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/ports/Bus.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

using namespace std;

class MyLeftBus : public sparta::Bus
{
public:
    MyLeftBus(sparta::TreeNode * node,
              const std::string & name,
              const std::string & grp,
              const sparta::TreeNode::group_idx_type idx,
              const std::string & desc, bool skip_good = false) :
        sparta::Bus(node, name, grp, idx, desc),
        addr_in(&getPortSet(), "addr_in"),
        sig_out(&getPortSet(), "token_dealloc_out"),
        inv_out(&getPortSet(), "inv_out")
    {
        if(!skip_good) {
            good_out.reset(new sparta::SignalOutPort(&getPortSet(), "good_out"));
        }
    }

    MyLeftBus(sparta::TreeNode * node, bool skip_good = false) :
        MyLeftBus(node,
                  "MyLeftBus",
                  sparta::TreeNode::GROUP_NAME_NONE,
                  sparta::TreeNode::GROUP_IDX_NONE,
                  "MyLeftBus Description", skip_good)
    { }

    void checkBinding() const {
        EXPECT_TRUE(addr_in.isBound());
        EXPECT_TRUE(good_out->isBound());
    }

    sparta::DataInPort<uint32_t> addr_in;
    std::unique_ptr<sparta::SignalOutPort> good_out;
    sparta::SignalOutPort    sig_out;
    sparta::SignalOutPort    inv_out;
};

class MyRightBus : public sparta::Bus
{
public:
    MyRightBus(sparta::TreeNode * node,
               const std::string & name,
               const std::string & grp,
               const sparta::TreeNode::group_idx_type idx,
               const std::string & desc) :
        sparta::Bus(node, name, grp, idx, desc),
        good_in (&getPortSet(), "good_in"),
        addr_out(&getPortSet(),"addr_out"),
        sig_in(&getPortSet(), "token_dealloc_in"), // A wonky, but allowable name
        inv_in(&getPortSet(), "inv_in")
    {
    }

    MyRightBus(sparta::TreeNode * node) :
        MyRightBus(node, "MyRightBus",
                   sparta::TreeNode::GROUP_NAME_NONE,
                   sparta::TreeNode::GROUP_IDX_NONE,
                   "MyRightBus Description")
    { }

    void checkBinding() const {
        EXPECT_TRUE(good_in.isBound());
        EXPECT_TRUE(addr_out.isBound());
    }

    sparta::SignalInPort      good_in;
    sparta::DataOutPort<uint32_t> addr_out;
    sparta::SignalInPort      sig_in;
    sparta::SignalInPort      inv_in;
};

class MyWackyBus : public sparta::Bus
{
public:
    MyWackyBus(sparta::TreeNode * node) :
        sparta::Bus(node, "MyWackyBus",
                  sparta::TreeNode::GROUP_NAME_NONE,
                  sparta::TreeNode::GROUP_IDX_NONE,
                  "MyWackyBus Description"),
        good_in (&getPortSet(), "good_bus_in"),
        in_good (&getPortSet(), "in_good_bus"),
        addr_out(&getPortSet(), "addr_out")
    {
    }
    sparta::SignalInPort      good_in;
    sparta::SignalInPort      in_good;
    sparta::DataOutPort<uint32_t> addr_out;
};

void test_good_bind()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::Clock clk("dummy", &sched);
    rtn.setClock(&clk);
    sparta::TreeNode lrsrc(&rtn, "lrsrc", "Left Resource");
    sparta::TreeNode rrsrc(&rtn, "rrsrc", "Right Resource");
    sparta::BusSet lbs(&lrsrc, "My Left Bus Set");
    sparta::BusSet rbs(&rrsrc, "My Right Bus Set");
    MyLeftBus  lbus(&lbs);
    MyRightBus rbus(&rbs);

    std::cout << "(GOOD) Before binding: \n" << rtn.renderSubtree() << std::endl;

    // get the buses and bind them together
    sparta::Bus * bus1 = rtn.getChildAs<sparta::Bus>("lrsrc.buses.MyLeftBus");
    EXPECT_TRUE(bus1 == &lbus);
    sparta::Bus * bus2 = rtn.getChildAs<sparta::Bus>("rrsrc.buses.MyRightBus");
    EXPECT_TRUE(bus2 == &rbus);

    // Test precedence with the Bus
    sparta::EventSet event_set(&rtn);
    sparta::Event<sparta::SchedulingPhase::Tick>  tick_event(&event_set, "tick_event",
                                                         sparta::SpartaHandler("dummy"));
    sparta::UniqueEvent<sparta::SchedulingPhase::PortUpdate>  pu_event(&event_set, "pu_event",
                                                                   sparta::SpartaHandler("dummy"));

    // This means that the tick_event must come before all OutPorts in
    // lbus
    tick_event >> lbus;

    // This means that the pu_event must come before all OutPorts in
    // rbus
    pu_event >> rbus;

    // This means that the lbus InPorts must come before the tick event
    lbus >> tick_event;

    // This means that the rbus InPorts must come before the pu pevent
    rbus >> pu_event;

    EXPECT_NOTHROW(sparta::bind(bus1, bus2));
    std::cout << "(GOOD) After binding: \n" << rtn.renderSubtree() << std::endl;

    rtn.enterTeardown();
}

// Test the bad binding...
void test_bad_bind()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::Clock clk("dummy", &sched);
    rtn.setClock(&clk);
    sparta::TreeNode lrsrc(&rtn, "lrsrc", "Left Resource");
    sparta::TreeNode rrsrc(&rtn, "rrsrc", "Right Resource");
    sparta::BusSet lbs(&lrsrc, "My Left Bus Set");
    sparta::BusSet rbs(&rrsrc, "My Right Bus Set");
    MyLeftBus  lbus(&lbs, true); // Screw up the name of one of the ports
    MyRightBus rbus(&rbs);
    MyWackyBus wbus(&rbs);

    std::cout << "(BAD) Before binding: \n" << rtn.renderSubtree() << std::endl;

    // get the buses and bind them together
    sparta::Bus * bus1 = rtn.getChildAs<sparta::Bus>("lrsrc.buses.MyLeftBus");
    EXPECT_TRUE(bus1 == &lbus);
    sparta::Bus * bus2 = rtn.getChildAs<sparta::Bus>("rrsrc.buses.MyRightBus");
    EXPECT_TRUE(bus2 == &rbus);
    sparta::Bus * bus3 = rtn.getChildAs<sparta::Bus>("rrsrc.buses.MyWackyBus");
    EXPECT_TRUE(bus3 == &wbus);

    // sparta::bind(bus1, bus2);
    // sparta::bind(bus1, bus3);
    EXPECT_THROW(sparta::bind(bus1, bus2)); // Should fail (lbus constructed differently)
    EXPECT_THROW(sparta::bind(bus1, bus3));

    std::cout << "(BAD) After binding: \n" << rtn.renderSubtree() << std::endl;

    rtn.enterTeardown();
}

void test_multiple_buses()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::Clock clk("dummy", &sched);
    rtn.setClock(&clk);
    sparta::TreeNode lrsrc(&rtn, "lrsrc", "Left Resource");
    sparta::TreeNode rrsrc(&rtn, "rrsrc", "Right Resource");
    sparta::BusSet lbs(&lrsrc, "My Left Bus Set");
    sparta::BusSet rbs(&rrsrc, "My Right Bus Set");
    MyLeftBus  lbus1(&lbs, "MyLeftBus_0", "MyLeftBus", 0, "Left Bus 0");
    MyLeftBus  lbus2(&lbs, "MyLeftBus_1", "MyLeftBus", 1, "Left Bus 1");
    MyRightBus rbus1(&rbs, "MyRightBus_0", "MyRightBus", 0, "Right Bus 0");
    MyRightBus rbus2(&rbs, "MyRightBus_1", "MyRightBus", 1, "Right Bus 1");

    std::cout << "(MUL) Before binding: \n" << rtn.renderSubtree() << std::endl;

    // get the buses and bind them together
    sparta::Bus * bus1 = rtn.getChildAs<sparta::Bus>("lrsrc.buses.MyLeftBus_0");
    EXPECT_TRUE(bus1 == &lbus1);
    sparta::Bus * bus2 = rtn.getChildAs<sparta::Bus>("rrsrc.buses.MyRightBus_0");
    EXPECT_TRUE(bus2 == &rbus1);

    EXPECT_NOTHROW(sparta::bind(bus1, bus2));
    bus1 = rtn.getChildAs<sparta::Bus>("lrsrc.buses.MyLeftBus_1");
    EXPECT_TRUE(bus1 == &lbus2);
    bus2 = rtn.getChildAs<sparta::Bus>("rrsrc.buses.MyRightBus_1");
    EXPECT_TRUE(bus2 == &rbus2);

    EXPECT_NOTHROW(sparta::bind(bus1, bus2));

    lbus1.checkBinding();
    lbus2.checkBinding();
    rbus1.checkBinding();
    rbus2.checkBinding();

    std::cout << "(MUL) After binding: \n" << rtn.renderSubtree() << std::endl;

    rtn.enterTeardown();
}

//____________________________________________________________
// MAIN
int main()
{
    // Bus uses this string function -- test it here
    std::string ret = sparta::utils::strip_string_pattern("in", "in_good");
    EXPECT_EQUAL(ret, "_good");
    ret = sparta::utils::strip_string_pattern("in", "good_in");
    EXPECT_EQUAL(ret, "good_");
    ret = sparta::utils::strip_string_pattern("in", "in_goodin_in");
    EXPECT_EQUAL(ret, "_goodin_");
    ret = sparta::utils::strip_string_pattern("in", "_goodin_");
    EXPECT_EQUAL(ret, "_goodin_");

    test_good_bind();
    test_bad_bind();
    test_multiple_buses();
    REPORT_ERROR;

    return ERROR_CODE;
}
