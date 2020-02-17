// <Event.h> -*- C++ -*-


/**
 * \file   Event.hpp
 *
 * \brief  File that defines the Event class
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#include <set>
#include <memory>

#include "sparta/events/EventNode.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    /**
     * \brief Event is a simple class for scheduling random events on the Scheduler
     *
     * The purpose of this class is for a user to create an event
     * specific to an action within simulation, but can occur at
     * random times and possibly many times during a time quantum.
     *
     *
     * Behaviors of the default Event:
     * \li No guarantee on order with repect to other Event if no precedence is established
     * \li If an Event is scheduled multiple times within same cycle,
     *     the consumer's callback is called multiple times within the
     *     targeted cycle [If this is undesired behavior, use
     *     sparta::UniqueEvent]
     * \li If a precedence is established, it is guaranteed that one
     *     Event will be scheduled before or after the other
     *     always, depending on the order requested
     */
    template<SchedulingPhase sched_phase_T = SchedulingPhase::Tick>
    class Event : public EventNode, public Scheduleable
    {
    public:
        // The phase this Event was defined with
        static constexpr SchedulingPhase event_phase = sched_phase_T;

        // Bring in the Scheduleable's schedule methods
        using Scheduleable::schedule;

        /**
         * \brief Create a generic Event. This type of class is hardly used, but available.
         *
         * \param event_set Pointer to the sparta::EventNodeSet this EventNode is part of
         * \param name   The name of this event (as it shows in the EventNodeSet)
         * \param clk    The clock this EventNode will use to schedule itself
         * \param delay  Preset delay for this event
         * \param scheduler Scheduler on which this even will scheduler. If nullptr,
         *        uses the scheduler attached to \a clk. If clk is nullptr or has no scheduler, uses
         *        the singleton scheduler
         *
         * Create a EventNode that will be associated with events placed
         * on the scheduler once data is send from one component to
         * another.
         *
         * The best practice for using events within a sparta::Resource
         * block, is to create a persistent sparta::EventNodeSet within the
         * resource and create events (which return handles) for use
         * within simulation.  For example:
         *
         * \code
         *
         * class MyResource : public sparta::Resource
         * {
         * public:
         *     // ...
         * private:
         *     sparta::UniqueEventNode & my_unique_event_;
         *     sparta::EventNode       & my_regular_event_;
         * };
         *
         * MyResource::MyResource(TreeNode * node, MyResourceParams * params) :
         *     my_event_set_(node),
         *     // ....
         *     my_unique_event_(my_event_set_.createEventNode<sparta::UniqueEventNode>(...)),
         *     my_regular_event_(my_event_set_.createEventNode<sparta::EventNode>(...))
         * {
         * }
         *
         * void MyResource::myFunction()
         * {
         *     my_unique_event_.schedule();
         * }
         *
         * \endcode
         */
        Event(TreeNode * event_set,
              const std::string & name,
              const SpartaHandler & consumer_event_handler,
              Clock::Cycle delay = 0) :
            EventNode(event_set, name, sched_phase_T),
            Scheduleable(consumer_event_handler, delay, sched_phase_T),
            fancy_name_(name + "[" + consumer_event_handler.getName() + "]")
        {
            Scheduleable::local_clk_ = getClock();
            Scheduleable::scheduler_ = determineScheduler(local_clk_);
            setLabel(fancy_name_.c_str());
        }

        // Used by EventNode and auto-precedence.  Return the
        // Scheduleable (this)
        Scheduleable & getScheduleable() override {
            return *this;
        }

    private:

        //! Called by the framework on all TreeNodes
        void createResource_() override {
            Scheduleable::local_clk_ = getClock();
            Scheduleable::scheduler_ = determineScheduler(local_clk_);
        }

        //! A fancy name for the event handler
        std::string fancy_name_;

    };
}


// __EVENT_H__
#endif
