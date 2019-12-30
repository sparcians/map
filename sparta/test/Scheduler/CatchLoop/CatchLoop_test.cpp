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
#include <chrono>
#include "sparta/kernel/SleeperThread.hpp"
// Simply force an infinit loop. And assert that the scheduler catches this loop, then exit cleanly.

TEST_INIT;

class CycleValidator
{
public:
    CycleValidator(sparta::TreeNode * es) :
        inf_looper(es, "inf_looper", CREATE_SPARTA_HANDLER(CycleValidator, infLoop))
    {}

    void infLoop(){
        //infinite loop for half a minute. This will get caught be scheduler.
        boost::timer::cpu_timer t;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << "test infLoop() " << std::endl;
        std::cout << " wont exit for 60 seconds " << std::endl;
        while(t.elapsed().user < 120000000000)
        {
            // a see if sparta can survive my hacky code.
        }

    }
    sparta::Event<> inf_looper;
};

// catch the loop exit when the scheduler throws the exception, and still exit cleanly.
void signal_handler(int)
{
    std::cout << SPARTA_UNMANAGED_COLOR_GREEN << "Caught inf loop successfully" << SPARTA_UNMANAGED_COLOR_NORMAL << std::endl;
    // Report error.
    REPORT_ERROR;
    exit(ERROR_CODE);
}


int main()
{
    // install a signal handler to catch the infinite loop exception and exit cleanly.
    signal(SIGABRT, signal_handler);
    sparta::Clock clk("clock");
    sparta::EventSet es(nullptr);
    es.setClock(&clk);


    //Test port dependency
    CycleValidator cval(&es);
    sparta::SleeperThread::getInstance()->setInfLoopSleepInterval(std::chrono::seconds(5));
    sparta::SleeperThread::getInstance()->attachScheduler(sparta::Scheduler::getScheduler());
    sparta::SleeperThread::getInstance()->finalize();
    sparta::Scheduler::getScheduler()->finalize();
    cval.inf_looper.schedule(101);

    boost::timer::cpu_timer t;
    sparta::Scheduler::getScheduler()->printNextCycleEventTree(std::cout, 0, 0);
    sparta::Scheduler::getScheduler()->run(102);

    std::cout << SPARTA_CURRENT_COLOR_RED << "Shouldn't be here..." << std::endl;
    return 1;


}
