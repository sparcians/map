// <Scheduler.cpp> -*- C++ -*-


/**
 * \file Scheduler.cpp
 * \brief A simple time-based, event precedence based scheduler
 *
 */

#include "sparta/kernel/Scheduler.hpp"

#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>

#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/ObjectAllocator.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/kernel/SleeperThread.hpp"
// Header file included to support the addition of sparta::GlobalEvent
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PhasedPayloadEvent.hpp"
#include "sparta/events/GlobalEvent.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/SleeperThreadBase.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/log/categories/CategoryManager.hpp"

namespace sparta
{
class GlobalTreeNode;


Scheduler::Scheduler() :
    Scheduler(NODE_NAME)
{
    // Delegated construction
}

Scheduler::Scheduler(const std::string& name, GlobalTreeNode* search_scope) :
    RootTreeNode(name, "DES Scheduler", search_scope),
    current_tick_quantum_(nullptr),
    dag_group_count_(1),
    firing_group_count_(dag_group_count_ + 2),
    dag_finalized_(false),
    current_tick_(0), //init 0
    elapsed_ticks_(0),
    prev_wdt_tick_(0),
    wdt_period_ticks_(0),
    running_(false),
    stop_event_(new Scheduleable(CREATE_SPARTA_HANDLER(Scheduler, stopRunning), 0, SchedulingPhase::Trigger)),
    cancelled_event_(new Scheduleable(CREATE_SPARTA_HANDLER(Scheduler, cancelCallback_), 0, SchedulingPhase::Tick)),
    events_fired_(0),
    is_finished_(false),
    current_group_firing_(0),
    current_event_firing_(0),
    debug_(this,
           sparta::log::categories::DEBUG,
           "Scheduler debug messages including queue dump"),
    call_trace_logger_(this, (std::string)"calltrace",
                       "Scheduler Event Call Trace"),
    latest_continuing_event_(0),
    sset_(this),
    scheduler_internal_clk_(new sparta::Clock("_internal_scheduler_clk", this)),
    ticks_roctr_(&sset_,
                 "ticks",
                 "Current tick number",
                 Counter::COUNT_NORMAL,
                 &elapsed_ticks_),
    picoseconds_roctr_(*this, scheduler_internal_clk_.get(), &sset_),
    seconds_stat_(&sset_,
                  "seconds",
                  "Seconds elapsed",
                  &sset_,
                  "picoseconds/1000000000000.0"),
    milliseconds_stat_(&sset_,
                      "milliseconds",
                      "Milliseconds elapsed",
                      &sset_,
                      "picoseconds/1000000000.0"),
    microseconds_stat_(&sset_,
                       "microseconds",
                       "Microseconds elapsed",
                       &sset_,
                       "picoseconds/1000000.0"),
    nanoseconds_stat_(&sset_,
                      "nanoseconds",
                      "Nanoseconds elapsed",
                      &sset_,
                      "picoseconds/1000.0"),
    user_runtime_stat_(&sset_,
                       "user_runtime_seconds",
                       "Simulation user runtime in seconds as measured on the host machine",
                       Counter::COUNT_LATEST),
    system_runtime_stat_(&sset_,
                         "system_runtime_seconds",
                         "Simulation system runtime in seconds as measured on the host machine",
                         Counter::COUNT_LATEST),
    es_uptr_(new EventSet(this))
#ifdef SYSTEMC_SUPPORT
    , item_scheduled_(this, "item_scheduled", "Broadcasted when something is scheduled", "item_scheduled")
#endif
{
    // Statistics and StatisticInstance objects require a clock to
    // obtain the scheduler (which is this object) to determine
    // start/stop times for differencing.
    sset_.setClock(scheduler_internal_clk_.get());

    // TODO NOTE: Should we turn cycle-checking on by default (true)?
    // Cycle-checking tells the DAG to flag a cycle at the earliest opportunity
    // This means checking for a cycle as each edge is added to the DAG (DAG::link)
    // It's expensive, but can save time in debugging precedence problems
    dag_.reset(new DAG(this, false));

    // Added to support sparta::GlobalEvent, must follow dag_ initialization
    for (uint32_t phase = 0; phase < sparta::NUM_SCHEDULING_PHASES; phase++) {
        sparta::SchedulingPhase sched_phase = static_cast<sparta::SchedulingPhase>(phase);

        std::stringstream str;
        str << "gbl_event_" << sched_phase;

        gbl_events_[phase].reset(new sparta::PhasedPayloadEvent<GlobalEventProxy>
                                 (es_uptr_.get(), this, str.str(), sched_phase, CREATE_SPARTA_HANDLER_WITH_DATA
                                  (Scheduler, fireGlobalEvent_, GlobalEventProxy)));
    }

    timer_.stop();
}

Scheduler::PicoSecondCounter::PicoSecondCounter(Scheduler& sched,
                                                sparta::Clock * clk,
                                                StatisticSet* parent) :
    ReadOnlyCounter(parent,
                    "picoseconds",
                    "Picosecond Count of this Clock",
                    CounterBase::COUNT_NORMAL),
    sched_(sched)
{
    // For obtaining the scheduler
    this->setClock(clk);
}

Scheduler::~Scheduler()
{
    running_ = false;
    enterTeardown(); // On RootTreeNode
}

void Scheduler::reset()
{
    // This can happen during exception stack unwinding or even in
    // other cases, so set running to false even though, under normal
    // circumstances, this should not happen
    running_ = false;

    enterTeardown(); // On RootTreeNode
    clearEvents();

    dag_.reset(new DAG(this, false));
    dag_finalized_ = false;

    tick_quantum_allocator_.clear();
}

void Scheduler::registerClock(sparta::Clock *clk)
{
    auto it = std::find(registered_clocks_.begin(),
                        registered_clocks_.end(),
                        clk);
    if(it == registered_clocks_.end()) {
        registered_clocks_.emplace_back(clk);
    }
}

void Scheduler::deregisterClock(sparta::Clock *clk)
{
    auto it = std::find(registered_clocks_.begin(),
                        registered_clocks_.end(),
                        clk);
    if(it != registered_clocks_.end()) {
        registered_clocks_.erase(it);
    }
}

void Scheduler::finalize()
{
    if(!dag_finalized_)
    {
        // First thing we need to do is run cycle zero when
        // the scheduler is turned on, which will do things
        // such as finalize the DAG.  we only want to do this
        // if scheduler was originally off.  Ideally this
        // should be startRunning() and the user should use
        // stopRunning to turn off the scheduler in my
        // opinion.

        // Run some initialization operations before cycle
        // one.  Essentially this method is cycle 0 of
        // simulation.

        if(debug_) {
            debug_ << "Scheduler is firing interal cycle ZERO";
        }

        //cache the number of groups in the dag to be used when populating the
        //ScheduleMap with appropriately sized arrays.
        dag_group_count_ = dag_->finalize();
        sparta_assert(dag_group_count_ > 0); // internal error
        firing_group_count_ = dag_group_count_ + 2; // Includes pre and post-tick events

        // Group zero is assigned the last group in the
        // scheduler's array.  We increment the group count to
        // account for this and remember the group zero
        // position
        group_zero_ = firing_group_count_++;
        dag_finalized_ = true;
        current_group_firing_ = 0;

        restartAt(0);

        // The scheduler always starts on tick 1.
        current_tick_ = 0; //init 0
        prev_wdt_tick_ = 0;
    }
}

void Scheduler::clearEvents()
{
    sparta_assert(!running_, "Cannot clear events on the scheduler if it is running");

    if(SPARTA_EXPECT_FALSE(debug_)) {
        debug_ << "Clearing all events";
    }

    uint32_t last_event_idx = current_event_firing_;
    auto tq = current_tick_quantum_;
    while(tq != nullptr)
    {
        for(auto & events : tq->groups)
        {
            // Iterate each scheduled sparta event, and cancel's it.
            // There's no need to replace the event with a null
            // delegate since the list is to be completely emptied.
            for(uint32_t i = last_event_idx; i < events.size(); ++i)
            {
                events[i]->eventCancelled_();
            }
            events.clear();
            last_event_idx = 0;
        }

        auto temp_tq = tq;
        tq = tq->next;

        temp_tq->next = nullptr;
        tick_quantum_allocator_.free(temp_tq);
    }
    current_tick_quantum_ = nullptr;
    latest_continuing_event_ = 0;
    is_finished_ = true;
}

void Scheduler::restartAt(Tick t)
{
    if(!dag_finalized_){
        throw SpartaException("Cannot reset tick to ")
            << t << " (or any value) before the scheduler is finalized";
    }

    if(running_){
        throw SpartaException("Cannot set current tick to ")
            << t << " while the scheduler is running it is running";
    }

    clearEvents();
    current_tick_ = t;
    if(first_tick_ || (t == 0)) {
        elapsed_ticks_ = t;
    }
    else {
        elapsed_ticks_ = t + 1;
    }
}

void Scheduler::scheduleEvent(Scheduleable * scheduleable,
                              Tick rel_time,
                              uint32_t dag_group,
                              bool continuing)
{
    sparta_assert(dag_finalized_ == true,
                "Cannot schedule an event before the DAG has been finalized.  The Scheduleable: "
                << getScheduleableLabel_(scheduleable));

    if(SPARTA_EXPECT_FALSE(debug_)) {
        debug_ << SPARTA_CURRENT_COLOR_BRIGHT_CYAN
               << "scheduling: " << getScheduleableLabel_(scheduleable)
               << " at tick: " << calcIndexTime(rel_time)
               << " rel_time: " << (rel_time)
               << " group: " << dag_group
               << " continuing: " << std::boolalpha << continuing
               << SPARTA_CURRENT_COLOR_NORMAL;
    }

    // Put zero-grouped objects at the end of the group list to be
    // called last.
    const uint32_t firing_group =
        (dag_group != 0) ? dag_group + 1 : group_zero_;

    // Check to make sure rules have been followed.
    if(rel_time == 0)
    {
        if(SPARTA_EXPECT_FALSE(firing_group < current_group_firing_))
        {
            // no reason to guard this -- it's a failure
            debug_ << SPARTA_CURRENT_COLOR_BRIGHT_RED
                   << "--- PRECEDENCE ISSUE FOUND SCHEDULING EVENT: "
                   << getScheduleableLabel_(scheduleable)
                   << " THROWING EXCEPTION ---"
                   << SPARTA_CURRENT_COLOR_NORMAL;
            // Moved to source to avoid circular include issue
            // with Scheduleable
            throwPrecedenceIssue_(scheduleable, firing_group);
        }
    }

    // Zero based array for the group count
    // In this comparison, add two to dag group count for pre and post-tick groups
    sparta_assert(firing_group < firing_group_count_,
                "Trying to schedule to a firing group (" << firing_group << ") above what exists ("
                << firing_group_count_ << ")");

    auto rit = determineTickQuantum_(rel_time);

    rit->addEvent(firing_group, scheduleable);

    if(continuing){
        // We're not done.
        is_finished_ = false;

        // Track the farthest continuing event (not pre/post-tick) in the future
        latest_continuing_event_ = std::max(latest_continuing_event_, calcIndexTime(rel_time));
#ifdef SYSTEMC_SUPPORT
        if(SPARTA_EXPECT_FALSE(item_scheduled_.observed())) {
            item_scheduled_.postNotification(rel_time);
        }
#endif
    }
}


void Scheduler::scheduleAsyncEvent(Scheduleable *scheduleable,
                                   Scheduler::Tick rel_tick)
{
    std::unique_lock<std::mutex> lock(async_event_list_mutex_);
    async_event_list_.emplace_back(scheduleable, rel_tick);
    async_event_list_empty_hint_ = false;
}

void Scheduler::run(Tick num_ticks,
                    const bool exacting_run,
                    const bool measure_run_time)
{
    // NOTE: Do not return from this method without setting running_ to
    // false.
    sparta_assert(dag_finalized_ == true, "Cannot run the scheduler before the scheduler is finalized");
    sparta_assert(running_ == false, "Cannot run the scheduler because it is already running. "
                "This is either a recursive run() call or an even more severe problem");

    // This does happen sometimes, in the SysC environment.
    if(SPARTA_EXPECT_FALSE(num_ticks == 0)) {
        return;
    }

    // unpause infinite loop protection if we need
    SleeperThread::getInstance()->unpause();

    // Special case the first tick.  Current Tick is always 1-based
    // and trails elapsed ticks. Since we can't make current_tick_ -1,
    // we special case 0.  The reason we can't make it -1 is the fact
    // that a LOT folks call getCurrentTick() before simulation even
    // starts to set start times.
    if(SPARTA_EXPECT_FALSE(first_tick_))
    {
        first_tick_ = false;
        // Fire off startup events
        for(auto & event : startup_events_) {
            event();
        }
        startup_events_.clear();
    }

    // Flag running. Do not return from this method without setting
    // running_ = false;
    running_ = (current_tick_quantum_ != nullptr);
    if(SPARTA_EXPECT_TRUE(measure_run_time)) {
        // Start the timer
        timer_.resume();
    }

    // Schedule a stop event if a stop time was specified.
    if(num_ticks != INDEFINITE)
    {
        // Check to see if we need to run the Scheduler to the given
        // time at all.  The conditions checked are:
        //
        // 1. Is there anything in the Scheduler to run? If not
        //    advance time based on exacting_run and return.
        // 2. Will advancing the Scheduler to the given time surpass
        //    the current_tick_quantum_'s tick?  If not advance time
        //    based on exacting_run and return
        //
        // We subtract out 1 from the num_ticks since current_tick_
        // was incremented earlier.
        if(!running_ ||
           ((current_tick_ + num_ticks) < current_tick_quantum_->tick))
        {
            if(exacting_run) {
                // The user wants to get to exact time requested.  If
                // the user restarted simulation (restartAt), it's
                // possible the elapsed tick count > current_tick
                current_tick_  += num_ticks;

                // Elapsed ticks always trail current_tick_ by one
                elapsed_ticks_ += std::llabs(int64_t(current_tick_) - int64_t(elapsed_ticks_) - 1);
                for(auto clk : registered_clocks_) {
                    clk->updateElapsedCycles(elapsed_ticks_);
                }
            }
            running_ = false;
            if(SPARTA_EXPECT_TRUE(measure_run_time)) {
                timer_.stop();
            }
            return;
        }
        else {
            // Schedule a event for now and place it in port grouping
            // of zero.  Continuing is based on whether or not the
            // user wants to run exactly num_ticks or just wants an
            // upper limit on a run.
            scheduleEvent(stop_event_.get(), num_ticks - 1, 0, exacting_run);
        }
    }

    // We're finished if we're not running.  Set this boolean here in
    // the situation where we ARE running and an event queries (for
    // whatever reason) the Scheduler's finished state.
    is_finished_ = !running_;

    // Iterate over our map, and fire off events by then iterating
    // over the list at the current key position.  The loop will
    // continue to run until we run out of events, or someone sets
    // running_ to false.
    while(running_)
    {
        // Officially advance to the next tick before executing it
        TickQuantum * const quantum = current_tick_quantum_;
        current_tick_               = quantum->tick;

        // Update the number of elapsed_ticks with the difference
        // between current time and the previous elapsed time.  If the
        // user restarted simulation (restartAt), it's possible the
        // elapsed tick count > current_tick
        elapsed_ticks_             += std::llabs(int64_t(current_tick_) - int64_t(elapsed_ticks_));

        // Optimization -- start at the first group with events
        current_group_firing_       = quantum->first_group_idx;

        for(auto clk : registered_clocks_) {
            clk->updateElapsedCycles(elapsed_ticks_);
        }

        if(SPARTA_EXPECT_FALSE(debug_)) {
            debug_ << SPARTA_CURRENT_COLOR_GREEN
                   << "=== SCHEDULER: Next tick boundary " << current_tick_ << " ==="
                   << SPARTA_CURRENT_COLOR_NORMAL;
        }

        // Note on locking. The read of async_event_list_empty_hint_ below is
        // not protected by async_event_list_mutex_. From a correctness point of
        // view this is not a problem since we test if async_event_list_ is
        // empty with async_event_list_mutex_ acquired before scheduling events,
        // clearing async_event_list_ and setting async_event_list_empty_hint_.
        // This however might cause events that are on async_event_list_ to not
        // be scheduled until writes to async_event_list_empty_hint_ have
        // propagated to this thread, but since we do not give any guarantees of
        // when async event are scheduled this is not a problem.
        if (SPARTA_EXPECT_FALSE(!async_event_list_empty_hint_)) {
            std::unique_lock<std::mutex> lock(async_event_list_mutex_);
            if (!async_event_list_.empty()) {
                sparta_assert(!async_event_list_empty_hint_);

                for (auto &i : async_event_list_) {
                    scheduleEvent(i.sched, i.tick,
                                  i.sched->getGroupID(),
                                  i.sched->isContinuing());
                }
                async_event_list_.clear();
                async_event_list_empty_hint_ = true;
            }
        }

        const uint32_t grp_cnt = firing_group_count_;
        while(current_group_firing_ < grp_cnt)
        {
            TickQuantum::Scheduleables & events = quantum->groups[current_group_firing_];

            // The design of this for loop is important to keep as is.
            // The events array can grow in size after firing the
            // current event.
            //
            // XXX To do this for loop backwards, have
            // current_group_firing_ = size() - 1 and count down to 0.
            // Once at zero, reset it to size and have it count down
            // to the _previous_ size().  If neither have changed, the
            // loop will exit.
            for(current_event_firing_ = 0;
                current_event_firing_ < events.size();
                ++current_event_firing_)
            {
                const Scheduleable * sched = events[current_event_firing_];
                current_scheduling_phase_ = sched->getSchedulingPhase();
                if(SPARTA_EXPECT_FALSE(debug_)) {
                    printNextCycleEventTree(debug_, current_group_firing_, current_event_firing_);
                    debug_ << SPARTA_CURRENT_COLOR_BRIGHT_CYAN << "--> SCHEDULER: Firing " << sched->getLabel()
                           << " at time: " << current_tick_
                           << " group: " << current_group_firing_
                           << SPARTA_CURRENT_COLOR_NORMAL;
                }
                if(SPARTA_EXPECT_FALSE(call_trace_logger_)) {
                    call_trace_stream_ << sched->getLabel() << " ";
                }
                sched->getHandler()();
                ++events_fired_;
            }
            events.clear();
            ++current_group_firing_;
        }

        if(SPARTA_EXPECT_FALSE(call_trace_logger_)) {
            call_trace_logger_ << call_trace_stream_.str();
            call_trace_stream_.str("");
        }

        // Move to the next quantum
        current_tick_quantum_ = quantum->next;
        quantum->next         = nullptr;
        tick_quantum_allocator_.free(quantum);
        sparta_assert(watchdogExpired_() == false);

        // Update state
        is_finished_              = (current_tick_quantum_ == nullptr);
        // Two things: if we're finished (i.e. no more tick quantums)
        // or there are no more future continuing events, we're done!
        if(SPARTA_EXPECT_FALSE(is_finished_ ||
                             (latest_continuing_event_ < current_tick_quantum_->tick)))
        {
            is_finished_ = true;
            // Note: do future events need to be cleaned up here? Or
            // is this the same case as a stop event?
            if(debug_) {
                std::string next_tick = "none";
                if(current_tick_quantum_) {
                    next_tick = std::to_string(current_tick_quantum_->tick);
                }
                debug_ << SPARTA_CURRENT_COLOR_GREEN
                       << "=== SCHEDULER: No more continuing events queued. Halting at "
                       << current_tick_ << ". Latest continuing event was at "
                       << latest_continuing_event_ << ", next tick = " << next_tick << " ==="
                       << SPARTA_CURRENT_COLOR_NORMAL;
            }
            break;
        }
    }// End run loop.

    // Reset the current group
    current_group_firing_ = 0;

    // Reset the Scheduling phase
    current_scheduling_phase_ = SchedulingPhase::Trigger;

    // Update elapsed_ticks_
    elapsed_ticks_ += std::llabs(int64_t(current_tick_) - int64_t(elapsed_ticks_));

    // We've completed the previous tick, move to the next tick
    ++current_tick_;

    // pause infinite loop protection if we need
    SleeperThread::getInstance()->pause();

    running_ = false;
    if(SPARTA_EXPECT_TRUE(measure_run_time)) {
        timer_.stop();
        user_runtime_stat_ = (timer_.elapsed().user / 1E9);  // Convert from ns to seconds
        system_runtime_stat_ = (timer_.elapsed().system / 1E9);  // Convert from ns to seconds
    }
}

bool Scheduler::isScheduled(const Scheduleable * scheduleable, Tick rel_time) const
{
    uint32_t dag_group = scheduleable->getGroupID();
    const Tick index_time = calcIndexTime(rel_time);
    const TickQuantum * rit = current_tick_quantum_;
    while(rit != nullptr) {
        if(rit->tick == index_time)
        {
            if(SPARTA_EXPECT_FALSE(dag_group == 0)) {
                dag_group = group_zero_;
            } else {
                dag_group += 1;
            }

            // This is the time quantum requested
            for(const Scheduleable * scheduled : rit->groups[dag_group])
            {
                if(scheduled == scheduleable) {
                    return true;
                }
            }
            return false;
        }
        else if(rit->tick > index_time) {
            // We're past the tick quantum -- didn't find it
            return false;
        }
        rit = rit->next;
    }
    return false;
}

bool Scheduler::isScheduled(const Scheduleable * scheduleable) const
{
    uint32_t dag_group = scheduleable->getGroupID();
    if(SPARTA_EXPECT_FALSE(dag_group == 0)) {
        dag_group = group_zero_;
    } else {
        dag_group += 1;
    }

    const TickQuantum * rit = current_tick_quantum_;
    while(rit != nullptr)
    {
        for(const Scheduleable * scheduled : rit->groups[dag_group])
        {
            if(scheduled == scheduleable) {
                return true;
            }
        }
        rit = rit->next;
    }
    return false;
}

void Scheduler::cancelEvent(const Scheduleable * scheduleable)
{
    uint32_t dag_group = scheduleable->getGroupID();
    if(SPARTA_EXPECT_FALSE(dag_group == 0)) {
        dag_group = group_zero_;
    } else {
        dag_group += 1;
    }

    TickQuantum * rit = current_tick_quantum_;
    while(rit != nullptr)
    {
        TickQuantum::Scheduleables & scheduleables = rit->groups[dag_group];
        for(uint32_t i = 0; i < scheduleables.size(); ++i)
        {
            if(scheduleables[i] == scheduleable) {
                scheduleables[i] = cancelled_event_.get();
                if(SPARTA_EXPECT_FALSE(debug_)) {
                    debug_ << SPARTA_CURRENT_COLOR_BRIGHT_YELLOW
                           << "canceling: " << scheduleable->getLabel()
                           << " at tick: " << rit->tick
                           << " group: " << dag_group
                           << SPARTA_CURRENT_COLOR_NORMAL;
                }
            }
        }
        rit = rit->next;
    }
}

void Scheduler::cancelEvent(const Scheduleable * scheduleable, Tick rel_time)
{
    uint32_t dag_group = scheduleable->getGroupID();
    if(SPARTA_EXPECT_FALSE(dag_group == 0)) {
        dag_group = group_zero_;
    } else {
        dag_group += 1;
    }
    const Tick index_time = calcIndexTime(rel_time);

    TickQuantum * rit = current_tick_quantum_;
    while(rit != nullptr)
    {
        if(rit->tick == index_time)
        {
            TickQuantum::Scheduleables & scheduleables = rit->groups[dag_group];
            for(uint32_t i = 0; i < scheduleables.size(); ++i)
            {
                if(scheduleables[i] == scheduleable) {
                    scheduleables[i]->eventCancelled_();
                    scheduleables[i] = cancelled_event_.get();
                    if(SPARTA_EXPECT_FALSE(debug_)) {
                        debug_ << SPARTA_CURRENT_COLOR_BRIGHT_YELLOW
                               << "canceling: " << scheduleable->getLabel()
                               << " at tick: " << rit->tick
                               << " reltime: " << rel_time
                               << " group: " << dag_group
                               << SPARTA_CURRENT_COLOR_NORMAL;
                    }
                }
            }
            return;
        }
        rit = rit->next;
    }
}

void Scheduler::cancelAsyncEvent(Scheduleable *scheduleable)
{
    std::unique_lock<std::mutex> lock(async_event_list_mutex_);

    /* Remove the Scheduleable from async_event_list_ */
    async_event_list_.remove_if(AsyncEventInfo(scheduleable));
    async_event_list_empty_hint_ = async_event_list_.empty();

    /* In case the event has already been scheduled, cancel it. */
    cancelEvent(scheduleable);
}

void Scheduler::throwPrecedenceIssue_(const Scheduleable * scheduleable, const uint32_t firing_group) const
{
    std::stringstream st;
    st << "\n\tCannot schedule an event \n\n\t'" << scheduleable->getLabel() << "' pgroup (" << firing_group
       << ") ""\n\n\twhich is a lower priority grouping than the currently firing event \n\n\t'"
       << current_tick_quantum_->groups[current_group_firing_][current_event_firing_]->getLabel()
       << "' in pgroup (";
    if(current_group_firing_ == group_zero_) {
        st << "zero";
    }
    else {
        st << current_group_firing_;
    }
    st << ")\n\n    Possible problems/solutions:"
       << "\n\t - Is '"
       << current_tick_quantum_->groups[current_group_firing_][current_event_firing_]->getLabel()
       << "' a producer to a zero-cycle Port?  If so, register the event associated with\n"
       << "\t   '" << current_tick_quantum_->groups[current_group_firing_][current_event_firing_]->getLabel()
       << "' as a producer on the port using 'registerProducingEvent(producer)'\n";
    st << "\t - Are you missing a precedence between these two events if they are in the same block?\n";
    st << "\t - If across blocks, can you consider using a zero-cycle SignalPort to set up a precedence or a GOP?\n";

    throw SpartaException(st.str());
}

const char * Scheduler::getScheduleableLabel_(const Scheduleable * sched) const
{
    return sched->getLabel();
}

/**
  * \brief Callback function of sparta::GlobalEvent
  * \param gep GlobalEventProxy instance which contains scheduling info of a specific event
  *
  * \note This function is added to support sparta::GlobalEvent
  */
void Scheduler::fireGlobalEvent_(const GlobalEventProxy & gep) {
    sparta_assert(current_scheduling_phase_ == gep.getSchedulingPhase(),
                "Global event scheduling phase is not consistent with current phase");
    gep();
}
// NOTE:
// (1)The function argument can only be pointer or reference to GlobalEventProxy,
//    also the definition of this function can only be put in the source file.
//    Otherwise, the compiler will complain about 'no match' candidate for 'gep()', or
//    'incomplete type def' of GlobalEventProxy, because there is only forward declaration
//    but no definition for this class in Scheduler.h
// (2)The "const" qualifier before GlobalEventProxy requires that the function-call operator of
//    GlobalEventProxy class has to be "const" also. Otherwise, the compiler will complain:
//    'passing 'const ...' as this argument of '...'discards qualifiers [-fpermissive]

}  // namespace sparta
