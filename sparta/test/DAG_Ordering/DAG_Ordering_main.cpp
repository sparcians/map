#include <inttypes.h>
#include <iostream>
#include <string>

#include "sparta/sparta.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/log/Tap.hpp"

TEST_INIT;

using namespace sparta;
using sparta::DAG;

uint32_t call_sequence = 0;

class Updateable
{
public:
    Updateable(const std::string &name, sparta::TreeNode * es) :
        name_(name),
        my_event_(es, name + "updateable_event", CREATE_SPARTA_HANDLER(Updateable, myCallback))
    {
    }

    void myCallback()
    {
        EXPECT_TRUE(call_sequence < 5);
        ++call_sequence;
    }
    void go() {
        my_event_.schedule(1);
    }

private:
    const std::string name_;
    sparta::Event<sparta::SchedulingPhase::Update> my_event_;
};

class PortType
{
public:
    PortType(TreeNode * ps, const std::string & name) :
        in_port_(ps, "PortType" + name, sparta::SchedulingPhase::PortUpdate, 0)
    {
        in_port_.registerConsumerHandler(CREATE_SPARTA_HANDLER(PortType, myCallback));
    }

    void myCallback()
    {
        EXPECT_TRUE(call_sequence > 4);
        EXPECT_TRUE(call_sequence < 10);
        ++call_sequence;
    }

    sparta::SignalInPort & getPort() {
        return in_port_;
    }

private:
    std::string name_;
    sparta::SignalInPort in_port_;
};


class Collectable
{
public:
    Collectable(const std::string &name, sparta::TreeNode * es) :
        name_(name),
        my_event_(es, name + "collectable_event", CREATE_SPARTA_HANDLER(Collectable, myCallback))
    {
    }

    void myCallback() {
        EXPECT_TRUE(call_sequence > 9);
        EXPECT_TRUE(call_sequence < 15);
        ++call_sequence;
    }
    void go() {
        my_event_.schedule(1);
    }

private:
    const std::string name_;
    sparta::Event<sparta::SchedulingPhase::Collection> my_event_;
};

class Tickable
{
public:
    Tickable(const std::string &name, sparta::TreeNode * es) :
        name_(name),
        my_event_(es, name + "tickable_event", CREATE_SPARTA_HANDLER(Tickable, myCallback))
    {
    }

    void myCallback() {
        EXPECT_TRUE(call_sequence > 14);
        EXPECT_TRUE(call_sequence < 20);
        ++call_sequence;
    }
    void go() {
        my_event_.schedule(1);
    }

    void precedes(Tickable & tick) {
        my_event_ >> tick.my_event_;
    }

private:
    const std::string name_;
    sparta::Event<> my_event_;
};

template<class ObjT>
void makeEmGo(ObjT & obj_list) {
    for(auto & i : obj_list) {
        i->go();
    }
}

//____________________________________________________________
// MAIN
int main()
{

    sparta::Clock zclk("dummy");

    sparta::log::Tap sched_logger(sparta::TreeNode::getVirtualGlobalNode(),
                                sparta::log::categories::DEBUG, "sched.out");

    sparta::RootTreeNode rtn;
    rtn.setClock(&zclk);
    sparta::EventSet es(&rtn);

    std::unique_ptr<Updateable>  ups[5]   = { std::unique_ptr<Updateable>(new Updateable("up1", &es)),
                                              std::unique_ptr<Updateable>(new Updateable("up2", &es)),
                                              std::unique_ptr<Updateable>(new Updateable("up3", &es)),
                                              std::unique_ptr<Updateable>(new Updateable("up4", &es)),
                                              std::unique_ptr<Updateable>(new Updateable("up5", &es)) };
    std::unique_ptr<Collectable> cols[5]  = { std::unique_ptr<Collectable>(new Collectable("cols1", &es)),
                                              std::unique_ptr<Collectable>(new Collectable("cols2", &es)),
                                              std::unique_ptr<Collectable>(new Collectable("cols3", &es)),
                                              std::unique_ptr<Collectable>(new Collectable("cols4", &es)),
                                              std::unique_ptr<Collectable>(new Collectable("cols5", &es)) };
    std::unique_ptr<Tickable>    tickables[5] = { std::unique_ptr<Tickable>(new Tickable("tickables1", &es)),
                                                  std::unique_ptr<Tickable>(new Tickable("tickables2", &es)),
                                                  std::unique_ptr<Tickable>(new Tickable("tickables3", &es)),
                                                  std::unique_ptr<Tickable>(new Tickable("tickables4", &es)),
                                                  std::unique_ptr<Tickable>(new Tickable("tickables5", &es)) };

    sparta::PortSet ps(&rtn);
    std::unique_ptr<PortType> ports[5] = {
        std::unique_ptr<PortType>(new PortType(&ps, "1")),
        std::unique_ptr<PortType>(new PortType(&ps, "2")),
        std::unique_ptr<PortType>(new PortType(&ps, "3")),
        std::unique_ptr<PortType>(new PortType(&ps, "4")),
        std::unique_ptr<PortType>(new PortType(&ps, "5")),
    };

    sparta::SignalOutPort sop(&ps, "outport");
    for(auto & p : ports) {
        sparta::bind(p->getPort(), sop);
    }

    tickables[0].get()->precedes(*tickables[1]);
    tickables[0].get()->precedes(*tickables[2]);
    tickables[0].get()->precedes(*tickables[3]);
    tickables[0].get()->precedes(*tickables[4]);
    tickables[1].get()->precedes(*tickables[2]);
    tickables[2].get()->precedes(*tickables[3]);
    tickables[3].get()->precedes(*tickables[4]);

    rtn.enterConfiguring();
    rtn.enterFinalized();

    sparta::Scheduler::getScheduler()->finalize();

    sop.send(1);

    sparta::Scheduler::getScheduler()->getDAG()->print(std::cout);

    makeEmGo(ups);
    makeEmGo(cols);
    makeEmGo(tickables);

    sparta::Scheduler::getScheduler()->run(2);
    rtn.enterTeardown();

    EXPECT_EQUAL(call_sequence, 20);

    REPORT_ERROR;
    return ERROR_CODE;
}
