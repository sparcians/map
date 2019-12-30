#include "sparta/sparta.hpp"

#include <signal.h>
#include <iostream>
#include <inttypes.h>
#include "sparta/ports/DataPort.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/events/EventSet.hpp"
#include <boost/timer/timer.hpp>
#include "sparta/kernel/SleeperThread.hpp"

TEST_INIT;

class CycleValidator
{
public:
    CycleValidator(sparta::TreeNode * es) :
        ev_test_cycle(es, "ev_test_cycle", CREATE_SPARTA_HANDLER(CycleValidator, testScheduler)),
        inf_looper(es, "inf_looper", CREATE_SPARTA_HANDLER(CycleValidator, infLoop))
    {}

    const uint32_t expected_time = 10;

    void testScheduler() {
        EXPECT_EQUAL(sparta::Scheduler::getScheduler()->getCurrentTick(), expected_time);
        // Since we are in the middle of a Scheduler "epoch" the
        // elapsed time should equal the current time
        EXPECT_EQUAL(sparta::Scheduler::getScheduler()->getCurrentTick(),
                     sparta::Scheduler::getScheduler()->getElapsedTicks());
    }



    void infLoop(){
        // this test has no loop. So it should just exit.
        boost::timer::cpu_timer t;
        while(t.elapsed().user < 3000000000){
            // hang out. :)
        }
    }

    sparta::Event<> ev_test_cycle;
    sparta::Event<> inf_looper;
};

void signal_handler(int)
{
    std::cout << "no exceptions should of been thrown. There is no loop" << std::endl;
    EXPECT_TRUE(false);
}

int main()
{
    std::cout << " this test should exit pretty fast, couple of seconds " << std::endl;
    signal(SIGABRT, signal_handler);
    sparta::Clock clk("clock");
    sparta::EventSet es(nullptr);
    es.setClock(&clk);

    EXPECT_TRUE(sparta::Scheduler::getScheduler()->getCurrentTick()  == 1);
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->getElapsedTicks() == 0);
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->isRunning() == false);

    // Test scheduler logging (general test of logging on global TreeNodes)
    // First, find the scheduler node
    std::vector<sparta::TreeNode*> roots;
    sparta::TreeNode::getVirtualGlobalNode()->findChildren(sparta::Scheduler::NODE_NAME, roots);
    EXPECT_EQUAL(roots.size(), 1);
    EXPECT_NOTHROW( EXPECT_EQUAL(sparta::TreeNode::getVirtualGlobalNode()->getChild(sparta::Scheduler::NODE_NAME), roots.at(0)) );

    //Test port dependency
    CycleValidator cval(&es);
    sparta::SleeperThread::getInstance()->attachScheduler(sparta::Scheduler::getScheduler());
    sparta::SleeperThread::getInstance()->finalize();
    sparta::Scheduler::getScheduler()->finalize();

    // To be scheduled at the expected time, you need to take into
    // account the current time
    cval.ev_test_cycle.schedule(cval.expected_time -
                                sparta::Scheduler::getScheduler()->getCurrentTick());


    cval.inf_looper.schedule(101);

    boost::timer::cpu_timer t;
    sparta::Scheduler::getScheduler()->printNextCycleEventTree(std::cout, 0, 0);
    sparta::Scheduler::getScheduler()->run(102);
    // we should of exited in less than a second
    std::cout << t.elapsed().user << std::endl;
    // we hopefully didn't add to much time reaping the thread.
    EXPECT_TRUE(t.elapsed().user < 60000000000);

    REPORT_ERROR;
    return ERROR_CODE; // if we got here, we failed. this should be impossible since the inf loop.


}
