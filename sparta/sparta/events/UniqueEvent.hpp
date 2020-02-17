// <UniqueEvent.h> -*- C++ -*-


/**
 * \file   UniqueEvent.hpp
 *
 * \brief  File that defines the UniqueEvent class
 */

#ifndef __UNIQUE_EVENT_H__
#define __UNIQUE_EVENT_H__

#include <set>
#include <memory>
#include "sparta/events/PhasedUniqueEvent.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    /**
     * \class UniqueEvent
     *
     * \brief A type of Event that uniquely schedules itself on the
     *        schedule within a single time quantum.  This class is
     *        typed on the SchedulingPhase and is the preferred type
     *        to use for UniqueEvent use, but the main API is found in
     *        PhasedUniqueEvent.
     *
     * This type of Event is exactly like sparta::Event, but a
     * UniqueEvent will not schedule itself twice on the scheduler for
     * the same time.  Example usage:
     * \code
     *
     * class MyClass : public sparta::Unit
     * {
     * private:
     *     sparta::UniqueEvent<> my_unique_event_;
     *     sparta::Clock::Cycle last_time_called_ = 0;
     * };
     *
     * MyClass::MyClass(TreeNode * parent, ...) :
     *      my_event_set_(parent),
     *      my_unique_event_(&Unit::unit_event_set_, "my_unique_event",
     *                       CREATE_SPARTA_HANDLER(MyClass, myCallback))
     * {}
     *
     *
     * // ... called later
     * void MyClass::anotherCallback()
     * {
     *     called_already_ = false;
     *     my_unique_event_.schedule();  // This schedules for NOW
     *     my_unique_event_.schedule();  // ignored
     *     my_unique_event_.schedule();  // ignored
     *     my_unique_event_.schedule();  // ignored
     *     my_unique_event_.schedule(1); // This schedules for NOW + 1
     *     my_unique_event_.schedule(1); // ignored
     * }
     *
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
    class UniqueEvent : public PhasedUniqueEvent
    {
    public:
        // The phase this Event was defined with
        static constexpr SchedulingPhase event_phase = sched_phase_T;

        // Bring in the Scheduleable's schedule methods
        using Scheduleable::schedule;

        /**
         * \brief Create a UniqueEvent.
         * \param event_set The EventSet this UniqueEvent belongs to
         * \param name   The name of this event (as it shows in the EventSet)
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         * \param delay  Preset delay for this event
         *
         * Create a UniqueEvent that will only be scheduled only once
         * per clock phase, no matter how many times this UniqueEvent
         * is scheduled.
         */
        UniqueEvent(TreeNode * event_set,
                    const std::string & name,
                    const SpartaHandler & consumer_event_handler,
                    Clock::Cycle delay = 0) :
            PhasedUniqueEvent(event_set, name, sched_phase_T, consumer_event_handler, delay)
        { }

        //! Uniquely destroy
        virtual ~UniqueEvent() = default;

        //! Disallow moves
        UniqueEvent(UniqueEvent &&) = delete;

        //! Disallow the copying of the UniqueEvent
        UniqueEvent(const UniqueEvent &) = delete;

        //! Disallow the assignment of the UniqueEvent
        UniqueEvent& operator=(const UniqueEvent &) = delete;
    };

}


// __UNIQUE_EVENT_H__
#endif
