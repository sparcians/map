#include "sparta/sparta.hpp"

#include <iostream>
#include <inttypes.h>

#include "systemc.h"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/SysCSpartaSchedulerAdapter.hpp"

TEST_INIT;

/*
 * Hammers on the scheduler
 */
uint64_t expected_cycle = 1;

typedef sparta::DataInPort<uint32_t> DataInPortType;
typedef sparta::DataOutPort<uint32_t> DataOutPortType;
bool first_called = false;
uint32_t events_fired = 0;

uint32_t group_cursor = 0;
const uint32_t offset = 1;
uint32_t expected_groups[] = {
    1 + offset,
    2 + offset,
    2 + offset,
    3 + offset,
    3 + offset,
    3 + offset,
    3 + offset,
    4 + offset
};
uint32_t expected_fired = sizeof(expected_groups)/sizeof(expected_groups[0]);

class InAndDataOutPort : sparta::TreeNode
{
    sparta::PortSet ps_;
public:
    InAndDataOutPort(sparta::TreeNode *parent, const std::string& name, sparta::Clock* clk, uint32_t group) :
        TreeNode(parent, name, "description"),
        ps_(this, "inanddataoutport_ps"),
        name_(name),
        in_port_(&ps_, "in_"+name),
        out_port_(&ps_, "out_"+name),
        clk_(clk),
        expected_group_(group)
    {
        //Bind a callback to the inport.
        in_port_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(InAndDataOutPort, callback, uint32_t));
    }

    void callback(const uint32_t &)
    {
        EXPECT_EQUAL(in_port_.getTickEvent().getGroupID(), expected_groups[group_cursor]);
        ++group_cursor;

        std::cout << "expected group: " << expected_group_ << " " << name_ << std::endl;
        ++events_fired;
    }


    //!Set this object's in port as dependent upon another port.
    void addDependency(InAndDataOutPort& helper)
    {
        //helper.getDataInPort().precedes(in_port_);
        helper.getDataInPort() >> in_port_;
        sparta::bind(out_port_, in_port_);
    }
    void bind()
    {
        sparta::bind(out_port_, in_port_);
    }
    DataInPortType& getDataInPort() { return in_port_;}
    DataOutPortType& getDataOutPort() {return out_port_; }

    void fire()
    {
        out_port_.send(5, clk_, 50);
    }

    uint32_t getPrecedenceGroup()
    {
        return in_port_.getScheduleable().getGroupID();
    }
private:
    std::string name_;
    DataInPortType in_port_;
    DataOutPortType out_port_;
    sparta::Clock* clk_;
    uint32_t expected_group_;

};

//!Set up and test that port's are fired in order of their dependencies.
class DependencyTest
{
public:
    // Dependency tree being built (in-port group IDs for each node)
    //
    // X (3) --.------------------> B (2) -.--> A (1)
    // Y (3) --|                           |
    // C (3) --'  F (4) -> Z (3) -> W (2) -'
    //
    //
    DependencyTest(sparta::TreeNode * parent, sparta::Clock* clk) :
        a(parent, "A", clk, expected_groups[0]),
        b(parent, "B", clk, expected_groups[1]),
        w(parent, "W", clk, expected_groups[2]),
        z(parent, "Z", clk, expected_groups[3]),
        x(parent, "X", clk, expected_groups[4]),
        y(parent, "Y", clk, expected_groups[5]),
        c(parent, "C", clk, expected_groups[6]),
        f(parent, "F", clk, expected_groups[7]),
        clk_(clk)
    {

        //Build up some precedence
        a.bind();
        b.addDependency(a);
        w.addDependency(a);
        z.addDependency(w);
        f.addDependency(z);
        x.addDependency(b);
        y.addDependency(b);
        c.addDependency(b);
    }
    void scheduleEvents()
    {
        clk_->scheduleEvent(CREATE_SPARTA_HANDLER(DependencyTest, fire), 1);
    }
    void checkDagFinalization()
    {
        std::cout << "checking the precedence groupings were assigned correctly"
            " to the ports" << std::endl;
        EXPECT_EQUAL(a.getPrecedenceGroup(), expected_groups[0]);
        EXPECT_EQUAL(b.getPrecedenceGroup(), expected_groups[1]);
        EXPECT_EQUAL(w.getPrecedenceGroup(), expected_groups[2]);
        EXPECT_EQUAL(z.getPrecedenceGroup(), expected_groups[3]);
        EXPECT_EQUAL(x.getPrecedenceGroup(), expected_groups[4]);
        EXPECT_EQUAL(y.getPrecedenceGroup(), expected_groups[5]);
        EXPECT_EQUAL(c.getPrecedenceGroup(), expected_groups[6]);
        EXPECT_EQUAL(f.getPrecedenceGroup(), expected_groups[7]);

        EXPECT_EQUAL(sparta::Scheduler::getScheduler()->getDAG()->numGroups(), 7);
    }

    void fire()
    {
        //Call a fire on a bunch of ports all at the same cycle,
        //see if our output is what was expected.
        EXPECT_EQUAL(a.getPrecedenceGroup(), expected_groups[0]);
        EXPECT_EQUAL(b.getPrecedenceGroup(), expected_groups[1]);
        EXPECT_EQUAL(w.getPrecedenceGroup(), expected_groups[2]);
        EXPECT_EQUAL(z.getPrecedenceGroup(), expected_groups[3]);
        EXPECT_EQUAL(x.getPrecedenceGroup(), expected_groups[4]);
        EXPECT_EQUAL(y.getPrecedenceGroup(), expected_groups[5]);
        EXPECT_EQUAL(c.getPrecedenceGroup(), expected_groups[6]);
        EXPECT_EQUAL(f.getPrecedenceGroup(), expected_groups[7]);

        c.fire();
        a.fire();
        x.fire();
        f.fire();
        y.fire();
        z.fire();
        b.fire();
        w.fire();
    }

    //Create some helper objects.
    InAndDataOutPort a;
    InAndDataOutPort b;
    InAndDataOutPort w;
    InAndDataOutPort z;
    InAndDataOutPort x;
    InAndDataOutPort y;
    InAndDataOutPort c;
    InAndDataOutPort f;
    sparta::Clock* clk_;
};

int sc_main(int, char *[])
{
    sparta::Clock clk("clock");
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->getCurrentTick() == 0);
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->isRunning() == 0);

    // Test scheduler logging (general test of logging on global TreeNodes)
    // First, find the scheduler node
    std::vector<sparta::TreeNode*> roots;
    sparta::TreeNode::getVirtualGlobalNode()->findChildren(sparta::Scheduler::NODE_NAME, roots);
    EXPECT_EQUAL(roots.size(), 1);
    EXPECT_NOTHROW(EXPECT_EQUAL(sparta::TreeNode::getVirtualGlobalNode()->getChild(sparta::Scheduler::NODE_NAME), roots.at(0)));
    // Get info messages from the scheduler node and send them to this file
    sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
                                   sparta::log::categories::DEBUG, "scheduler.debug");
    sparta::log::Tap t(roots.at(0), "info", "scheduler.log.basic");

    sparta::RootTreeNode rtn("dummyrtn");
    rtn.setClock(&clk);

    //Test port dependency
    DependencyTest test(&rtn, &clk);
    sparta::Scheduler::getScheduler()->finalize();
    //sparta::DAG::getDAG()->print(std::cout);
    test.checkDagFinalization();
    test.scheduleEvents();
    sparta::Scheduler::getScheduler()->printNextCycleEventTree(std::cout, 0, 0);

    sparta::SysCSpartaSchedulerAdapter sysc_sched_runner;
    sysc_sched_runner.run(53);
    sparta_assert(sc_core::sc_time_stamp().value() == 53);

    EXPECT_EQUAL(events_fired, expected_fired);

    // Compare the schedler log output with the expected to ensure it is logging
    EXPECT_FILES_EQUAL("scheduler.log.basic.EXPECTED", "scheduler.log.basic");

    rtn.enterTeardown();

    // Returns error
    REPORT_ERROR;
    return ERROR_CODE;
}
