

// This test hammers on the sparta::Scheduler.
// It checks for:
// - Missed DAG precedence issues
// - Order of Scheduler startup
// - Speed of the Scheduler
// - Startup scheduling
// - Reset mechanisms
// - Out of range events
// - Start/stop behavior
// - Clearing of events during run
// - restart behavior
//

#include "sparta/sparta.hpp"
#include <iostream>
#include <inttypes.h>
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include <boost/timer/timer.hpp>
#include "sparta/kernel/SleeperThread.hpp"

TEST_INIT;

sparta::SchedulingPhase global_phase = sparta::SchedulingPhase::Trigger;

template<sparta::SchedulingPhase phase>
class TestEvent : public sparta::Scheduleable
{
public:
    TestEvent(sparta::TreeNode * rtn) :
        Scheduleable(CREATE_SPARTA_HANDLER(TestEvent, testEventCB), 0, phase)
    {
        std::stringstream str;
        str << getHandler().getName();
        str << "[" << phase << "]";
        my_name = str.str();
        setLabel(my_name.c_str());

        sparta::Scheduleable::local_clk_ = rtn->getClock();
        sparta::Scheduleable::scheduler_ = rtn->getClock()->getScheduler();
    }

    void testEventCB()
    {
        EXPECT_TRUE(scheduler_->isRunning());

        sparta_assert(global_phase == phase);
        time_called = scheduler_->getCurrentTick();

        global_phase = static_cast<sparta::SchedulingPhase>(static_cast<uint32_t>(global_phase) + 1);

        /// Yeah, don't do this in real code...
        if(global_phase == sparta::SchedulingPhase::Invalid) {
            global_phase = static_cast<sparta::SchedulingPhase>(0); // Trigger for now...
        }
        //sparta_assert(!"This should not have been called");
    }

    sparta::Scheduler::Tick time_called = sparta::Scheduler::INDEFINITE;
    std::string my_name;
};

// A bad dag event -- in the phase Tick, but will try to schedule
// events in the previous phases.
class BadDagEvent : public sparta::Scheduleable
{
public:
    BadDagEvent(sparta::TreeNode * rtn) :
        Scheduleable(CREATE_SPARTA_HANDLER(BadDagEvent, testBadSchedule), 0,
                     sparta::SchedulingPhase::Tick),
        ev_trigger(rtn),
        ev_update(rtn),
        ev_portupdate(rtn),
        ev_collect(rtn)
    {
        sparta::Scheduleable::local_clk_ = rtn->getClock();
        sparta::Scheduleable::scheduler_ = rtn->getClock()->getScheduler();
    }

    void testBadSchedule()
    {
        if(!self_sched) {
           EXPECT_TRUE(scheduler_->isRunning());
           EXPECT_THROW(scheduler_->scheduleEvent(&ev_trigger, 0, ev_trigger.getGroupID()));
           EXPECT_THROW(scheduler_->scheduleEvent(&ev_update, 0, ev_update.getGroupID()));
           EXPECT_THROW(scheduler_->scheduleEvent(&ev_portupdate, 0, ev_portupdate.getGroupID()));
           EXPECT_THROW(scheduler_->scheduleEvent(&ev_collect, 0, ev_collect.getGroupID()));
           EXPECT_NOTHROW(scheduler_->scheduleEvent(this, 0, getGroupID()));
           self_sched = true;
        }
    }

    bool self_sched = false;

    TestEvent<sparta::SchedulingPhase::Trigger>    ev_trigger;
    TestEvent<sparta::SchedulingPhase::Update>     ev_update;
    TestEvent<sparta::SchedulingPhase::PortUpdate> ev_portupdate;
    TestEvent<sparta::SchedulingPhase::Collection> ev_collect;

};

static_assert(sparta::NUM_SCHEDULING_PHASES == 7,
              "\n\nIf you got this compile-time assert, then you need to update this test 'cause you added more phases to SchedulingPhase. \n"
              "Specifically, you need to add more TestEvent's below\n\n");

int main()
{
    sparta::Scheduler lsched;
    sparta::Clock clk("clock", &lsched);
    sparta::RootTreeNode rtn("dummyrtn");
    rtn.setClock(&clk);

    sparta::Scheduler * sched = rtn.getClock()->getScheduler();

    EXPECT_TRUE(sched->getCurrentTick()  == 0); //sched not finalized, tick init to 0
    EXPECT_TRUE(sched->getElapsedTicks() == 0);
    EXPECT_TRUE(sched->isRunning() == false);

    // Get info messages from the scheduler node and send them to this file
    sparta::log::Tap t2(sched, "debug", "scheduler.log.debug");
    sparta::log::Tap t3(sched, "calltrace", "scheduler.log.calltrace");

    TestEvent<sparta::SchedulingPhase::Trigger> ev_trigger(&rtn);
    TestEvent<sparta::SchedulingPhase::Update> ev_update(&rtn);
    TestEvent<sparta::SchedulingPhase::PortUpdate> ev_portupdate(&rtn);
    TestEvent<sparta::SchedulingPhase::Collection> ev_collect(&rtn);
    TestEvent<sparta::SchedulingPhase::Tick> ev_tick(&rtn);
    TestEvent<sparta::SchedulingPhase::PostTick> ev_posttick(&rtn);
    TestEvent<sparta::SchedulingPhase::Flush> ev_flush(&rtn);

    BadDagEvent ev_baddag(&rtn);

    // Order test -- should not be allowed to schedule an event before
    // finalization
    EXPECT_THROW(sched->scheduleEvent(&ev_trigger, 0, ev_trigger.getGroupID()));

    sched->finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    sched->run(1, true, false);

    EXPECT_NOTHROW(sched->scheduleEvent(&ev_trigger, 0, ev_trigger.getGroupID()));

    // Tick is now 1-based
    EXPECT_TRUE(sched->getCurrentTick() == 1);
    EXPECT_TRUE(sched->getElapsedTicks() == 0);

    // This should fire the testEventCB() in the ev_trigger event
    sched->run(1, true, false);

    // "Current tick" has been changed to reflect that the Scheduler
    // has moved on and is now on the "next" tick.  Elapsed time
    // should reflect current tick - 1
    EXPECT_TRUE(sched->getCurrentTick() == 2);
    EXPECT_TRUE(sched->getElapsedTicks() == 1);
    EXPECT_TRUE(sched->isRunning() == false);
    EXPECT_TRUE(sched->isFinished() == true);
    EXPECT_EQUAL(sched->nextEventTick(), sparta::Scheduler::INDEFINITE);
    EXPECT_EQUAL(ev_trigger.time_called, 1);

    // Test a bad DAG precedence check -- schedule an event that when
    // fired will try to schedule an event in a previous phase in the
    // same cycle.  The catch is in the BadDagEvent class
    sched->scheduleEvent(&ev_baddag, 0, ev_baddag.getGroupID());
    sched->run(1, true, false);
    EXPECT_TRUE(sched->getCurrentTick() == 3);
    EXPECT_TRUE(sched->isRunning() == false);
    EXPECT_EQUAL(sched->nextEventTick(), sparta::Scheduler::INDEFINITE);

    // Test the scheduling of items for next cycle (in this test,
    // cycle 3), starting with Tick and go OOO from there.  Make sure
    // the events are still called within order.
    sched->scheduleEvent(&ev_tick,       0, ev_tick.getGroupID());
    sched->scheduleEvent(&ev_update,     0, ev_update.getGroupID());
    sched->scheduleEvent(&ev_portupdate, 0, ev_portupdate.getGroupID());
    sched->scheduleEvent(&ev_collect,    0, ev_collect.getGroupID());
    sched->scheduleEvent(&ev_flush,      0, ev_flush.getGroupID());
    sched->scheduleEvent(&ev_posttick,   0, ev_posttick.getGroupID());
    sched->scheduleEvent(&ev_trigger,    0, ev_trigger.getGroupID());

    // Start the phase in trigger
    global_phase = sparta::SchedulingPhase::Trigger;

    // no throws!
    sched->run(1, true, false);
    EXPECT_TRUE(sched->getCurrentTick() == 4);
    EXPECT_TRUE(sched->getElapsedTicks() == 3);

    // Should have come back around...
    EXPECT_TRUE(global_phase == sparta::SchedulingPhase::Trigger);

    // Schedule a bunch of events in the future, then clear the events
    for(uint32_t i = 0; i < 1000; ++i) {
        sched->scheduleEvent(&ev_tick, i % 10 + 1, ev_tick.getGroupID());
    }
    EXPECT_EQUAL(sched->nextEventTick(), (0 % 10 + 1) + sched->getCurrentTick());

    // This should clear out the events just scheduled
    sched->clearEvents();
    EXPECT_TRUE(sched->isFinished() == true);
    EXPECT_TRUE(sched->isRunning() == false);

    EXPECT_EQUAL(sched->nextEventTick(), sparta::Scheduler::INDEFINITE);

    sparta::Scheduler::Tick elapsed   = sched->getElapsedTicks();
    sparta::Scheduler::Tick curr_tick = sched->getCurrentTick();

    // nothing should happen
    sched->run(1, true, false);
    EXPECT_TRUE(sched->getCurrentTick() == 5);

    // Elapsed time should continue to advance even through the
    // scheduler was cleared.
    EXPECT_EQUAL(elapsed + 1, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick + 1, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick, elapsed + 1);

    ////////////////////////////////////////////////////////////////////////////////
    // Test restart functionality
    // For this test, make sure time is 3 ticks, elapsed being 4
    EXPECT_EQUAL(sched->getCurrentTick(), 5);
    EXPECT_EQUAL(sched->getElapsedTicks(), 4);

    // Restart the phase in trigger
    global_phase = sparta::SchedulingPhase::Trigger;

    // Schedule a bunch of events now and the future...
    for(uint32_t i = 0; i < 1000; ++i) {
        sched->scheduleEvent(&ev_tick, i % 10 + 1, ev_tick.getGroupID());
    }
    EXPECT_EQUAL(sched->nextEventTick(), (0 % 10 + 1) + sched->getCurrentTick());
    EXPECT_TRUE(sched->isFinished() == false);

    // Restart the Scheduler @tick == 2
    sched->restartAt(2);
    EXPECT_TRUE(sched->isFinished() == true);
    EXPECT_TRUE(sched->isRunning() == false);
    EXPECT_EQUAL(sched->nextEventTick(), sparta::Scheduler::INDEFINITE);

    // After a restartAt, the scheduler is in a "confusing state."
    // Basically, a user is asking the Scheduler to go back to a
    // specific time and repeat it -- as if the Scheduler were going
    // back into the middle of a run, which means elapsed ticks can be
    // more than the current tick value.  This is only true if the
    // user restarts the Scheduler at a tick != 0
    elapsed   = 3;
    curr_tick = 2;

    // Elapsed time should continue to advance even through the
    // scheduler was cleared.
    EXPECT_EQUAL(elapsed, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick + 1, elapsed);

    // nothing should happen, but time advancement
    sched->run(1, true, false);

    elapsed   = 4;
    curr_tick = 3;

    EXPECT_EQUAL(elapsed, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick + 1, elapsed);

    // Try running stuff again
    ev_trigger.time_called = 0;

    EXPECT_TRUE(global_phase == sparta::SchedulingPhase::Trigger);

    // Test the scheduling of items for next cycle, starting with Tick
    // and go OOO from there.  Make sure the events are still called
    // within order.
    sched->scheduleEvent(&ev_tick,       0, ev_tick.getGroupID());
    sched->scheduleEvent(&ev_update,     0, ev_update.getGroupID());
    sched->scheduleEvent(&ev_portupdate, 0, ev_portupdate.getGroupID());
    sched->scheduleEvent(&ev_collect,    0, ev_collect.getGroupID());
    sched->scheduleEvent(&ev_flush,      0, ev_flush.getGroupID());
    sched->scheduleEvent(&ev_posttick,   0, ev_posttick.getGroupID());
    sched->scheduleEvent(&ev_trigger,    0, ev_trigger.getGroupID());

    sched->run(1, true, false);
    EXPECT_TRUE(sched->getCurrentTick() == 4);

    // The event was called at least 3 times by the time this test
    // fired.  The scheduler is on the 4 tick, so we subtract 1 to
    // align it with the number of times the event was called
    EXPECT_EQUAL(ev_trigger.time_called, sched->getCurrentTick() - 1);

    // Finally, restart at time == 0
    sched->restartAt(0);

    // After a restartAt, the scheduler is in a "confusing state."
    // Basically, a user is asking the Scheduler to go back to a
    // specific time and repeat it -- as if the Scheduler were going
    // back into the middle of a run, which means elapsed and current
    // tick are the same.
    elapsed   = 0;
    curr_tick = 0;

    // Elapsed time should continue to advance even through the
    // scheduler was cleared.
    EXPECT_EQUAL(elapsed, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick, elapsed);
    EXPECT_EQUAL(sched->nextEventTick(), sparta::Scheduler::INDEFINITE);

    // nothing should happen, but time advancement
    sched->run(1, true, false);

    elapsed   = 0; // one tick has been executed
    curr_tick = 1; // 0 -> 1
    EXPECT_EQUAL(elapsed, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick, elapsed + 1);

    // Restart the phase in trigger
    global_phase = sparta::SchedulingPhase::Trigger;

    // Test the scheduling of items for next cycle, starting with Tick
    // and go OOO from there.  Make sure the events are still called
    // within order.
    sched->scheduleEvent(&ev_tick,       0, ev_tick.getGroupID());
    sched->scheduleEvent(&ev_update,     0, ev_update.getGroupID());
    sched->scheduleEvent(&ev_portupdate, 0, ev_portupdate.getGroupID());
    sched->scheduleEvent(&ev_collect,    0, ev_collect.getGroupID());
    sched->scheduleEvent(&ev_flush,      0, ev_flush.getGroupID());
    sched->scheduleEvent(&ev_posttick,   0, ev_posttick.getGroupID());
    sched->scheduleEvent(&ev_trigger,    0, ev_trigger.getGroupID());

    sched->run(1, true, false);
    EXPECT_TRUE(global_phase == sparta::SchedulingPhase::Trigger);
    // After the run, the scheduler's current tick is one past when
    // the trigger event was called
    EXPECT_EQUAL(ev_trigger.time_called, sched->getCurrentTick() - 1);

    elapsed   = 1;
    curr_tick = 2;
    EXPECT_EQUAL(elapsed, sched->getElapsedTicks());
    EXPECT_EQUAL(curr_tick, sched->getCurrentTick());
    EXPECT_EQUAL(curr_tick, elapsed + 1);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr()->getSchedulingPhase(), sparta::SchedulingPhase::Update);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Trigger>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Trigger>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::Trigger);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::PortUpdate>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::PortUpdate>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::PortUpdate);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Flush>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Flush>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::Flush);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Collection>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Collection>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::Collection);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Tick>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::Tick>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::Tick);

    EXPECT_NOTEQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::PostTick>(), nullptr);
    EXPECT_EQUAL(sched->getGlobalPhasedPayloadEventPtr<sparta::SchedulingPhase::PostTick>()->getSchedulingPhase(),
                 sparta::SchedulingPhase::PostTick);

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
