// <PayloadEvent.h> -*- C++ -*-


/**
 * \file   PhasedPayloadEvent.hpp
 *
 * \brief File that defines the PhasedPayloadEvent class.  Suggest
 *        using sparta::PayloadEvent instead
 */

#ifndef __PHASED_PAYLOAD_EVENT_H__
#define __PHASED_PAYLOAD_EVENT_H__

#include <list>
#include <memory>
#include "sparta/events/EventNode.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace sparta
{

    /**
     * \class PhasedPayloadEvent
     *
     * \brief Class to schedule a Scheduleable in the future with a
     *        payload, but the class itself is not typed on the phase,
     *        but is constructed with one.  The preference, however,
     *        is for the modeler to use sparta::PayloadEvent instead.
     *
     * \tparam DataT The payload's data type
     *
     * See \ref sparta::PayloadEvent for usage examples
     */
    template<class DataT>
    class PhasedPayloadEvent : public EventNode
    {
    private:

        class PayloadDeliveringProxy;
        using ProxyAllocation   = std::vector<std::unique_ptr<PayloadDeliveringProxy>>;
        using ProxyFreeList     = std::vector<PayloadDeliveringProxy *>;
        using ProxyInflightList = std::list  <PayloadDeliveringProxy *>;

        //! Internal class used by PhasedPayloadEvent to schedule the
        //! delivery of a payload to a consumer sometime in the
        //! future.  As long as the PhasedPayloadEvent stays alive, so does
        //! this object.
        class PayloadDeliveringProxy : public Scheduleable
        {
        public:

            void scheduleRelativeTick(sparta::Scheduler::Tick rel_tick,
                                      sparta::Scheduler * scheduler) override
            {
                sparta_assert((cancelled_ == false) && (scheduled_ == false),
                            "This Payload handle is already scheduled or was previously cancelled.  "
                            "To schedule again, you must create a new one");
                Scheduleable::scheduleRelativeTick(rel_tick, scheduler);
                scheduled_ = true;
            }


            PayloadDeliveringProxy(const Scheduleable & prototype,
                                   PhasedPayloadEvent<DataT> * parent) :
                Scheduleable(prototype),
                parent_(parent),
                target_consumer_event_handler_(prototype.getHandler()),
                loc_(parent_->inflight_pl_.end())
            {
                // Reset the base class consumer event handler to be
                // this class' delivery proxy
                Scheduleable::consumer_event_handler_ =
                    CREATE_SPARTA_HANDLER(PayloadDeliveringProxy, deliverPayload_);
            }

        private:

            // Make the parent class a friend
            friend class PhasedPayloadEvent;

            // If this Scheduleable is managed by a
            // ScheduleableHandle, then this method is called when the
            // Handle goes out of scope.  The proxy is only reclaimed
            // if it's not on the Scheduler and there are not handles
            // pointing to it.
            void reclaim_() override {
                if(!scheduled_ && (getScheduleableHandleCount_() == 0)) {
                    parent_->reclaimProxy_(loc_);
                    cancelled_ = false;
                }
            }

            // This Scheduleable was cancelled via an indirect cancel
            // on the Scheduler
            void eventCancelled_() override {
                scheduled_ = false;
                cancelled_ = true;
                reclaim_();
            }

            // Set a payload for a delayed delivery
            void setPayload_(const DataT & pl) {
                sparta_assert(scheduled_ == false);
                payload_ = pl;
            }

            // Get a payload for a delayed delivery
            const DataT & getPayload_() const {
                return payload_;
            }


            // Set the location in the inflight list of the parent for
            // quick removal
            void setInFlightLocation_(const typename ProxyInflightList::iterator & loc) {
                loc_ = loc;
            }

            // Called by the SPARTA scheduler.  Deliver the payload to
            // the user's callback.  Then, recycle this Proxy
            void deliverPayload_() {
                sparta_assert(scheduled_ == true,
                            "Some construct is trying to deliver a payload twice: "
                            << parent_->name_ << " to handler: "
                            << target_consumer_event_handler_.getName());
                scheduled_ = false;
                target_consumer_event_handler_((const void*)&payload_);
                reclaim_();
            }

            PhasedPayloadEvent<DataT> * parent_ = nullptr;
            const SpartaHandler         target_consumer_event_handler_;
            DataT                       payload_;
            typename ProxyInflightList::iterator loc_;
            bool scheduled_ = false;
            bool cancelled_ = false;
        };


        //! Allocate a delivering proxy for the payload.
        ScheduleableHandle allocateProxy_(const DataT & dat)
        {
            PayloadDeliveringProxy * proxy = nullptr;
            if(SPARTA_EXPECT_TRUE(free_idx_ != 0)) {
                --free_idx_;
                proxy = free_pl_[free_idx_];
            }
            else {
                // See if we hit the ceiling
                if(allocation_idx_ == allocated_proxies_.size()) {
                    addProxies_();
                }

                // Take one from the allocation list
                proxy = allocated_proxies_[allocation_idx_].get();
                ++allocation_idx_;

                sparta_assert(allocation_idx_ < 100000,
                              "The PayloadEvent: '" << getLocation() <<
                              "' has allocatged over 100000 outstanding events -- does that seem right?");
            }
            inflight_pl_.push_front(proxy);
            proxy->setInFlightLocation_(inflight_pl_.begin());
            proxy->setPayload_(dat);

            return proxy;
        }

        //! Allow the proxy to reclaim itself
        friend class PayloadDeliveringProxy;

        //! The payload was delivered, this proxy object is now
        //! reusable.
        void reclaimProxy_(typename ProxyInflightList::iterator & pl_location) {
            sparta_assert(pl_location != inflight_pl_.end());
            free_pl_[free_idx_++] = *pl_location;
            inflight_pl_.erase(pl_location);
            pl_location = inflight_pl_.end();
        }

    public:

        /*
         * \brief Create a PhasedPayloadEvent to deliver data at a particular time
         * \param event_set The sparta::EventSet this PhasedPayloadEvent belongs to
         * \param name      The name of this event (as it shows in the EventSet)
         * \param sched_phase The SchedulingPhase this PhasedPayloadEvent belongs to
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         * \param delay The relative time (in Cycles) from "now" to schedule
         *
         * The suggestion is to use the derived class sparta::PayloadEvent
         * instead of this class directly.
         */
        PhasedPayloadEvent(TreeNode            * event_set,
                           const std::string   & name,
                           SchedulingPhase       sched_phase,
                           const SpartaHandler & consumer_event_handler,
                           Clock::Cycle          delay = 0) :
            EventNode(event_set, name, sched_phase),
            name_(name + "[" + consumer_event_handler.getName() + "]"),
            prototype_(consumer_event_handler, delay, sched_phase)
        {
            sparta_assert(consumer_event_handler.argCount() == 1,
                          "You must assign a PhasedPayloadEvent a consumer handler ""that takes exactly one argument");
            prototype_.setScheduleableClock(getClock());
            prototype_.setScheduler(determineScheduler(getClock()));
        }

        //! Destroy!
        virtual ~PhasedPayloadEvent() {}

        //! No assignments, no copies
        PhasedPayloadEvent<DataT> & operator=(const PhasedPayloadEvent<DataT> &) = delete;
        //! No assignments, no copies
        PhasedPayloadEvent(PhasedPayloadEvent<DataT> &&) = delete;
        //! No assignments, no copies
        PhasedPayloadEvent(const PhasedPayloadEvent<DataT> &) = delete;

        /**
         * \brief Prepare a Scheduleable Payload for scheduling either
         *        now or later
         * \param payload The payload to eventually deliver
         * \return A handle the Scheduleable item.  Can be scheduled
         *         now or sometime in the future
         *
         * Used in both cases where a payload will be delivered either
         * now or later.  For the later case, this is usually where a
         * payload is to be delivered when <i>something else</i>
         * triggers the scheduling of the delivery.  For example, when
         * state changes in another block.
         */
        ScheduleableHandle preparePayload(const DataT & payload) {
            auto proxy = allocateProxy_(payload);
            return ScheduleableHandle(proxy);
        }

        //! Overload precedence operator for PhasedPayloadEvents since they
        //! are not Scheduleables
        Scheduleable& operator>>(Scheduleable & consumer)
        {
            prototype_ >> consumer;
            return consumer;
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Get the underlying Scheduleable prototype
        Scheduleable & getScheduleable() override {
            return prototype_;
        }

        /**
         * \brief This event, if continuing == true, will keep the
         * simulation running
         *
         * If this event is NOT a continuing event, just simply
         * scheduling it will NOT keep simulation running.  This is
         * useful for events like heartbeats, etc.
         */
        void setContinuing(bool continuing) {
            getScheduleable().setContinuing(continuing);
        }

        /**
         * \brief Return the number of unfired/unscheduled Payloads
         * \return The number of unscheduled or scheduled, but unfired events
         *
         * This count includes the number of outstanding handles to
         * PayloadEvent returned from preparePayload
         */
        uint32_t getNumOutstandingEvents() const {
            return inflight_pl_.size();
        }

        //! \brief Determine if this PhasedPayloadEvent is driven on the
        //!        given cycle
        //! \param rel_cycle The relative cycle (from now) the data
        //!                  will be delivered
        //! \return true if driven at the given cycle (data not yet delivered)
        bool isScheduled(Clock::Cycle rel_cycle) const {
            for(auto * proxy : inflight_pl_) {
                if(proxy->isScheduled(rel_cycle)) {
                    return true;
                }
            }
            return false;
        }

        //! \brief Is this PhasedPayloadEvent have anything scheduled?
        //! \return true if driven at all (data not yet delivered)
        bool isScheduled() const {
            return getNumOutstandingEvents() > 0;
        }

        /**
         * \brief Cancel all inflight PayloadEvents
         * \return The number of canceled events
         */
        uint32_t cancel()
        {
            const uint32_t cancel_cnt = inflight_pl_.size();
            // Cancelling the event will change the inflight_pl_
            // list, hence we cannot use a range for loop
            auto bit = inflight_pl_.begin();
            while(bit != inflight_pl_.end()) {
                auto proxy = *bit;
                ++bit;
                proxy->cancel();
            }
            return cancel_cnt;
        }

        /*!
         * \brief Cancel inflight PayloadEvents at the given relative cycle
         * \param rel_cycle The relative time to look for the event
         * \return The number of canceled events
         *
         * This will cancel all instances of this event at the given cycle.
         */
        uint32_t cancel(Clock::Cycle rel_cycle) {
            const uint32_t cancel_cnt = inflight_pl_.size();
            auto bit = inflight_pl_.begin();
            while(bit != inflight_pl_.end()) {
                auto proxy = *bit;
                ++bit;
                // Cancelling the event will change the inflight_pl_
                // list, hence we cannot use a range for loop
                proxy->cancel(rel_cycle);
            }
            return cancel_cnt;
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given criteria
         * \param criteria The criteria to compare; must respond to operator==
         * \return The number of canceled events
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload is squashed and the event unscheduled (if scheduled)
         */
        uint32_t cancelIf(const DataT & criteria)
        {
            uint32_t cancel_cnt = 0;
            // Cancelling the event will change the inflight_pl_
            // list, hence we cannot use a range for loop
            auto bit = inflight_pl_.begin();
            while(bit != inflight_pl_.end()) {
                auto proxy = *bit;
                ++bit;
                if(proxy->getPayload_() == criteria) {
                    proxy->cancel();
                    ++cancel_cnt;
                }
            }

            return cancel_cnt;
        }

        /**
         * \brief Return a vector of scheduleable handles that match the given criteria
         * \param criteria The criteria to compare; must respond to operator==
         * \return Vector of scheduleables
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload's scheduleable handle is returned.
         */
        std::vector<Scheduleable*> getHandleIf(const DataT & criteria) {
            std::vector<Scheduleable*> ple_vector;
            for(auto * proxy : inflight_pl_)
            {
                if(proxy->getPayload_() == criteria) {
                    ple_vector.push_back(proxy);
                }
            }
            return ple_vector;
        }

        /**
         * \brief Confirm if any scheduled payload matches the given criteria
         * \param criteria The criteria to compare; must respond to operator==
         * \return The existence of an event matching a given criteria
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload is confirmed.
         */
        bool confirmIf(const DataT & criteria) {
            for(auto * proxy : inflight_pl_)
            {
                if(proxy->getPayload_() == criteria) {
                    return true;
                }
            }
            return false;
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given function
         * \param compare The function comparitor to use
         * \return The number of canceled events
         *
         * This function allows a user to define his/her own
         * comparison operation outside of a direct operator== comparison.
         *
         * Example usage:
         * \code
         *
         * class MyPayload
         * {
         * public:
         *     MyPayload (const uint32_t v) : a_val_(v) {}
         *
         *      bool isItAMatch(const MyPayload & other) const {
         *         return a_val_ == other.a_val_;
         *     }
         *
         * private:
         *     uint32_t a_val_;
         * };
         *
         * // Using Lambda function
         * MyPayload mple(10);
         * my_payload_event_.cancelIf([&mple](const MyPayload & other) -> bool {
         *                                return mple.isItAMatch(other);
         *                            });
         *
         * // or prettier:
         * auto matchCompareFunc = std::bind(&MyPayload::isItAMatch, mple, std::placeholders::_1);
         * my_payload_event_.cancelIf(matchCompareFunc);
         *
         * \endcode
         */
        uint32_t cancelIf(std::function<bool(const DataT &)> compare) {
            uint32_t cancel_cnt = 0;
            // Cancelling the event will change the inflight_pl_
            // list, hence we cannot use a range for loop
            auto bit = inflight_pl_.begin();
            while(bit != inflight_pl_.end()) {
                auto proxy = *bit;
                ++bit;
                if(compare(proxy->getPayload_())) {
                    proxy->cancel();
                    ++cancel_cnt;
                }
            }
            return cancel_cnt;
        }

        /**
         * \brief Return a vector of scheduleable handles that match the given function
         * \param criteria The criteria to compare; must respond to operator==
         * \return Vector of scheduleables
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload's scheduleable handle is returned.
         *
         * Example usage:
         * \code
         *
         * class MyPayload
         * {
         * public:
         *     MyPayload (const uint32_t v) : a_val_(v) {}
         *
         *      bool isItAMatch(const MyPayload & other) const {
         *         return a_val_ == other.a_val_;
         *     }
         *
         * private:
         *     uint32_t a_val_;
         * };
         *
         * // Using Lambda function
         * MyPayload mple(10);
         * auto handle_vector = my_payload_event_.getHandleIf([&mple](const MyPayload & other) -> bool {
         *                                return mple.isItAMatch(other);
         *                            });
         * // do something with handle_vector
         *
         * // or prettier:
         * auto matchCompareFunc = std::bind(&MyPayload::isItAMatch, mple, std::placeholders::_1);
         * auto handle_vector = my_payload_event_.getHandleIf(matchCompareFunc);
         * // do something with handle_vector
         *
         * \endcode
         */
        std::vector<Scheduleable*> getHandleIf(std::function<bool(const DataT &)> compare) {
            std::vector<Scheduleable*> ple_vector;
            for(auto * proxy : inflight_pl_) {
                if(compare(proxy->getPayload_())) {
                    ple_vector.push_back(proxy);
                }
            }
            return ple_vector;
        }

        /**
         * \brief Confirm if any scheduled payload matches the given criteria
         * \param compare The function comparitor to use
         * \return The existence of an event matching a given criteria
         *
         * This function allows a user to define his/her own
         * comparison operation outside of a direct operator== comparison.
         *
         * Example usage:
         * \code
         *
         * class MyPayload
         * {
         * public:
         *     MyPayload (const uint32_t v) : a_val_(v) {}
         *
         *      bool isItAMatch(const MyPayload & other) const {
         *         return a_val_ == other.a_val_;
         *     }
         *
         * private:
         *     uint32_t a_val_;
         * };
         *
         * // Using Lambda function
         * MyPayload mple(10);
         * const bool confirm = my_payload_event_.confirmIf([&mple](const MyPayload & other) -> bool {
         *                                return mple.isItAMatch(other);
         *                            });
         *
         * // or prettier:
         * auto matchCompareFunc = std::bind(&MyPayload::isItAMatch, mple, std::placeholders::_1);
         * const bool confirm = my_payload_event_.cancelIf(matchCompareFunc);
         *
         * \endcode
         */
        uint32_t confirmIf(std::function<bool(const DataT &)> compare) {
            for(auto * proxy : inflight_pl_)
            {
                if(compare(proxy->getPayload_())) {
                    return true;
                }
            }
            return false;
        }

    private:

        friend class Scheduler;
        /*
         * \brief Create an PayloadEvent to deliver a payload in the future.
         *
         * \param event_set The sparta::EventSet this PhasedPayloadEvent belongs to
         * \param scheduler Pointer to the sparta::Scheduler that would schedule this event
         * \param name      The name of this event (as it shows in the EventSet)
         * \param sched_phase The SchedulingPhase this PhasedPayloadEvent belongs to
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         * \param delay The relative time (in Cycles) from "now" to schedule
         *
         * \note This constructor is restricted to be used by the sparta::Scheduler only
         *       in order to support sparta::GlobalEvent
         */
        PhasedPayloadEvent(TreeNode            * event_set,
                           Scheduler           * scheduler,
                           const std::string   & name,
                           SchedulingPhase       sched_phase,
                           const SpartaHandler & consumer_event_handler,
                           Clock::Cycle          delay = 0) :
            EventNode(event_set, name, sched_phase),
            name_(name + "[" + consumer_event_handler.getName() + "]"),
            prototype_(consumer_event_handler, delay, sched_phase)
        {

            sparta_assert(consumer_event_handler.argCount() == 1,
                          "You must assign a PhasedPayloadEvent a consumer handler ""that takes exactly one argument");
            prototype_.setScheduler(scheduler);
        }

    private:

        //! Called by the framework on all TreeNodes
        void createResource_() override {
            prototype_.setScheduleableClock(getClock());
            prototype_.setScheduler(determineScheduler(getClock()));

            // Make sure no proxies are outstanding and all are allocated
            sparta_assert(inflight_pl_.empty());

        }

        void addProxies_()
        {
            const uint32_t old_size = allocated_proxies_.size();
            const uint32_t new_size = payload_proxy_allocation_cadence_ + old_size;
            allocated_proxies_.resize(new_size);
            for(uint32_t i = old_size; i < new_size; ++i) {
                allocated_proxies_[i].reset(new PayloadDeliveringProxy(prototype_, this));
            }
            free_pl_.resize(new_size, nullptr);
        }

        //! Fancy name for this PhasedPayloadEvent
        std::string       name_;

        //! Prototype used for creating proxy objects
        Scheduleable      prototype_;

        ProxyAllocation   allocated_proxies_;
        ProxyFreeList     free_pl_;
        ProxyInflightList inflight_pl_;

        // Use 16, a power of 2 for allocation of more objects.  No
        // rhyme or reason, but this seems to be a sweet spot in
        // performance.
        const uint32_t payload_proxy_allocation_cadence_ = 16;
        uint32_t free_idx_           = 0;
        uint32_t allocation_idx_     = 0;

    };
}


// __PHASED_PAYLOAD_EVENT_H__
#endif
