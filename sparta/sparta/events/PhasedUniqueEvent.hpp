// <PhasedUniqueEvent.h> -*- C++ -*-


/**
 * \file   PhasedUniqueEvent.hpp
 *
 * \brief  File that defines the PhasedUniqueEvent class
 */

#ifndef __PHASED_UNIQUE_EVENT_H__
#define __PHASED_UNIQUE_EVENT_H__

#include <set>
#include <memory>
#include "sparta/events/EventNode.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    /**
     * \class PhasedUniqueEvent
     *
     * \brief A type of Event that uniquely schedules itself on the
     *        schedule within a single time quantum.  This UniqueEvent
     *        is *not* typed on the SchedulingPhase.  It is
     *        discouraged to use this class -- use UniqueEvent
     *        templatized on the SchedulingPhase
     *
     * See \ref sparta::UniqueEvent for examples.
     */
    class PhasedUniqueEvent : public EventNode, public Scheduleable
    {
    public:
        /**
         * \brief Create a PhasedUniqueEvent. The recommendation
         *        is to use the EventSet::createEvent method to create one.
         * \param event_set The EventSet this PhasedUniqueEvent belongs to
         * \param name   The name of this event (as it shows in the EventSet)
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         * \param delay  Preset delay for this event
         *
         * Create a PhasedUniqueEvent that will only be scheduled only once
         * per clock phase, no matter how many times this PhasedUniqueEvent
         * is scheduled.
         */
        PhasedUniqueEvent(TreeNode * event_set,
                          const std::string & name,
                          SchedulingPhase sched_phase,
                          const SpartaHandler & consumer_event_handler,
                          Clock::Cycle delay = 0) :
            EventNode(event_set, name, sched_phase),
            Scheduleable(CREATE_SPARTA_HANDLER(PhasedUniqueEvent, deliverEvent_), delay, sched_phase),
            pue_consumer_event_handler_(consumer_event_handler),
            fancy_name_(name + "[" + consumer_event_handler.getName() + "]")
        {
            Scheduleable::local_clk_ = getClock();
            Scheduleable::scheduler_ = determineScheduler(local_clk_);
            setLabel(fancy_name_.c_str());
        }

        //! Disallow the copying of the PhasedUniqueEvent
        PhasedUniqueEvent(const PhasedUniqueEvent &) = delete;

        //! Disallow the assignment of the PhasedUniqueEvent
        PhasedUniqueEvent& operator=(const PhasedUniqueEvent &) = delete;

        //! Get the scheduler this Scheduleable is assigned to
        Scheduler * getScheduler(const bool must_exist = true) {
            return Scheduleable::getScheduler(must_exist);
        }

        //! Get the scheduler this Scheduleable is assigned to
        const Scheduler * getScheduler(const bool must_exist = true) const {
            return Scheduleable::getScheduler(must_exist);
        }

        //! Uniquely destroy
        virtual ~PhasedUniqueEvent() {}

        //! Tell callers which method to use for getting the SchedulingPhase
        using EventNode::getSchedulingPhase;

        //! Bring in the Scheduleable's schedule methods
        using Scheduleable::schedule;

        /**
         * \brief Schedule at time rel_tick
         * \param rel_tick The relative tick to schedule
         */
        void scheduleRelativeTick(sparta::Scheduler::Tick rel_tick,
                                  sparta::Scheduler * scheduler) override final
        {
            sparta_assert(scheduler_);
            sparta_assert(last_tick_called_ != (scheduler->getCurrentTick() + rel_tick),
                        "PhasedUniqueEvent (UniqueEvent) '" << getName()
                        << "' was already scheduled and fired this cycle."
                        "\t\nAre you missing a precedence rule?");

            if(!scheduler->isScheduled(this, rel_tick)) {
                Scheduleable::scheduleRelativeTick(rel_tick, scheduler);
            }
        }

#ifndef DO_NOT_DOCUMENT
        // Used by EventNode and auto-precedence.  Return the
        // Scheduleable (this)
        Scheduleable & getScheduleable() override {
            return *this;
        }
#endif

    private:

        //! Set call time, then call the consumer payload.  This
        //! prevents a user from scheduling the PhasedUniqueEvent again in
        //! the same cycle if the event had already been fired
        void deliverEvent_() {
            // I don't like this, but since there are a couple of
            // legal ways to create the PhasedUniqueEvent (one includes the
            // lack of a clock), I cannot remember the time...
            if(SPARTA_EXPECT_TRUE(local_clk_ != nullptr)) {
                last_tick_called_ = local_clk_->getScheduler()->getCurrentTick();
            }
            pue_consumer_event_handler_();
        }

        //! Called by the framework on all TreeNodes
        void createResource_() override {
            Scheduleable::local_clk_ = getClock();
            Scheduleable::scheduler_ = determineScheduler(local_clk_);
        }

        //! The actual callback registered
        SpartaHandler pue_consumer_event_handler_;

        //! The last time this PhasedUniqueEvent was called -- for sanity checking
        Scheduler::Tick last_tick_called_ = Scheduler::Tick(-1);

        //! A fancy name for the event handler
        std::string fancy_name_;
    };

}


// __UNIQUE_EVENT_H__
#endif
