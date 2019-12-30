
#include <iostream>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "sparta/sparta.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/AsyncEvent.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

/*
 * Simple test with eight threads that each schedules a bunch of async events.
 * To keep the simulation going a "regular" event keeps rescheduling itself
 * every cycle until it is told to stop. The test tests that all the async
 * events are fired and that no event handlers are executed concurrently.
 */

class EventGen
{
public:
    EventGen(sparta::TreeNode *node,
             sparta::EventSet *event_set,
             sparta::SpartaHandler handler)
    : event_(event_set,
             "event",
             CREATE_SPARTA_HANDLER(EventGen, handler)),
      handler_(handler)
    {
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(EventGen, startUp_));
    }

    void stop()
    {
        done_ = true;
    }

private:
    void startUp_()
    {
        event_.schedule(1);
    }

    void handler()
    {
        handler_();
        if (!done_) {
            event_.schedule(1);
        }
    }

    bool done_ = false;

    sparta::Event<> event_;

    sparta::SpartaHandler handler_;
};

class AsyncEventGen
{
public:
    AsyncEventGen(sparta::TreeNode *node,
                  sparta::EventSet *event_set,
                  sparta::SpartaHandler handler,
                  unsigned int event_count,
                  int id)
    : async_event_(event_set,
                   "async_event" + std::to_string(id),
                   handler),
      event_count_(event_count)
    {
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(AsyncEventGen, startUp_));
    }

    ~AsyncEventGen()
    {
        thread_.join();
    }

private:
    void startUp_()
    {
        thread_ = std::thread(&AsyncEventGen::genAsyncEvents_, this);
    }

    void genAsyncEvents_()
    {
        while (event_count_-- > 0) {
            async_event_.schedule(sparta::Clock::Cycle(0));
            sleep_(1000);
        }
    }

    void sleep_(long usec)
    {
        struct timespec spec{0, 1000 * usec};
        nanosleep(&spec, nullptr);
    }

    std::thread thread_;

    sparta::AsyncEvent<> async_event_;

    unsigned int event_count_;
};

class TestDriver
{
public:
    TestDriver(sparta::TreeNode *node, sparta::EventSet *event_set)
    : event_gen_(node,
                 event_set,
                 CREATE_SPARTA_HANDLER(TestDriver, eventHandler_))
    {
        for (unsigned int i = 0; i < THREADS; i++) {
            async_event_gens_.emplace_back(
                node,
                event_set,
                CREATE_SPARTA_HANDLER(TestDriver,
                                    asyncEventHandler_),
                ASYNC_EVENTS_PER_THREAD, i);
        }
    }

private:
    void eventHandler_()
    {
        /* Test that no handlers are executed in parallel */
        EXPECT_TRUE(test_lock_.try_lock());
        doWork_(1000);
        test_lock_.unlock();
    }

    void asyncEventHandler_()
    {
        /* Test that no handlers are executed in parallel */
        EXPECT_TRUE(test_lock_.try_lock());
        ++async_event_count_;

        /* Test that we get the expected number asyc events fires */
        EXPECT_TRUE(async_event_count_ <= ASYNC_EVENTS_PER_THREAD * THREADS);

        if (async_event_count_ == ASYNC_EVENTS_PER_THREAD * THREADS) {
            event_gen_.stop();
        } else {
            doWork_(1000);
        }
        test_lock_.unlock();
    }


    double doWork_(unsigned int count)
    {
        volatile double x = 0;
        while (count-- > 0) {
            x = count * count / 12345;
        }
        return x;
    }

    static constexpr unsigned int THREADS = 8;
    static constexpr unsigned int ASYNC_EVENTS_PER_THREAD = 16;

    EventGen event_gen_;

    std::list<AsyncEventGen> async_event_gens_;

    std::mutex test_lock_;

    unsigned long async_event_count_ = 0;
};

constexpr unsigned int TestDriver::THREADS;
constexpr unsigned int TestDriver::ASYNC_EVENTS_PER_THREAD;

int main()
{
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->getCurrentTick() == 1);
    EXPECT_TRUE(sparta::Scheduler::getScheduler()->isRunning() == 0);

    sparta::Clock clk("clock");
    sparta::RootTreeNode rtn;
    sparta::EventSet event_set(&rtn);
    rtn.setClock(&clk);

    TestDriver test_driver(&rtn, &event_set);

    sparta::Scheduler::getScheduler()->finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    sparta::Scheduler::getScheduler()->run(-1U);

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
