// <GlobalEvent.h> -*- C++ -*-


/**
 * \file   GlobalEvent.hpp
 * \brief  File that defines the GlobalEvent class
 */

#ifndef __GLOBAL_EVENT_H__
#define __GLOBAL_EVENT_H__

#include <memory>
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/utils/LifeTracker.hpp"
#include "sparta/events/PhasedPayloadEvent.hpp"
#include "sparta/events/SchedulingPhases.hpp"

namespace sparta
{
    template<SchedulingPhase sched_phase_T>
    class GlobalEvent;

    /**
     * \class GlobalEventProxy
     *
     * \brief A helper class of GlobalEvent
     *
     * GlobalEventProxy itself monitors a event handler and remembers its scheduling phase.
     * It triggers the event callback only when it still exists (with the help of std::weak_ptr)
     * It appears as an "event" payload of sparta::PhasedPayloadEvent in the sparta::Scheduler.
     */
    class GlobalEventProxy
    {
    public:
        GlobalEventProxy() = default;
        GlobalEventProxy(const GlobalEventProxy&) = default;

        template<SchedulingPhase sched_phase_T = SchedulingPhase::Update>
        GlobalEventProxy(const utils::LifeTracker<SpartaHandler> & handler) :
            phase_(sched_phase_T),
            ev_handler_(handler)
        {}

        /**
         * \brief function call operator of GlobalEventProxy
         *
         * It supports the "dynamic" event scheduling semantic provided by sparta::GlobalEvent by
         * triggering the event callback only when it still exists (with the help of std::weak_ptr)
         */
        void operator()(void) const
        {
            if(!ev_handler_.expired()) {
                auto sptr = ev_handler_.lock();
                (*(sptr->tracked_object))();
            }
        }

        const SchedulingPhase & getSchedulingPhase() const { return phase_; }

        ~GlobalEventProxy() {
            //std::cout << "Destroy GlobalEventProxy!\n";
        }

    private:
        SchedulingPhase                                 phase_;
        std::weak_ptr<utils::LifeTracker<SpartaHandler>> ev_handler_;
   };

    /**
     * \class GlobalEvent
     *
     * \brief A type of "global" reusable event
     *
     * This is to support dynamically created objects that require
     * event semantics, but cannot create events.  The original
     * requirement comes from sparta::CoreExample: On the one hand,
     * each dummy instruction contains a sparta::SharedData instance
     * to represent and update its status.  sparta::SharedData
     * contains event handler to support "delayed update (from
     * present-state to next-state)" semantics.  On the other hand,
     * the sparta::SharedData instance is required to be "copyable",
     * since dummy instructions are created dynamically in the fetch
     * unit.  However, the original implementation of
     * sparta::SharedData uses sparta::UniqueEvent, which is not
     * copyable at run time.
     *
     * To support this, the concept of "global" reusable events can be created, but with certain cautions in place:
     * #. The Scheduleable being scheduled will be unique to a specific object requiring a timed call.
     * #. The Scheduleable being scheduled might point to a dead callback (the originator of the event might be deallocated).
     * #. The Scheduleable can be scheduled immediately, in the immediate future, or far into the future.
     *
     */
    template<SchedulingPhase sched_phase_T = SchedulingPhase::Update>
    class GlobalEvent
    {
    public:
        /**
         * \brief Create a GlobalEvent.
         *
         * \param clk The clock to which synchronized by this global event
         * \param event_handler The event handler of this global event
         */
        GlobalEvent(const Clock * clk,
                    const SpartaHandler & event_handler) :
            local_clk_(clk),
            event_handler_(event_handler),
            ev_sched_ptr_(clk->getScheduler()->getGlobalPhasedPayloadEventPtr<sched_phase_T>())
        {}

        void schedule(const Clock::Cycle & delay, const Clock * clk) {
            sparta_assert(ev_sched_ptr_ != nullptr);
            sparta_assert(ev_sched_ptr_->getSchedulingPhase() == sched_phase_T);

            ev_sched_ptr_->preparePayload(GlobalEventProxy(ev_handler_lifetime_))->schedule(delay, clk);
        }

        void schedule(const Clock::Cycle & delay) {
            sparta_assert(ev_sched_ptr_ != nullptr);
            sparta_assert(ev_sched_ptr_->getSchedulingPhase() == sched_phase_T);

            ev_sched_ptr_->preparePayload(GlobalEventProxy(ev_handler_lifetime_))->schedule(delay, local_clk_);
        }

        void resetHandler(const SpartaHandler & event_handler) {
            event_handler_ = event_handler;
        }

        ~GlobalEvent() {
            //std::cout << "Destroy GlobalEvent!\n";
        }

        friend class GlobalEventProxy;

    private:
        const Clock                          * local_clk_ = nullptr;
        SpartaHandler                          event_handler_;
        utils::LifeTracker<SpartaHandler>      ev_handler_lifetime_{&event_handler_};
        PhasedPayloadEvent<GlobalEventProxy> * ev_sched_ptr_;
    };

}


// __GLOBAL_EVENT_H__
#endif
