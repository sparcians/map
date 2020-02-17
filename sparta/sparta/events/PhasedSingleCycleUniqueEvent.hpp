// <PhasedSingleCycleUniqueEvent.h> -*- C++ -*-


/**
 * \file   PhasedSingleCycleUniqueEvent.hpp
 *
 * \brief  File that defines the PhasedSingleCycleUniqueEvent class
 */

#ifndef __PHASED_SINGLE_CYCLE_UNIQUE_EVENT_H__
#define __PHASED_SINGLE_CYCLE_UNIQUE_EVENT_H__

#include <array>
#include <memory>
#include <string>
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/EventNode.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    /**
     * \class PhasedSingleCycleUniqueEvent
     * \brief An event that can only be schedule one cycle into the future
     *
     * Analysis shows that modelers using UniqueEvent typically
     * schedule the event either the same cycle or exactly one cycle
     * into the future. UniqueEvent is a bit costly in performance due
     * to the fact that it needs to check with the Scheduler to see if
     * it's already scheduled at the given time. It is discouraged to
     * use this class directly -- use SingleCycleUniqueEvent
     * templatized on the SchedulingPhase
     *
     * See \ref sparta::SingleCycleUniqueEvent for examples.
     */
    class PhasedSingleCycleUniqueEvent : public EventNode
    {
    public:
        /**
         * \brief Create a PhasedSingleCycleUniqueEvent.
         *
         * \param event_set The EventSet this PhasedSingleCycleUniqueEvent belongs to
         * \param name      The name of this event (as it shows in the EventSet)
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         *
         * Create a PhasedSingleCycleUniqueEvent that will only
         * be scheduled only once per clock phase, exactly 1 clock
         * cycle in the future, no matter how many times this
         * PhasedSingleCycleUniqueEvent is scheduled.
         */
        PhasedSingleCycleUniqueEvent(TreeNode * event_set,
                                     const std::string & name,
                                     SchedulingPhase sched_phase,
                                     const SpartaHandler & consumer_event_handler) :
            EventNode(event_set, name, sched_phase),
            local_clk_(getClock()),
            fancy_name_(name + "[" + consumer_event_handler.getName() + "]"),
            single_cycle_event_scheduleable_(consumer_event_handler, 1 /*HARD CODED*/, sched_phase)
        {
            single_cycle_event_scheduleable_.setScheduleableClock(getClock());
            single_cycle_event_scheduleable_.setLabel(fancy_name_.c_str());
        }

        //! Disallow the copying of the PhasedSingleCycleUniqueEvent
        PhasedSingleCycleUniqueEvent(const PhasedSingleCycleUniqueEvent &) = delete;

        //! Disallow the assignment of the PhasedSingleCycleUniqueEvent
        PhasedSingleCycleUniqueEvent& operator=(const PhasedSingleCycleUniqueEvent &) = delete;

        //! Set the internal Scheduleable continuing
        //! \param continuing True if this event should keep simulation alive
        void setContinuing(bool continuing) {
            single_cycle_event_scheduleable_.setContinuing(continuing);
        }

        /**
         * \brief Is this Event continuing?
         * \return true if it will keep the scheduler alive
         */
        bool isContinuing() const {
            return single_cycle_event_scheduleable_.isContinuing();
        }

        /*!
         * \brief Cancel the event for now and one cycle into the future
         */
        void cancel() {
            single_cycle_event_scheduleable_.cancel();
        }

        /*!
         * \brief Return true if this scheduleable was scheduled at all
         * \return true if scheduled at all
         *
         * This is an expensive call as it searches all time quantums
         * for instances of this Scheduleable object.  Use with care.
         */
        bool isScheduled() const {
            return single_cycle_event_scheduleable_.isScheduled();
        }

        //! Uniquely destroy
        virtual ~PhasedSingleCycleUniqueEvent() = default;

        //! Tell callers which method to use for getting the SchedulingPhase
        using EventNode::getSchedulingPhase;

        /*!
         * \brief Schedule this PhasedSingleCycleUniqueEvent exactly
         *        zero or one cycle into the future.  This is the only
         *        schedule call allowed
         *
         * \param rel_cycle Either 0 (default) or 1.  Cannot be
         *                  anything but 0 or 1
         *
         */
        void schedule(Clock::Cycle rel_cycle = 0) {
            sparta_assert(rel_cycle < 2,
                        "Cannot schedule sparta::SingleCycleUniqueEvent:'"
                        << getName() << "' in any relative time other than 0 or 1. rel_cycle given: "
                        << rel_cycle);

            const auto to_be_scheduled_relative_tick =
                local_clk_->getTick(Clock::Cycle(rel_cycle));

            const auto to_be_scheduled_abs_tick =
                local_scheduler_->calcIndexTime(to_be_scheduled_relative_tick);

            if(SPARTA_EXPECT_TRUE(next_scheduled_tick_ < to_be_scheduled_abs_tick))
            {
                // This is a handy debug assertion to see if
                // SingleCycleUniqueEvent is actually only scheduled once.
                //
                // sparta_assert(single_cycle_event_scheduleable_.isScheduled(rel_cycle) == false);
                single_cycle_event_scheduleable_.
                    scheduleRelativeTick(to_be_scheduled_relative_tick, local_scheduler_);
                prev_scheduled_tick_ = next_scheduled_tick_;
                next_scheduled_tick_ = to_be_scheduled_abs_tick;
            }
            else if(to_be_scheduled_abs_tick < next_scheduled_tick_)
            {
                if(prev_scheduled_tick_ != to_be_scheduled_abs_tick) {
                    // This is a handy debug assertion to see if
                    // SingleCycleUniqueEvent is actually only scheduled once.
                    //
                    // sparta_assert(single_cycle_event_scheduleable_.isScheduled(rel_cycle) == false);
                    single_cycle_event_scheduleable_.
                        scheduleRelativeTick(to_be_scheduled_relative_tick, local_scheduler_);
                    prev_scheduled_tick_ = to_be_scheduled_relative_tick;
                }
            }
        }

#ifndef DO_NOT_DOCUMENT
        // Used by EventNode and auto-precedence.  Return the
        // Scheduleable (this)
        Scheduleable & getScheduleable() override {
            return single_cycle_event_scheduleable_;
        }
#endif

    private:
        //! Called by the framework on all TreeNodes
        void createResource_() override {
            local_clk_ = getClock();
            local_scheduler_ = local_clk_->getScheduler();
        }

        //! A local clock for speed
        const Clock * local_clk_ = nullptr;

        //! A local scheduler for speed
        Scheduler * local_scheduler_ = nullptr;

        //! The last time this PhasedSingleCycleUniqueEvent was scheduled (rel cycle == [0,1])
        //std::array<Scheduler::Tick, 2> next_scheduled_tick_ = {0, 0};
        Scheduler::Tick next_scheduled_tick_ = 0;
        Scheduler::Tick prev_scheduled_tick_ = 0;

        //! A fancy name for the event handler
        std::string fancy_name_;

        //! The actual scheduled item on the scheduler
        Scheduleable single_cycle_event_scheduleable_;
    };

}


// __UNIQUE_EVENT_H__
#endif
