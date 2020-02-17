// <SingleCycleUniqueEvent.h> -*- C++ -*-


/**
 * \file   SingleCycleUniqueEvent.hpp
 *
 * \brief  File that defines the SingleCycleUniqueEvent class
 */

#ifndef __SINGLE_CYCLE_UNIQUE_EVENT_H__
#define __SINGLE_CYCLE_UNIQUE_EVENT_H__

#include <set>
#include <memory>
#include "sparta/events/PhasedSingleCycleUniqueEvent.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    /**
     * \class SingleCycleUniqueEvent
     * \brief An event that can only be schedule one cycle into the future
     *
     * Analysis shows that modelers using UniqueEvent typically
     * schedule the event either the same cycle or exactly one cycle
     * into the future. UniqueEvent is a bit costly in performance due
     * to the fact that it needs to check with the Scheduler to see if
     * it's already scheduled at the given time.
     *
     * This type of Event is exactly like sparta::Event, but a
     * SingleCycleUniqueEvent will not schedule itself twice on the
     * scheduler for the same time and will _only_ schedule itself on
     * the subsequent cycle.
     *
     * Example usage: \code
     *
     * class MyClass : public sparta::Unit
     * {
     * private:
     *     sparta::SingleCycleUniqueEvent<> my_single_event_;
     *     sparta::Clock::Cycle last_time_called_ = 0;
     * };
     *
     * MyClass::MyClass(TreeNode * parent, ...) :
     *      my_event_set_(parent),
     *      my_single_event_(&Unit::unit_event_set_, "my_single_event",
     *                       CREATE_SPARTA_HANDLER(MyClass, myCallback))
     * {}
     *
     *
     * // ... called later
     * void MyClass::anotherCallback()
     * {
     *     called_already_ = false;
     *     my_single_event_.schedule();  // This schedules for 1 cycle in the future
     *     my_single_event_.schedule();  // ignored
     *     my_single_event_.schedule();  // ignored
     *     my_single_event_.schedule();  // ignored
     * }
     *
     * // Fired once
     * void MyClass::myCallback()
     * {
     *     sparta::Clock::Cycle curr_time = getClock()->currentCycle();
     *     sparta_assert(curr_time != last_time_called_,
     *                 "What?! Is SPARTA broken?  "
     *                 "I got called twice in the same cycle!");
     *     last_time_called_ = curr_time;
     * }
     * \endcode
     *
     */
    template<SchedulingPhase sched_phase_T = SchedulingPhase::Tick>
    class SingleCycleUniqueEvent : public PhasedSingleCycleUniqueEvent
    {
    public:
        // The phase this Event was defined with
        static constexpr SchedulingPhase event_phase = sched_phase_T;

        // Bring in PhasedSingleCycleUniqueEvent's schedule method.
        // This is the only one really allowed.
        using PhasedSingleCycleUniqueEvent::schedule;

        /**
         * \brief Create a SingleCycleUniqueEvent.
         * \param event_set The EventSet this SingleCycleUniqueEvent belongs to
         * \param name   The name of this event (as it shows in the EventSet)
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         *
         * Create a SingleCycleUniqueEvent that will only be scheduled only once
         * per clock phase, no matter how many times this SingleCycleUniqueEvent
         * is scheduled.
         */
        SingleCycleUniqueEvent(TreeNode * event_set,
                               const std::string & name,
                               const SpartaHandler & consumer_event_handler) :
            PhasedSingleCycleUniqueEvent(event_set, name, sched_phase_T,
                                         consumer_event_handler)
        { }

        //! Uniquely destroy
        virtual ~SingleCycleUniqueEvent() = default;

        //! Disallow moves
        SingleCycleUniqueEvent(SingleCycleUniqueEvent &&) = delete;

        //! Disallow the copying of the SingleCycleUniqueEvent
        SingleCycleUniqueEvent(const SingleCycleUniqueEvent &) = delete;

        //! Disallow the assignment of the SingleCycleUniqueEvent
        SingleCycleUniqueEvent& operator=(const SingleCycleUniqueEvent &) = delete;
    };

}


// __SINGLE_CYCLE_UNIQUE_EVENT_H__
#endif
