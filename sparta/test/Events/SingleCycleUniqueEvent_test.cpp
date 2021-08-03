// <SingleCycleUniqueEvent_test.cpp> -*- C++ -*-


#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/Precedence.hpp"

#include "sparta/sparta.hpp"

#include <iostream>
#include <cinttypes>

#include "sparta/events/EventSet.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/kernel/SleeperThread.hpp"

TEST_INIT;

class SCUEEventHandler
{
public:
    SCUEEventHandler(const sparta::Clock *clk) :
        test_handler_for_called_once(CREATE_SPARTA_HANDLER(SCUEEventHandler, testCalledOncePerCycle_)),
        test_handler_for_precedence_called_first(CREATE_SPARTA_HANDLER(SCUEEventHandler, testPrecedenceCalledFirst_)),
        test_handler_for_precedence_called_second(CREATE_SPARTA_HANDLER(SCUEEventHandler, testPrecedenceCalledSecond_)),
        do_nothing(CREATE_SPARTA_HANDLER(SCUEEventHandler, doNothing_)),
        do_nothing_data(CREATE_SPARTA_HANDLER_WITH_DATA(SCUEEventHandler, doNothingData_, int)),
        clk_(clk)
    {}

    sparta::SpartaHandler test_handler_for_called_once;

    sparta::SpartaHandler test_handler_for_precedence_called_first;
    sparta::SpartaHandler test_handler_for_precedence_called_second;
    sparta::SpartaHandler do_nothing;
    sparta::SpartaHandler do_nothing_data;

    uint32_t getCalledCount() const {
        return called_;
    }

    sparta::Clock::Cycle getLastTimeCalled() const {
        return last_time_called_;
    }

    void adjustTime(sparta::Clock::Cycle adjustment) {
        adjusted_time_ = adjustment;
    }

    void clearCalledBools() {
        first_one_called_ = false;
        second_one_called_ = false;
    }

private:

    bool first_one_called_ = false;
    bool second_one_called_ = false;

    void testPrecedenceCalledFirst_() {
        EXPECT_TRUE(second_one_called_ == false);
        EXPECT_TRUE(first_one_called_ == false);
        first_one_called_ = true;
    }
    void testPrecedenceCalledSecond_() {
        EXPECT_TRUE(first_one_called_ == true);
        EXPECT_TRUE(second_one_called_ == false);
        second_one_called_ = true;
    }

    // This should only be called on a new cycle, one cycle in the
    // future
    void testCalledOncePerCycle_(void) {
        EXPECT_TRUE((last_time_called_ + adjusted_time_) == clk_->currentCycle());
        ++called_;
        last_time_called_ = clk_->currentCycle();
        adjusted_time_ = 1;
    }
    uint32_t called_ = 0;

    void doNothing_() {}
    void doNothingData_(const int&) {}

    // time is 1-based
    sparta::Clock::Cycle last_time_called_ = 1;

    // In case the tester moves the scheduler WAY ahead for testing adjust
    sparta::Clock::Cycle adjusted_time_ = 1;
    const sparta::Clock * clk_;
};

// Test basic functionality:
//  - Instantiation
//  - Scheduling (only once per call and only one cycle in the future)
//
void testBasicFunctionality()
{
    sparta::Scheduler basic_scheduler("basic_scheduler");
    sparta::Clock clk("clk", &basic_scheduler);
    sparta::RootTreeNode rtn("test_root");
    sparta::EventSet event_set(&rtn);
    event_set.setClock(&clk);

    //new sparta::log::Tap(&basic_scheduler, "debug", std::cout);

    SCUEEventHandler handler(&clk);

    sparta::SingleCycleUniqueEvent<> sc_uniq_event(&event_set,
                                                 "sc_uniq_event",
                                                 handler.test_handler_for_called_once);

    basic_scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    // proceed to tick 1, nothing should happen, but time advancement
    basic_scheduler.run(1, true, false);

    basic_scheduler.run(1, true); // 1 -> 2

    EXPECT_EQUAL(handler.getCalledCount(), 0);

    sc_uniq_event.schedule(); // Schedules for cycle 2
    EXPECT_EQUAL(handler.getCalledCount(), 0);

    for(uint32_t i = 0; i < 100; ++i) {
        sc_uniq_event.schedule(); // Schedules for cycle 2, but doesn't
        EXPECT_EQUAL(handler.getCalledCount(), 0);
    }
    EXPECT_EQUAL(handler.getCalledCount(), 0);

    basic_scheduler.run(1, true); // 2 -> 3

    EXPECT_EQUAL(handler.getCalledCount(), 1);

    sc_uniq_event.schedule(0); // Schedules for cycle 3
    sc_uniq_event.schedule(0); // Schedules for cycle 3, but doesn't
    sc_uniq_event.schedule();  // Schedules for cycle 3, but doesn't

    basic_scheduler.run(1, true); // -> 4
    EXPECT_EQUAL(handler.getCalledCount(), 2);

    sc_uniq_event.schedule(); // Schedules for cycle 4
    basic_scheduler.run(1, true); // -> 5
    EXPECT_EQUAL(handler.getCalledCount(), 3);

    for(uint32_t i = 0; i < 100; ++i) {
        sc_uniq_event.schedule(); // Schedules for cycle 5, but doesn't
        EXPECT_EQUAL(handler.getCalledCount(), 3);
    }
    EXPECT_EQUAL(handler.getCalledCount(), 3);

    basic_scheduler.run(100, true); // 5 -> 105
    EXPECT_EQUAL(handler.getCalledCount(), 4);
    handler.adjustTime(100);

    sc_uniq_event.schedule(); // Schedules for cycle 101
    basic_scheduler.run(1, true); // 105 -> 106
    EXPECT_EQUAL(handler.getCalledCount(), 5);

    // Test cycle 1 scheduling
    basic_scheduler.run(1, true); // 106 -> 107
    EXPECT_EQUAL(handler.getCalledCount(), 5);

    sc_uniq_event.schedule(1); // Schedules for cycle 108
    sc_uniq_event.schedule(1);
    handler.adjustTime(3);

    basic_scheduler.run(1, true); // 107 -> 108
    EXPECT_EQUAL(handler.getCalledCount(), 5);
    basic_scheduler.run(1, true); // 108 -> 109
    EXPECT_EQUAL(handler.getCalledCount(), 6);
    EXPECT_EQUAL(handler.getLastTimeCalled(), 108);
    handler.adjustTime(1); // Tell the test handler we've moved ahead
                           // by two cycles without a call

    sc_uniq_event.schedule(0); // This should increment the call count to 7 on tick 109
    sc_uniq_event.schedule(0); // Do nothing
    sc_uniq_event.schedule(0); // Do nothing
    sc_uniq_event.schedule(0); // Do nothing
    sc_uniq_event.schedule(1); // This should increment the call count to 8 on tick 110
    sc_uniq_event.schedule(1); // Do nothing
    sc_uniq_event.schedule(1); // Do nothing
    sc_uniq_event.schedule(1); // Do nothing
    sc_uniq_event.schedule(0); // Do nothing
    sc_uniq_event.schedule(1); // Do nothing
    basic_scheduler.run(1, true); // 109 -> 110
    EXPECT_EQUAL(handler.getCalledCount(), 7);
    basic_scheduler.run(1, true); // 111 -> 112
    EXPECT_EQUAL(handler.getCalledCount(), 8);
    EXPECT_EQUAL(handler.getLastTimeCalled(), 110);
    handler.adjustTime(1); // Tell the test handler we've moved ahead
                           // by one cycle without a call

    sc_uniq_event.schedule(1); // This should increment the call count to 10 on tick 113
    sc_uniq_event.schedule(0); // This should increment the call count to 9  on tick 112
    basic_scheduler.run(1, true); // 111 -> 112
    EXPECT_EQUAL(handler.getCalledCount(), 9);

    sc_uniq_event.schedule(1); // This should increment the call count to 11 on tick 112
    EXPECT_EQUAL(basic_scheduler.getCurrentTick(), 112);  // Sanity check
    sc_uniq_event.schedule(0); // This should do nothing since we
                               // already have an event scheduled from
                               // the previous cycle
    sc_uniq_event.schedule(1); // This should do nothing
    sc_uniq_event.schedule(0); // This should do nothing
    sc_uniq_event.schedule(1); // This should do nothing
    sc_uniq_event.schedule(0); // This should do nothing
    sc_uniq_event.schedule(1); // This should do nothing
    sc_uniq_event.schedule(0); // This should do nothing
    sc_uniq_event.schedule(0); // This should do nothing
    sc_uniq_event.schedule(0); // This should do nothing
    basic_scheduler.run(1, true); // 112 -> 113
    EXPECT_EQUAL(handler.getCalledCount(), 10);
    EXPECT_EQUAL(handler.getLastTimeCalled(), 112);

    basic_scheduler.run(1, true); // 113 -> 114
    EXPECT_EQUAL(handler.getCalledCount(), 11);
    EXPECT_EQUAL(handler.getLastTimeCalled(), 113);


    EXPECT_TRUE(basic_scheduler.isFinished());

    rtn.enterTeardown();
}

void testPrecedence()
{
    sparta::Scheduler basic_scheduler("basic_scheduler");
    sparta::Clock clk("clk", &basic_scheduler);
    sparta::RootTreeNode rtn("test_root");
    sparta::EventSet event_set(&rtn);
    event_set.setClock(&clk);

    SCUEEventHandler scue_to_scue_handler(&clk);

    sparta::SingleCycleUniqueEvent<> sc_uniq_event_first(&event_set,
                                                       "sc_uniq_event_first",
                                                       scue_to_scue_handler.test_handler_for_precedence_called_first);
    sparta::SingleCycleUniqueEvent<> sc_uniq_event_second(&event_set,
                                                        "sc_uniq_event_second",
                                                        scue_to_scue_handler.test_handler_for_precedence_called_second);

    sparta::UniqueEvent<> uniq_event(&event_set, "uniq_event", scue_to_scue_handler.do_nothing);
    sparta::UniqueEvent<> uniq_event2(&event_set, "uniq_event2", scue_to_scue_handler.do_nothing);
    sparta::PayloadEvent<int> ple_event(&event_set, "ple_event", scue_to_scue_handler.do_nothing_data);
    sparta::PayloadEvent<int> ple_event2(&event_set, "ple_event2", scue_to_scue_handler.do_nothing_data);

    sc_uniq_event_first >> sc_uniq_event_second;

    uniq_event >> sc_uniq_event_first;
    sc_uniq_event_first >> uniq_event2;

    ple_event >> sc_uniq_event_first;
    sc_uniq_event_first >> ple_event2;

    basic_scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    // proceed to tick 1, nothing should happen, but time advancement
    basic_scheduler.run(1, true, false);

    sc_uniq_event_second.schedule();
    sc_uniq_event_first.schedule();
    basic_scheduler.run(2, true);

    EXPECT_TRUE(basic_scheduler.isFinished());
    rtn.enterTeardown();
}

// For this test, I want to see if the SCUE will outperform the
// standard UniqueEvent
//
// With some optimizations to the Scheduler, these are the times:
//
// SCUE
// real	0m11.378s
// user	0m11.364s
// sys	0m0.011s
//
// UE
// real	0m11.241s
// user	0m11.236s
// sys	0m0.005s
//
// It fairs a little better, but this is a really simple test.
//
#define SCUE_PERF_TEST 1
void testPerformance()
{
    sparta::Scheduler basic_scheduler("basic_scheduler");
    sparta::Clock clk("clk", &basic_scheduler);
    sparta::RootTreeNode rtn("test_root");
    sparta::EventSet event_set(&rtn);
    event_set.setClock(&clk);

    // Turn off the sleeper thread
    std::chrono::duration<double, std::ratio<3600> > duration(0);
    sparta::SleeperThread::getInstance()->setTimeout(duration, true, true);

    //new sparta::log::Tap(&basic_scheduler, "debug", std::cout);

    SCUEEventHandler handler(&clk);
#ifdef SCUE_PERF_TEST
    sparta::SingleCycleUniqueEvent<> uniq_event(&event_set,
                                              "uniq_event",
                                              handler.test_handler_for_called_once);
#else
    sparta::UniqueEvent<> uniq_event(&event_set, "uniq_event", handler.test_handler_for_called_once, 1);
#endif
    basic_scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    // proceed to tick 1, nothing should happen, but time advancement
    basic_scheduler.run(1, true, false);

    for(uint32_t i = 0; i < 100000000; ++i) {
        uniq_event.schedule();
        basic_scheduler.run(1, true, false);
        EXPECT_EQUAL(handler.getCalledCount(), i);
    }
    EXPECT_TRUE(basic_scheduler.isFinished());

    rtn.enterTeardown();
}

int main()
{
    testBasicFunctionality();
    testPrecedence();
    //testPerformance();

    REPORT_ERROR;
    return ERROR_CODE;
}
