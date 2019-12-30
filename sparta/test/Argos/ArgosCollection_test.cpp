// <Scheduleable> -*- C++ -*-


#include "sparta/utils/SpartaTester.hpp"

#include "sparta/collection/Collectable.hpp"
#include "sparta/collection/PipelineCollector.hpp"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"

#include "sparta/log/Tap.hpp"

TEST_INIT;

class DummyObject
{
public:
    std::string msg = "<initial_value>";

};

std::ostream & operator<<(std::ostream & os, const DummyObject & dumb) {
    os << dumb.msg;
    return os;
}

class ObjectClk : public sparta::TreeNode
{
public:
    ObjectClk(TreeNode * node, const std::string & name) :
        sparta::TreeNode(node, name, "A random pretend head node for tests" ),
        pc2_var(1000),
        pc1_(this, name + "0_int_manual_collectable"),
        pc1_always_close_(this, name + "0_int_manual_collectable_will_close"),
        pc2_(this, name + "1_int_local_collectable", &pc2_var),
        pc3_(this, name + "2_dummy_collectable", &pc3_dummy),
        es_(this),
        ev_update_(&es_, "update", CREATE_SPARTA_HANDLER(ObjectClk, updateCollectables))
    {
        pc1_.initialize(1000);
        pc1_always_close_.initialize(1000);
        pc3_dummy.msg = name;

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(ObjectClk, startup));
    }

    void startup() {
        ev_update_.schedule(1);
    }

    void updateCollectables()
    {
        ++pc2_var;
        pc1_.collect(pc2_var);
        if(toggle & 1) {
            pc1_always_close_.collectWithDuration(pc2_var, 1);
        }
        ++toggle;
        std::stringstream ss;
        ss << pc3_dummy.msg << " " << pc2_var;
        pc3_dummy.msg = ss.str();
        ev_update_.schedule(1);
    }

private:
    DummyObject pc3_dummy;
    uint64_t pc2_var;
    sparta::collection::Collectable<uint64_t>    pc1_;
    sparta::collection::Collectable<uint64_t>    pc1_always_close_;
    sparta::collection::Collectable<uint64_t>    pc2_;
    sparta::collection::Collectable<DummyObject> pc3_;
    sparta::EventSet es_;
    sparta::Event<sparta::SchedulingPhase::Update> ev_update_;
    uint32_t toggle = 0;
};


int main()
{

    // Start with a root
    sparta::RootTreeNode root_node("root");
    sparta::RootTreeNode root_clks("clocks",
                                 "Clock Tree Root",
                                 root_node.getSearchScope());
    sparta::ClockManager cm;
    sparta::Clock::Handle root_clk = cm.makeRoot(&root_clks);
    sparta::Clock::Handle clk_1000 = cm.makeClock("clk_1000", root_clk, 1000.0);
    sparta::Clock::Handle clk_100  = cm.makeClock("clk_100",  root_clk,  100.0);
    sparta::Clock::Handle clk_10   = cm.makeClock("clk_10",   root_clk,   10.0);
    cm.normalize();

    root_node.setClock(root_clk.get());

    sparta::TreeNode obj1000_tn(&root_node, "obj1000", "obj1000 desc");
    sparta::TreeNode obj100_tn(&root_node, "obj100", "obj100 desc");
    sparta::TreeNode obj10_tn(&root_node, "obj10", "obj10 desc");

    obj1000_tn.setClock(clk_1000.get());
    obj100_tn.setClock(clk_100.get());
    obj10_tn.setClock(clk_10.get());

    // Add some objects
    ObjectClk obj1000(&obj1000_tn, "level1_0");
    ObjectClk obj100 (&obj100_tn,  "level1_1");
    ObjectClk obj10  (&obj10_tn,   "level1_2");

    root_node.enterConfiguring();
    root_node.enterFinalized();

     // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
     //                                sparta::log::categories::DEBUG, std::cout);

    sparta::collection::PipelineCollector pc("testPipe", 0, root_node.getClock(), &root_node);

    sparta::Scheduler::getScheduler()->finalize();

    pc.startCollection(&root_node);

    pc.printMap();

    sparta::Scheduler::getScheduler()->run(100000);
    pc.stopCollection();
    sparta::Scheduler::getScheduler()->run(100000);
    pc.startCollection(&root_node);
    sparta::Scheduler::getScheduler()->run(100000);

    pc.stopCollection(&root_node);
    pc.destroy();

    root_node.enterTeardown();
    root_clks.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;

}
