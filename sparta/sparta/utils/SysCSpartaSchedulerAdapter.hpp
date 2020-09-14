// <SysCSpartaSchedulerAdapter> -*- C++ -*-


/**
 * \file SysCSpartaSchedulerAdapter.hpp
 *
 * \brief Glue code that connect the Sparta scheduler to SystemC
 *
 */

#pragma once

#include <climits>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"

// Ignore -Wconversion issues with SysC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
  #include "sysc/kernel/sc_event.h"
  #include "sysc/kernel/sc_time.h"
  #include "sysc/kernel/sc_module.h"
  #include "sysc/communication/sc_clock.h"
#pragma GCC diagnostic pop

namespace sparta
{

//! \def SC_Sparta_SCHEDULER_NAME
//! The name of the scheduler adapter
#define SC_SPARTA_SCHEDULER_NAME  "SysCSpartaSchedulerAdapter"

//! \def SC_Sparta_STOP_EVENT_NAME
//! The name of the SystemC event used to stop simulation
#define SC_SPARTA_STOP_EVENT_NAME "sc_ev_stop_simulation"

/*!
 * \class SysCSpartaSchedulerAdapter
 * \brief Class that "connects" Sparta to SystemC
 *
 * This class will allow a Sparta developer to interoperate a
 * Sparta-based simulator with the SystemC kernel.  The general rule
 * of thumb is that the Sparta scheduler is either always equal to or
 * 1 cycle ahead of the SystemC scheduler.  The Sparta Scheduler will
 * "sleep" waiting for SysC to catch up the next scheduled Sparta
 * event.
 *
 * There are two ways to stop simulation using this adapter:
 *
 * -# In SystemC, find the event SC_SPARTA_STOP_EVENT_NAME and notify it
 *    when SystemC is complete
 * -# Register a sparta::Event via registerSysCFinishQueryEvent that is
 *    called by Sparta to query the SystemC side
 *
 * There are some caveats to know about this adapter.  See the todo.
 *
 * \todo The Sparta scheduler is on its own SC_THREAD and put to sleep
 * between scheduled events.  For example, if the Sparta scheduler has
 * an event scheduled @ tick 1000, and time is currently 500, the Sparta
 * scheduler thread will wait() for 500 ticks. However, if a SystemC
 * component puts an event on the Sparta scheduler during this sleep
 * window (say at 750 ticks), we do not have a mechanism to wake this
 * thread early.
 */
class SysCSpartaSchedulerAdapter : public sc_core::sc_module
{

    // Called by the Scheduler when a new event is scheduled
    void wakeupAdapter_(const Scheduler::Tick&) {
        sc_wake_sparta_.notify();
        sparta_scheduler_->
            deregisterForNotification<Scheduler::Tick,
                                      SysCSpartaSchedulerAdapter,
                                      &SysCSpartaSchedulerAdapter::wakeupAdapter_>(this, "item_scheduled");
    }

public:

    //! Register the process for SystemC
    SC_HAS_PROCESS(SysCSpartaSchedulerAdapter);

    //! Initialized the sc_module this adapter is part of
    SysCSpartaSchedulerAdapter(Scheduler * scheduler) :
        sc_module(sc_core::sc_module_name(SC_SPARTA_SCHEDULER_NAME)),
        sparta_scheduler_(scheduler),
        sc_ev_stop_simulation_(SC_SPARTA_STOP_EVENT_NAME),
        sc_wake_sparta_("sc_ev_wake_sparta")
    {
        SC_THREAD(runScheduler_);
        SC_METHOD(setSystemCSimulationDone);
        dont_initialize();
        sensitive << sc_ev_stop_simulation_;

        switch(PS_PER_SECOND/sparta_scheduler_->getFrequency())
        {
        case 1:
            sparta_sc_time_ = sc_core::SC_PS;
            break;
        case 10:
            sparta_sc_time_ = sc_core::SC_NS;
            break;
        case 100:
            sparta_sc_time_ = sc_core::SC_US;
            break;
        default:
            throw sparta::SpartaException("Frequency not supported");
            break;
        }
    }

    /*!
     * \brief Run simulation -- all of it including SystemC
     * \param num_ticks The number of ticks to run simulation (both
     *                  SysC and Sparta)
     *
     * Run simulation using the Sparta command line infrastructure and
     * world.  This method is typically called from derivatives of
     * sparta::Simulator via the runRaw_() overridden method.
     */
    void run(Scheduler::Tick num_ticks = Scheduler::INDEFINITE)
    {
        double convert_to_sysc_time = std::numeric_limits<double>::max();
        if(num_ticks != Scheduler::INDEFINITE) {
            convert_to_sysc_time = num_ticks;
        }
        sc_core::sc_start(sc_core::sc_time(convert_to_sysc_time, sparta_sc_time_));
    }

    /**
     * \brief Set simulation complete on the SystemC side via the
     *        SC_Sparta_STOP_EVENT_NAME sc_event.  Can be called
     *        directly if need be.
     */
    void setSystemCSimulationDone() {
        // Only report this once.
        if(!sysc_simulation_done_) {
            std::cout << "SysCSpartaSchedulerAdapter: SystemC reports finished on tick "
                      << sc_core::sc_time_stamp().value() << std::endl;
            sysc_simulation_done_ = true;
        }
    }

    /*!
     * \brief Register a sparta::Event that is used to determine if the
     *        SystemC components are finished
     * \param sysc_query_event The sparta::Event to schedule at the given interval
     * \param interval The time to schedule the query event
     *
     * Since there are two schedulers running with this adapter, there
     * are interesting scenarios that must be acknowledged.  One
     * scenario is that the SysC clock is finished (no events), but
     * the Sparta Scheduler is still busy (and injecting events) into
     * the SystemC side.  Likewise, Sparta could be idle, but SystemC
     * still running.  The best way to handle this (as most SystemC
     * users have continuous events and force simulation stop using
     * sc_stop), is to have a Sparta Event that queries the SystemC side
     * to see if it's truly complete.  If so, then we drain Sparta and
     * this adapter terminiates simulation.
     */
    void registerSysCFinishQueryEvent(sparta::Scheduleable * sysc_query_event,
                                      const Scheduler::Tick interval)
    {
        sysc_query_event_ = sysc_query_event;
        sysc_query_event_interval_ = interval;
        next_sysc_event_fire_tick_ = sparta_scheduler_->getCurrentTick() + interval;
        sparta_assert(sysc_query_event_->isContinuing() == false,
                          "This event should be non-continuing");
    }

    /*!
     * \brief Return whether the schedule called sc_stop()
     */
    bool wasScStopCalled() const {
        return sc_stop_called_;
    }

private:

    //! SC_THREAD that runs the Sparta scheduler
    void runScheduler_()
    {
        // Start simulation -- align the schedulers
        sparta_assert(sparta_scheduler_->nextEventTick() > 0);

        // Align the schedulers.  Sparta starts at tick 1
        wait(sc_core::sc_time(double(1), sparta_sc_time_));

        do {
            // If the Sparta Scheduler has nothing to do, put it to sleep
            if(sparta_scheduler_->nextEventTick() == Scheduler::INDEFINITE) {
                sparta_scheduler_->registerForNotification<Scheduler::Tick,
                                                           SysCSpartaSchedulerAdapter,
                                                           &SysCSpartaSchedulerAdapter::wakeupAdapter_>(this, "item_scheduled");
                wait(sc_wake_sparta_);
            }

            //
            // Wait for SysC to get to the next event time on the
            // Sparta Scheduler, then advance Sparta
            //
            const sc_core::sc_time sysc_time = sc_core::sc_time_stamp();
            if(sparta_scheduler_->nextEventTick() >= sysc_time.value()) {
                // Wait until the sysc scheduled catches up with Sparta
                wait(sc_core::sc_time(double(sparta_scheduler_->nextEventTick() - sysc_time.value()),
                                      sparta_sc_time_));
            }

            // Align to the posedge events in systemc
            wait(sc_core::SC_ZERO_TIME);

            // If given, schedule the user's SystemC query event to
            // allow the Sparta user to check to see if the SystemC side
            // of simulation is complete.  This query can be as simple
            // as asking the SystemC kernel if there are any events
            // (sc_core::sc_pending_activity()) or asking the SystemC
            // simulator if it's done.
            if(sysc_query_event_ && sparta_scheduler_->getCurrentTick() >= next_sysc_event_fire_tick_)
            {
                next_sysc_event_fire_tick_ = sparta_scheduler_->getCurrentTick() + sysc_query_event_interval_;
                sysc_query_event_->scheduleRelativeTick(1, sparta_scheduler_);
            }

            advanceSpartaScheduler_();

        } while(!sparta_scheduler_->isFinished() || !sysc_simulation_done_);

        // Stop simulation
        sc_core::sc_stop();
        sc_stop_called_ = true;
    }

    void advanceSpartaScheduler_()
    {
        const sc_core::sc_time sysc_time = sc_core::sc_time_stamp();

        // The SystemC scheduler will always be exactly at the same
        // tick as Sparta when this function is called.  Following
        // this rule allows safe assumptions in scheduling
        // synchronization.
        sparta_assert(sysc_time.value() == sparta_scheduler_->nextEventTick());
        sparta_assert(sparta_scheduler_->nextEventTick() >= sparta_scheduler_->getCurrentTick());

        const bool exacting_run = true;
        const bool measure_scheduler_time = false; // no need to do this
        // Run to the next event scheduled and then run that cycle
        sparta_scheduler_->run(sparta_scheduler_->nextEventTick() - sparta_scheduler_->getCurrentTick() + 1,
                               exacting_run, measure_scheduler_time);

        // Sparta should now be finished with the cycle at `sysc_time`
        // and be exactly one cycle ahead of it.
        sparta_assert(sysc_time.value() + 1 == sparta_scheduler_->getCurrentTick());
    }

    // Local copy of the sparta scheduler
    sparta::Scheduler * sparta_scheduler_ = nullptr;

    // Time unit Sparta runs in
    sc_core::sc_time_unit sparta_sc_time_ = sc_core::SC_PS;

    // Boolean that tells our runScheduler_ thread to exit -- the rest
    // of the SystemC scheduling is complete
    bool sysc_simulation_done_ = false;

    // SystemC event that stops simulation
    sc_core::sc_event sc_ev_stop_simulation_;
    bool sc_stop_called_ = false;

    // SystemC event that wakes the Sparta Scheduler
    sc_core::sc_event sc_wake_sparta_;

    // Sparta Event (optional) that will be automatically scheduled if
    // the Sparta scheduler is finished and the driver needs to query systemc
    sparta::Scheduleable * sysc_query_event_ = nullptr;
    Scheduler::Tick sysc_query_event_interval_ = 10000;
    Scheduler::Tick next_sysc_event_fire_tick_ = 0;
};

}
