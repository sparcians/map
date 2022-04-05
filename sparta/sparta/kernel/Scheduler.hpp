// <Scheduler> -*- C++ -*-

/**
 * \file Scheduler.hpp
 * \brief A simple time-based, event precedence based scheduler
 *
 */

#pragma once

#include <ctime>
#include <unistd.h>
#include <boost/timer/timer.hpp>
//#include <boost/algorithm/string/predicate.hpp>
#include <cstdint>
#include <string>
#include <cmath>
#include <vector>
#include <chrono>
#include <mutex>
#include <array>
#include <memory>
#include <algorithm>
#include <limits>
#include <list>
#include <ostream>

#include "sparta/utils/Colors.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/kernel/ObjectAllocator.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"

#ifdef SYSTEMC_SUPPORT
#include "sparta/log/NotificationSource.hpp"
#endif

namespace sparta {
class GlobalTreeNode;
}  // namespace sparta


//! Picoseconds per second constant
#define PS_PER_SECOND 1000000000000

namespace sparta {
    class Scheduler;
    class Clock;
    class DAG;

    class Scheduleable;
    class StartupEvent;
    // Forward declaration to support the addition of sparta::GlobalEvent
    template<typename DataT>
    class PhasedPayloadEvent;
    class EventSet;
    class GlobalEventProxy;
}

namespace sparta
{

/**
 * \class Scheduler
 * \brief A class that lets you schedule events now and in the future.
 *
 * The sparta::Scheduler class simply schedules callback methods for
 * some time in the future.  These callbacks are typically scheduled
 * by sparta::Scheduleable class and its derivatives, but the
 * sparta::scheduler is open to anyone who wishes to schedule a
 * callback (but this is highly discouraged).
 *
 * The callback type is sparta::SpartaHandler, a copyable method delegate
 * that allows a user to specify a function of their class as a
 * callback point.  The callback form is expected to be of the following:
 *
 * \code void func(); \endcode
 *
 * Time in the sparta::Scheduler can be interpreted anyway the user wishes,
 * but the base unit is a sparta::Scheduler::Tick.  For most
 * simulation uses, the Tick is considered a PS of time.  To convert a
 * Tick to a higher-order unit such as a clock cycle, use a
 * sparta::Clock made from a sparta::ClockManager to perform the
 * conversions.
 *
 * A typical flow for scheduling events is:
 *
 * -# Create an sparta::Scheduleable (or derivative sparta::Event,
 *    sparta::UniqueEvent, sparta::PayloadEvent) object with a given
 *    handler and a sparta::EventSet (that contains a
 *    sparta::Clock)
 *
 * -# Schedule the event using the event's schedule method, which will
 *    place the event on the sparta::Scheduler at the appropriate Tick using
 *    the sparta::Clock it got from the sparta::EventSet
 *
 * -# Wait for the sparta::Scheduler to get around to calling the method during
 *    the sparta::Scheduler::run call
 *
 * The sparta::Scheduler is a sparta::RootTreeNode so that it can be seen in a
 * global search scope, and loggers can be attached.  For example, try
 * this on the CoreExample:
 *
 * \code
 *   <build-dir>/example/CoreExample/sparta_core_example -r 1000 -l scheduler debug 1
 * \endcode
 *
 * Or do this in your C++ code:
 * \code
 *    new sparta::log::Tap(&my_scheduler_, "debug", std::cout);
 * \endcode
 *
 * \section event_ordering Event Ordering
 *
 * The sparta::Scheduler has a concept of "phased grouping" that
 * allows a user to specify which callback they want called before
 * another in time.  Each SPARTA event type has an associated
 * sparta::SchedulingPhase phase in its template parameter list that
 * the event will always be placed in.  In that phase, the event will
 * always come before a "higher priority phase" and always after a
 * "lower priority phase."  But, \a within its assigned phase, the
 * event will still be semi-random with respect to other events.  It's
 * "semi-random," meaning order will be indentical between simulation
 * runs, but possibly different once the simulator is modified at the
 * source-code level.  This can be annoying.
 *
 * Ordering within a phase is provided by a Direct Acyclic Graph or
 * sparta::DAG.  The DAG uses a class called sparta::Scheduleable that
 * represents a position within the DAG and an ordering group within a
 * SchedulingPhase.  By default the sparta::Scheduleable is not in a
 * group and is standalone within its assigned sparta::SchedulingPhase.
 * Once a precedence between two sparta::Scheduleable objects is
 * established, an ordering with assigned.  This results in each
 * sparta::Scheduleable being designated into an ordering group by the
 * DAG.  Event types (sparta::Event, sparta::UniqueEvent,
 * sparta::PayloadEvent) provide this support.  The developer can order
 * an event type to precede another event, but only if the events are
 * in the same SchedulingPhase:
 *
 * \code
 * sparta::EventSet my_ev_set;
 *
 * // These events are created in the SchedulingPhase::Tick by default.
 * sparta::Event<> my_go_first_event(&my_ev_set, "my_go_first_event",
 *                        CREATE_SPARTA_HANDLER(MyClass, myFirstMethod));
 * sparta::Event<> my_go_second_event(&my_ev_set, "my_go_second_event",
 *                        CREATE_SPARTA_HANDLER(MyClass, mySecondMethod));
 *
 * // Make myFirstMethod always get called before mySecondMethod.
 * // This can be done since both my_go events fall into the
 * // SchedulingPhase::Tick
 * my_go_first_event.precedes(my_go_second_event);
 *
 * // Likewise, you can do this to set up precedence:
 * // my_go_first_event >> my_go_second_event;
 *
 * sparta::Event<sparta::SchedulingPhase::Update> my_update_event(&my_ev_set_, "my_update_event",
 *                        CREATE_SPARTA_HANDLER(MyClass, myUpdateMethod));
 *
 * // COMPILER ERROR!  The update event is in a different phase than
 * // the my_go_first_event -- these events automatically happen in
 * // order.
 * my_update_event >> my_go_first_event;
 *
 * \endcode
 *
 * The sparta::Scheduler is responsible for finalizing the DAG. The DAG
 * is finalized when the sparta::Scheduler is finalized through the
 * sparta::Scheduler::finalize method called by the framework. Therefor,
 * all events can only be scheduled after the sparta::scheduler is
 * finalized. It is illegal to schedule events before the dag is
 * finalized because precedence has not been fully established.  Any
 * startup work can be scheduled via the sparta::StartupEvent class
 * before sparta::Scheduler finalization.
 *
 * The expected usage is something like:
 * \code
 * sched.finalize();
 * producer.scheduleMyStuff(); //a method that puts events on the scheduler.
 * sched.run(100); // will run up to at most 99 ticks
 * // sched.run(100, true); // will run to exactly 99 ticks
 * \endcode
 *
 */
class Scheduler : public RootTreeNode
{
public:

    //! Typedef for our unit of time.
    typedef uint64_t Tick;

private:

    /**
     * \struct TickQuantum
     * \brief The internal structure the Scheduler uses to maintain event lists
     *
     */
    struct TickQuantum
    {
        //! Typedef for a dynamic vector of events
        typedef std::vector<Scheduleable *> Scheduleables;

        //! Typedef for a static vector of event groups.
        typedef std::vector<Scheduleables>  Groups;

        /*!
         * \brief Construct a TickQuantum with the number of groups in
         * simulation
         * \param num_groups The number of firing groups (including pre/post
         * tick groups)
         */
        TickQuantum(uint32_t num_firing_groups) :
            tick(0),
            groups(num_firing_groups)
        { }

        /**
         * \brief Add an event to the timequantum
         * \param firing_group The Firing group (dag_group + 1) to add the
         *                     event. Must be > 0.
         * \param scheduleable The sparta::Scheduleable being scheduled
         */
        void addEvent(uint32_t firing_group, Scheduleable * scheduleable) {
            sparta_assert(firing_group > 0);
            sparta_assert(firing_group < groups.size());
            groups[firing_group].emplace_back(scheduleable);
            first_group_idx = std::min(first_group_idx, firing_group);
        }

        Tick tick;     //!< The tick this quantum represents
        Groups groups; //!< The list of firing groups. This is indexed by dag_group+1
        uint32_t first_group_idx = std::numeric_limits<uint32_t>::max(); //!< The first group idx with events
        TickQuantum * next = nullptr;
    };

    //! The current time quantum
    TickQuantum * current_tick_quantum_;

    //! The ObjectAllocator used to create time quantum structures
    ObjectAllocator<TickQuantum> tick_quantum_allocator_;

    //! return whether the watchdog has fired
    bool watchdogExpired_() const
    {
        bool watchdoc_expired = false;
        if (wdt_period_ticks_ > 0) {
            sparta_assert(current_tick_ >= prev_wdt_tick_);
            uint64_t num_ticks_since_wdt = current_tick_ - prev_wdt_tick_;
            if (num_ticks_since_wdt >= wdt_period_ticks_) {
                watchdoc_expired = true;
            }
        }
        return watchdoc_expired;
    }

public:

    //! Const expression to calculate tick value for indexing
    Tick calcIndexTime(const Tick rel_time) const {
        return current_tick_ + rel_time;
    }

    //! Return the number of nanoseconds the scheduler has been in run.
    template <typename DurationT>
    DurationT getRunCpuTime() const
    {
        return std::chrono::duration_cast<DurationT>(std::chrono::nanoseconds(timer_.elapsed().user));
    }

    //! Get the wall clock run time.
    template <typename DurationT>
    DurationT getRunWallTime() const
    {
        return std::chrono::duration_cast<DurationT>(std::chrono::nanoseconds(timer_.elapsed().wall));
    }

    /**
     * \brief Constant for infinite tick count
     */
    static const Tick INDEFINITE;

    /**
     * \brief Name of the Scheduler' TreeNode
     */
    static constexpr char NODE_NAME[] = "scheduler";

    //! Constructor
    Scheduler();

    /*!
     * \brief Constructor with name
     *
     * See other scheduler constructor
     */
    Scheduler(const std::string& name) :
        Scheduler(name, nullptr)
    {
        // Delegated Constructor
    }

    /*!
     * \brief Construct with a name and a specific global search scope (global
     * parent)
     * \param name Name of this scheduler node.
     * \param search_scope Scope in which this global scheduler node will exist
     * \warning Name must not be "scheduler". Name must not collide with any
     * other schedulers'
     */
    Scheduler(const std::string& name, GlobalTreeNode* search_scope);

    //! Dey-stroy
    ~Scheduler();

    //! Reset the scheduler.  Tears down all events, the DAG, and sets
    //! all outstanding TopoSortables to an invalid group ID
    void reset();

    //! Register a clock with this Scheduler
    //! \param clk Pointer to a sparta::Clock to be registered
    //!
    //! The purpose of registering the Clock is to allow the Scheduler
    //! to update the elapsed cycles in that Clock instead of having
    //! the Clock calculate that value over and over again.
    void registerClock(sparta::Clock *clk);

    //! Deregister a clock from this Scheduler
    //! \param clk Pointer to a sparta::Clock to be deregistered
    //!
    void deregisterClock(sparta::Clock *clk);

    //! \name Setup
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief Finalize the scheduler and allow running
     * \pre Must be called after all ports are created and associated with
     *      the DAG
     * \post Scheduler will be runnable
     * \note has no effect if already finalized
     * \see isFinalized
     */
    void finalize();

    //! \brief Get the internal DAG
    //! \return Pointer to the DAG
    DAG * getDAG() const{
        return dag_.get();
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Scheduler Management
    //! @{
    ////////////////////////////////////////////////////////////////////////


    /**
     * \brief Tell the scheduler to stop running.
     * \see isRunning
     * \note Will stop the scheduler immediately; even between events on the
     *       same tick.
     *
     * \todo Change the granularity of when the scheduler stops running so
     *       that it must finish the tick.
     *
     * If scheduler is not running or not finalized, has no effect.
     */
    void stopRunning() {
        running_ = false;
    }

    /**
     * \brief Clears all events in the scheduler without executing any of
     *        them
     * \note If scheduler is not finalized, has no effect.
     * \throw SpartaException if the scheduler is running. This method
     *        can not be called while running.
     * \pre Scheduler must be stopped
     * \pre Cannot be called from within a scheduler event callback
     * \pre Cannot not be called while simulator is running
     */
    void clearEvents();

    /*!
     * \brief Clears the events in the scheduler, sets the current tick to
     *        \a tick and the elapsed ticks to either \a tick or \a tick plus 1
     * \param t Tick to set scheduler at
     * \pre Scheduler must be finalized
     * \pre Scheduler must be stopped
     * \post The method getCurrentTick() will report \a t
     * \post The method getElapsedTicks() will report \a t or \a t + 1
     * \post All previously scheduled events will be removed
     *
     * A call to restartAt is asking the Scheduler to back up to the
     * given time as if it were either at the beginning or end of that
     * time.  Events scheduled with a zero rel_time will be scheduled
     * at time \a t.  Elapsed time still reflects \a t + 1 in the case
     * where \a t != 0, indicating how many ticks have elapsed.
     *
     * In the case where \a t == 0, elapsed time is also set to zero
     * as no time has elapsed.
     */
    void restartAt(Tick t);

    /**
     * \brief A method used for debugging the scheduler.
     *        Prints the scheduler's schedule of events.
     * \param os the stream to print the schedule to.
     * \param curr_grp   The group to start printing from (set to 0)
     * \param curr_event Within a group, the current event to start printing from
     * \param future By default equals 0 which will print the
     *        scheduled events for the next cycle.
     */
    template<class StreamType>
    void printNextCycleEventTree(StreamType & os, uint32_t curr_grp = 0,
                                 uint32_t curr_event = 0, uint32_t future=0) const;

    /**
     * \brief Schedule a single event. This method should also be thread
     *        safe.
     * \param scheduleable The sparta::Scheduleable to schedule
     * \param rel_time The relative time to schedule the event (i.e. 10
     *                 hyper-cycles from now)
     * \param dag_group the group id assigned by the Dag, default 0
     *
     * \param continuing If true, this event will cause the simulator
     * to continue running at least until it is fired (or something
     * explicitly stops the scheduler). If false, the simulator can
     * exhaust its list of continuing events and stop before this
     * event is reached. This usage is typically for infrastructure
     * events or counters, etc.
     */
    void scheduleEvent(Scheduleable * scheduleable,
                       Tick rel_time,
                       uint32_t dag_group=0,
                       bool continuing=true);

    /**
     * \brief Asynchronously schedule an event.
     * \param sched The sparta::Scheduleable to schedule
     * \param delay Delay relative to the time the event is actually scheduled
     *
     * This method is intended to be called by threads other than the main
     * scheduling thread (i.e., the thread that runs Scheduler::run()) that
     * wants to schedule events on the scheduler.
     *
     * This method does not necessarily schedule the event right away. The event
     * will be queued up and scheduled at the schedulers first convenience. The
     * delay parameter is relative to the time when the event is actually being
     * scheduled.
     */
    void scheduleAsyncEvent(Scheduleable *sched, Scheduler::Tick delay);

    /**
     * \brief Is the given Scheduleable item anywhere (in time now ->
     *        future) on the Scheduler?
     * \param scheduleable The Scheduleable item (used for pointer compare)
     * \return true if scheduled, false otherwise
     *
     * This function will look for the given Scheduleable item and
     * determine if it is on the scheduler at any time either now or
     * in the future.  The function does *not* do a full blown
     * Scheduleable class compare, but rather a pointer comparison.
     *
     * This function is expensive and should be used with caution.
     */
    bool isScheduled(const Scheduleable * scheduleable) const;

    /**
     * \brief Is the given Scheduleable item already scheduled?
     * \param scheduleable The Scheduleable item (used for pointer compare)
     * \param rel_time     The intended delivery time
     * \return true if scheduled, false otherwise
     *
     * This function will look for the given Scheduleable item and
     * determine if it is on the scheduler at the given time.  The
     * function does *not* do a full blown Scheduleable class compare,
     * but rather a pointer comparison.
     *
     * This function is rather inefficient for rel_times that are pretty large.
     */
    bool isScheduled(const Scheduleable * scheduleable, Tick rel_time) const;


    /**
     * \brief Cancel the given Scheduleable if on the Scheduler
     * \param scheduleable The Scheduleable to cancel (remove)
     *
     * Will search in all time quantums for the given Scheduleable
     * instance and cancel it everywhere found.
     */
    void cancelEvent(const Scheduleable * scheduleable);

    /**
     * \brief Cancel the given Scheduleable if on the Scheduler at the given time
     * \param scheduleable The Scheduleable to cancel (remove)
     * \param rel_time The time quantum to search in
     *
     * Will search only in the given time quantum for the given
     * Scheduleable and cancel it.
     */
    void cancelEvent(const Scheduleable * scheduleable, Tick rel_time);

    /**
     * \brief Cancel the given Scheduleable
     * \param scheduleable The Scheduleable to cancel (remove)
     *
     * This method should only be called from the main scheduler thread.
     */
    void cancelAsyncEvent(Scheduleable *scheduleable);

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Running
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief Enter running state and runs the scheduler until running
     *        is stopped (e.g. through a stop event) or the scheduler
     *        runs out of queued, continuing events.
     * \param num_ticks    The most number of ticks to advance.
     *                     This number is *not* inclusive (see notes)
     * \param exacting_run If num_ticks < Inf, should the Scheduler
     *                     run to exactly the number given? Default is no.
     * \param measure_run_time Capture running time (user and system).
     *                         Default is true.  This can be expensive
     *                         in back-to-back-to-back calls of run.
     *
     * \pre Scheduler must be finalized
     * \pre Must not already be running
     * \post current tick After running will be greater than the previous
     *       current tick and less-than or equal to the previous current tick +
     *       \a num_ticks
     * \post isRunning() will report false upon completion of this function
     * \post isFinished() will report true if there are more events to process
     * \see isFinished()
     * \see isFinalized
     * \see finalized
     * \see stopRunning
     * \warning If this method throws an exception, the scheduler
     *          should be stopped and reset in order to allow running
     *          once again (which is likely a bad idea).
     *
     * \code
     * scheduler.stopRunning();
     * scheduler.restartAt(1);
     * \endcode
     *
     * Runs the Scheduler for the given number of ticks.  Default
     * behavior is "run forever" where the Scheduler will only stop
     * if one of the following condition occur:
     *
     * #. A call to Scheduler::stopRunning()
     * #. There are no more non-continuing events on the Scheduler
     *
     * If a finite number of ticks are given to run, then the
     * Scheduler will stop when one of the following conditions is
     * met:
     *
     * #. The Scheduler reaches the end of the tick quantum determined
     *    by current time + num_ticks - 1, regardless of the setting of
     *    exacting_run
     *
     * #. If exacting_run == false and there are no more events or
     *    there are no more non-continuing events scheduled, the
     *    Scheduler will cease _before_ num_ticks have elapsed.  To
     *    change this behavior, set exacting_run to true.
     *
     * \note Calling run with a value for num_ticks less than (next
     *       tick quantum) - (current time) and exacting_run set to
     *       false will cause the Scheduler to *not* advance time and
     *       no events will ever to be fired.
     *
     * \note Setting exacting_run to true and providing a finite
     *       number of ticks to advance forces the Scheduler to run
     *       _up to_ that finite number and _no more_.  As an example,
     *       if the modeler calls sparta::Scheduler::run(10, true), the
     *       Scheduler will advance time (and run events if any) up to
     *       tick 9 and stop.  At this point, the Scheduler is ready
     *       to accept events for cycle 10
     *       (schedule(0)). sparta::Scheduler::getCurrentTick() will
     *       return 10 and sparta::Scheduler::getElapsedTicks() will
     *       return 9.
     *
     * \note The command line option '-r' sets exacting_run to false.
     *
     * \note The concept of finished w.r.t. run is if the Scheduler is
     *       not running, then it can either be finished or not
     *       finished.  This is dependent on whether there are
     *       continuing events scheduled in the future.  If the
     *       Scheduler is running, then it's not finished.
     */
    void run(Tick num_ticks=INDEFINITE,
             const bool exacting_run = false,
             const bool measure_run_time = true);

    /*!
     * \brief Returns true if there are no more pending non-continuing events
     * \return True if there are no pending non-continuing events
     */
    bool isFinished() const {
        return is_finished_;
    }

    /*!
     * \brief Returns the next tick an event is pending
     * \return The next "busy" tick; INDEFINITE if no event pending
     * \note This method should ONLY be called during an idle run
     *       (i.e. isRunning() == false).  Calling it during a run may
     *       result in incorrect tick count.
     */
    Tick nextEventTick() const {
        sparta_assert(!isRunning());
        if(current_tick_quantum_ == nullptr) {
            return INDEFINITE;
        }
        return (current_tick_quantum_->tick);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Scheduler State & Attributes
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief Is the scheduler finalized
     * \return true if finalzed, false otherwise
     * \see finalize
     */
    bool isFinalized() const noexcept override
    {
        return dag_finalized_;
    }

    /**
     * \brief Query if the scheduler is running
     * \return true if running; false otherwise
     *
     */
    bool isRunning() const noexcept
    {
        return running_;
    }

    /**
     * \brief The current tick the Scheduler is working on or just finished
     * \return The current tick
     * \note This can be called before finalization and works regardless of
     *       whether the scheduler is running
     */
    Tick getCurrentTick() const noexcept
    {
        return current_tick_;
    }

    /**
     * \brief The total elapsed ticks
     * \return The elapsed ticks
     *
     * This function returns the number of elapsed ticks since the
     * beginning of simulation.  This is different from the tick
     * returned from getCurrentTick() in that it will be typically one
     * tick ahead of the current tick at the end of a time quantum.
     *
     * For example, prior to a call to run(), current tick could be
     * 100, while the number of elapsed ticks is 101.  During the run,
     * both numbers will be the same, 101 until the Scheduler finishes
     * the current time quantum.  Once complete, the return from
     * getCurrentTick() will be 101 and elapsed tick will be 102.
     */
    Tick getElapsedTicks() const noexcept
    {
        return elapsed_ticks_;
    }

    /**
     * \return The number of picoseconds we've executed
     */
    Tick getSimulatedPicoSeconds() const noexcept
    {
        return current_tick_;
    }

    /**
     * \brief Reset the watchdog timer
     */
    void kickTheDog() noexcept {
        prev_wdt_tick_ = getCurrentTick();
    }

    /**
     * \brief Enable the watchdog timer.
     * \param watchdog_timeout_ps The timeout period for the watchdog timer
     *
     * If the watchdog is enabled, then the Scheduler will assert if
     * simulation ever runs for 'watchdog_timeout_ps' picoseconds without
     * someone calling kickTheDog().
     *
     * By default, the watchdog is disabled.
     */
    void enableWatchDog(uint64_t watchdog_timeout_ps) {
        // TODO: Handle the case when different callers want different
        // enable/disable behavior
        if (watchdog_timeout_ps == 0) {
            sparta_assert(wdt_period_ticks_ == 0);
        }

        // Only increase the period; don't allow it to decrease
        if (wdt_period_ticks_ < watchdog_timeout_ps) {
            // Scheduler ticks are currently pico-seconds, so this is 1:1
            wdt_period_ticks_ = watchdog_timeout_ps;
        }
    }

    /**
     * \return The number of events that the scheduled fired since it's creation.
     */
    Tick getNumFired() const noexcept
    {
        return events_fired_;
    }

    /**
     * \brief Returns the Tick quantum where the next continuing event resides
     *
     * \return The Tick where the next continuing event lies.  This
     *         can be either in the future or the current time.
     *
     * You cannot use this function to determine if there are
     * continuing events left in the Scheduler.  Use isFinished() to
     * determine that.
     */
    Tick getNextContinuingEventTime() const noexcept {
        return latest_continuing_event_;
    }

    /**
     * \return The current dag event firing.  The index is adjusted by
     *         one
     */
    const Scheduleable * getCurrentFiringEvent() const
    {
        if(current_tick_quantum_) {
            if(current_tick_quantum_->groups.size() > current_group_firing_) {
                if(current_tick_quantum_->groups[current_group_firing_].size() > current_event_firing_) {
                    return current_tick_quantum_->groups[current_group_firing_][current_event_firing_];
                }
            }
        }
        return nullptr;
    }

    //! \return The current firing event's index
    uint32_t getCurrentFiringEventIdx() const {
        return current_event_firing_;
    }

    //! \return The current SchedulingPhase.
    SchedulingPhase getCurrentSchedulingPhase() const {
        return current_scheduling_phase_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Instrumentation
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Returns the frequency (in ticks per simulated second) of this
     * Scheduler.
     * \note Tick elapsed can be roughly computed by:
     * getCurrentTick()/double(getFrequency())
     */
    Tick getFrequency() const {
        return PS_PER_SECOND; // 1tick = 1ps
    }

    /*!
     * \brief Returns a counter holding the current tick count of this
     * scheduler
     */
    ReadOnlyCounter& getCurrentTicksROCounter() {
        return ticks_roctr_;
    }

    /*!
     * \brief Returns a counter holding the current picosecond count of this
     * scheduler
     */
    ReadOnlyCounter& getCurrentPicosecondsROCounter() {
        return picoseconds_roctr_;
    }

    /*!
     * \brief Returns a StatisticDef holding the picosecond count of this
     * scheduler
     */
    StatisticDef& getSecondsStatisticDef() {
        return seconds_stat_;
    }

    /*!
     * \brief Returns a StatisticDef holding the millisecond count of this
     * scheduler
     */
    StatisticDef& getCurrentMillisecondsStatisticDef() {
        return milliseconds_stat_;
    }

    /*!
     * \brief Returns a StatisticDef holding the microsecond count of this
     * scheduler
     */
    StatisticDef& getCurrentMicrosecondsStatisticDef() {
        return microseconds_stat_;
    }

    /*!
     * \brief Returns a StatisticDef holding the nanosecond count of this
     * scheduler
     */
    StatisticDef& getCurrentNanosecondsStatisticDef() {
        return nanoseconds_stat_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

private:

    // The startup event adds itself to internal structures
    friend class StartupEvent;

    /**
     * \brief A temporary queue used for "cranking" the simulation
     * \param event_del The event delegate to call
     *
     * Not to be used during a running simulation, but during
     * construction of objects within simulation.  This queue is
     * used to "kick start" any work that must be done in "cycle
     * 0."  These events are invoked during Scheduler::finalize()
     * function call.
     */
    void scheduleStartupHandler_(const SpartaHandler & event_del) {
        startup_events_.emplace_back(event_del);
    }

    //! Moved to source to avoid circular header include issues with
    //! Scheduleable
    void throwPrecedenceIssue_(const Scheduleable * scheduleable, const uint32_t firing_group) const;
    const char * getScheduleableLabel_(const Scheduleable * sched) const;

    /*!
     * \brief Determines whick tick quantum a new tick will land in
     * \param rel_time Relative time
     * \return TickQuantum* that can be scheduled on for \a rel_time ticks in
     * the future
     */
    TickQuantum* determineTickQuantum_(Tick rel_time)
    {
        Tick index_time = (current_tick_ + rel_time);

        // This might look inefficient, but 99.9% of the time the
        // event being scheduled is either on the current time
        // quantum or the next.  A straight walk of two elements
        // is faster than a map lookup.
        TickQuantum * rit = current_tick_quantum_;
        TickQuantum * last_tq = nullptr;
        while(rit != nullptr) {
            if(rit->tick == index_time) {
                // This is the time quantum to add the event to
                break;
            }
            else if(rit->tick > index_time) {
                // We're past the tick quantum.  Insert before rit
                rit = tick_quantum_allocator_.create(firing_group_count_);
                rit->tick = index_time;

                // rit could have pointed to the
                // current_tick_quantum_.  If so, move the
                // current_tick_quantum_ back.
                if(SPARTA_EXPECT_FALSE(last_tq == nullptr)) {
                    rit->next = current_tick_quantum_;
                    current_tick_quantum_ = rit;
                }
                else {
                    rit->next = last_tq->next;
                    last_tq->next = rit;
                }
                break;
            }
            last_tq = rit;
            rit = rit->next;
        }
        // If we reached the end of our search in the time quantums,
        // we need to append this event to the end of the list
        //if(SPARTA_EXPECT_FALSE(rit == nullptr))
        if(rit == nullptr)
        {
            rit = tick_quantum_allocator_.create(firing_group_count_);
            rit->tick = index_time;
            if(SPARTA_EXPECT_TRUE(last_tq != nullptr)) {
                last_tq->next = rit;
            }
            else {
                // If the last_tq was null, there is a good change the
                // current_tick_quantum_ is null as well
                if(current_tick_quantum_ == nullptr) {
                    current_tick_quantum_ = rit;
                }
            }
        }

        return rit;
    }

    //! The DAG used for grouping
    std::unique_ptr<DAG> dag_;

    //! The number of groups in the DAG after finalization.
    uint32_t dag_group_count_;

    //! The number of firing groups (dag_group_count_ [+pre] [+post])
    uint32_t firing_group_count_;

    //! Identifier for group zero -- index of the array that represents it
    uint32_t group_zero_;

    //! A boolean for asserting whether or not the scheduler has
    //! called finalize on the dag
    bool dag_finalized_;

    //! Are we at the very first tick?
    bool first_tick_ = true;

    //! The current time of the scheduler
    Tick current_tick_;

    //! Elapsed ticks
    Tick elapsed_ticks_;

    //! Previous tick that someone kicked the WDT
    Tick prev_wdt_tick_;

    //! Tickout period in ticks for the WDT; a value of 0 indicates
    //! WDT is disabled
    Tick wdt_period_ticks_;

    //! Is the scheduler running. True = yes
    bool running_;

    //! A callback delegate to stop running the scheduler
    std::unique_ptr<Scheduleable> stop_event_;

    //! A callback delegate to replace cancelled events
    std::unique_ptr<Scheduleable> cancelled_event_;
    void cancelCallback_() {}

    //! A count of the number of events fired since this scheduler's
    //! creation
    Tick events_fired_;

    //! A count of the number of non-continuing events scheduled since
    //! this scheduler's creation
    bool is_finished_;

    //! A list of events that are zero priority to be fired
    std::vector<SpartaHandler> startup_events_;

    //! A vector of associated clocks with this scheduler.  Do not
    //! make this a std::set -- iteration is 120x slower
    std::vector<sparta::Clock*> registered_clocks_;

    //! The current dag group priority being fired.
    uint32_t current_group_firing_;

    //! The current event being fired.
    uint32_t current_event_firing_;

    //! The current SchedulingPhase
    SchedulingPhase current_scheduling_phase_ = SchedulingPhase::Trigger;

    //! Debug log message source
    log::MessageSource debug_;

    //! Scheduler call list
    log::MessageSource call_trace_logger_;

    //! Create the call trace stream once
    std::ostringstream call_trace_stream_;

    //! Furthest continuing event in the future. Used to determine when to stop the simulation
    Tick latest_continuing_event_;

    //! Set of counters & stats for the Scheduler
    StatisticSet sset_;

    //! A dummy clock for the stats in this Scheduler -- mostly here to carry this Scheduler object
    std::unique_ptr<sparta::Clock> scheduler_internal_clk_;

    //! Current tick count. References current_tick_ and assumes it is the same as counter_type_
    ReadOnlyCounter ticks_roctr_;
    //! Internal counter for the pico seconds...
    class PicoSecondCounter : public ReadOnlyCounter {
        Scheduler& sched_;
    public:
        PicoSecondCounter(Scheduler& sched,
                          sparta::Clock * clk,
                          StatisticSet* parent);

        counter_type get() const override {
            return static_cast<counter_type>(static_cast<double>(sched_.getElapsedTicks()) *
                                             (PS_PER_SECOND / static_cast<double>(sched_.getFrequency()))); // 150000.0
        }
        //! Number of picoseconds
    } picoseconds_roctr_;

    //! Computes number of seconds elapsed
    StatisticDef seconds_stat_;

    //! Computes number of milliseconds elapsed
    StatisticDef milliseconds_stat_;

    //! Computes number of microseconds elapsed
    StatisticDef microseconds_stat_;

    //! Computes number of nanoseconds elapsed
    StatisticDef nanoseconds_stat_;

    //! user runtime of the process, in seconds
    StatisticDef user_runtime_stat_;

    //! system runtime f the process, in seconds
    StatisticDef system_runtime_stat_;

    //! wall clock runtime f the process, in seconds
    StatisticDef wall_runtime_stat_;

    //! Timer used to calculate runtime
    boost::timer::cpu_timer timer_;

    //! User, System, and Wall clock counts
    uint64_t        user_time_ = 0;
    ReadOnlyCounter user_time_cnt_;

    uint64_t        system_time_ = 0;
    ReadOnlyCounter system_time_cnt_;

    uint64_t        wall_time_ = 0;
    ReadOnlyCounter wall_time_cnt_;

public:
    /**
     * \brief Get the raw pointer of "global" PhasedPayloadEvent inside sparta::Scheduler
     * \return The raw pointer of PhasedPayloadEvent<GlobalEventProxy>
     *
     * \note This function is added to support sparta::GlobalEvent
     */
    template<SchedulingPhase sched_phase_T = SchedulingPhase::Update>
    PhasedPayloadEvent<GlobalEventProxy>* getGlobalPhasedPayloadEventPtr() {
        static_assert(static_cast<uint32_t>(sched_phase_T) < NUM_SCHEDULING_PHASES,
                     "Invalid Scheduling Phase is provided!");

        return gbl_events_[static_cast<uint32_t>(sched_phase_T)].get();
    }

private:
    //! Unique pointer of EventSet
    std::unique_ptr<EventSet> es_uptr_;

    //! Array of unique pointers of PhasedPayloadEvent, each corresponds to a specific phase
    std::array<std::unique_ptr<PhasedPayloadEvent<GlobalEventProxy>>,
               NUM_SCHEDULING_PHASES> gbl_events_;

    //! Fire GlobalEvent callback function
    void fireGlobalEvent_(const GlobalEventProxy &);

    struct AsyncEventInfo {
        AsyncEventInfo(Scheduleable *sched, Scheduler::Tick tick)
            : sched(sched), tick(tick) { }

        AsyncEventInfo(Scheduleable *sched)
            : AsyncEventInfo(sched, 0) { }

        bool operator() (const AsyncEventInfo &info)
        {
            return info.sched == sched;
        }

        Scheduleable *sched = nullptr;
        Scheduler::Tick tick = 0;
    };

    //! Hint that there are events on async_event_list_ ready to be scheduled
    volatile bool async_event_list_empty_hint_ = true;

    //! Queue of asynchronous events that have not yet been scheduled
    std::list<AsyncEventInfo> async_event_list_;

    //! Lock protecting async_event_list_ and async_event_list_empty_hint_
    std::mutex async_event_list_mutex_;

    //! Broadcast a notification when something is scheuled.  This is
    //! only useful for the SysC adapter and not compiled in for
    //! regular Sparta users
#ifdef SYSTEMC_SUPPORT
    sparta::NotificationSource<Tick> item_scheduled_;
#endif
};


template<class StreamType>
inline void Scheduler::printNextCycleEventTree(StreamType& os,
                                               uint32_t curr_grp,
                                               uint32_t curr_event,
                                               uint32_t future) const
{
    if(current_tick_quantum_ == nullptr) {
        os << "sparta::Scheduler is empty" << std::endl;
        return;
    }

    uint32_t scheduler_map_idx = current_tick_ + future;
    os << "Scheduler's event tree for tick: " << scheduler_map_idx << std::endl;
    if(current_tick_quantum_->tick > scheduler_map_idx) {
        os << "\tNo events for time: '"
           << scheduler_map_idx << "' next event @"
           << current_tick_quantum_->tick << std::endl;
    }
    const TickQuantum::Groups & group_array = current_tick_quantum_->groups;
    for(uint32_t i = curr_grp; i < group_array.size(); ++i)
    {
        std::stringstream output;
        if((i + 1) == group_array.size()) {
            output << "\tGroup[zero]: ";
        }
        else {
            output << "\tGroup[" << i + 1 << "]: ";
        }
        const TickQuantum::Scheduleables & scheduleables = group_array[i];

        output << SPARTA_CURRENT_COLOR_GREEN;
        for(uint32_t x = 0; x < scheduleables.size(); ++x)
        {
            if(x) {
                output << ", ";
            }
            if((curr_grp == i) && (curr_event == x)) {
                output << SPARTA_CURRENT_COLOR_BRIGHT_GREEN << getScheduleableLabel_(scheduleables[x]);
            }
            else {
                output << SPARTA_CURRENT_COLOR_GREEN << getScheduleableLabel_(scheduleables[x]);
            }
        }

        os << output.str() << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
    }
}




}
