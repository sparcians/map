// <TriggerManager.hpp>  -*- C++ -*-



/**
 * \file TriggerManager.hpp
 * \brief Manages implementation of certain types of triggers. Does not actually
 * own triggers.
 */

#ifndef __TRIGGER_MANAGER_H__
#define __TRIGGER_MANAGER_H__

#include <cstdint>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "sparta/trigger/ManagedTrigger.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"

namespace sparta {

    /*!
     * \brief Contains mechanisms for triggering actions based on simulator
     * behavior in more complex ways than simply scheduling by ticks or cycles
     */
    namespace trigger {

/*!
 * \brief Manages implementation of certain types of triggers. Does not actually
 * own triggers. This is used internally by ManagedTriggers
 * \note This is intended to be used as a singleton
 */
class TriggerManager
{
public:

    /*!
     * \brief Token to place into constructed_token_ member when the
     * TriggerManager static instance is initialized. This prevents access
     * before initialization
     */
    static const uint32_t CONSTRUCTED_TOKEN = 0x12345678;

    /*!
     * \brief Access to singleton instance
     */
    static TriggerManager& getTriggerManager()
    {
        sparta_assert(trig_man_singleton_.constructed_token_ == CONSTRUCTED_TOKEN,
                          "TriggerManager singleton was not yet statically initialized, before "
                          "getTriggerManager was called");
        return trig_man_singleton_;
    }

    /*!
     * \brief Default constructor
     * \note This can only be used to construct the static singleton instance trig_man_singleton_
     */
    TriggerManager() :
        constructed_token_(CONSTRUCTED_TOKEN)
    {
        sparta_assert(this == &trig_man_singleton_,
                          "Cannot construct a TriggerManager. Only the singleton instance must be "
                          "used");
    }

    /*!
     * \brief Destructor
     */
    ~TriggerManager()
    {
        // Note: assumes this is calle in static uninitialization
        for(const auto& ch : clocks_){
            if(ch.second->getNumTriggers() > 0){
                std::cerr << "Some ManagedTriggers were not destroyed before static uninitialization. "
                             "This is probably a mistake and a memory leak" << std::endl;
                break;
            }
        }
    }

    /*!
     * \brief Add a ManagedTrigger to update
     * \param trig Trigger to add. This must not already be in the TriggerManager. Must not be
     * nullptr
     */
    void addTrigger(ManagedTrigger* trig) {
        sparta_assert(trig != nullptr,
                          "Cannot add null trigger to TriggerManager");
        sparta_assert(trig->getClock() != nullptr,
                          "Cannot add Trigger " << trig << " (which has a null clock) to TriggerManager");

        // Check if the manager already has this trigger. Note that this checks each ClockHandler
        // and considers deferred added/removed
        sparta_assert(hasTrigger(trig) == false,
                          "Cannot add Trigger " << trig << " to TriggerManager");

        ClockHandler* handler = nullptr;
        auto handler_itr = clocks_.find(trig->getClock());
        if(handler_itr == clocks_.end()){
            // Does not have a handler for this clock
            clocks_[trig->getClock()].reset(handler = new ClockHandler(trig->getClock()));
        }else{
            handler = handler_itr->second.get();
        }

        handler->addTrigger(trig);
    }

    /*!
     * \brief Does this handler have a particular trigger
     * \param trig Trigger to look for. This handler will never have null triggers
     */
    bool hasTrigger(ManagedTrigger* trig) const {
        auto handler_itr = clocks_.find(trig->getClock());
        if(handler_itr == clocks_.end()){
            return false;
        }
        return handler_itr->second->hasTrigger(trig);
    }

    /*!
     * \brief Remove a trigger from this handler
     * \param trig Trigger to remove. Has no effect if not found (or if nullptr)
     */
    void removeTrigger(const ManagedTrigger* trig) {
        if(nullptr == trig){
            return;
        }

        auto handler_itr = clocks_.find(trig->getClock());
        if(handler_itr == clocks_.end()){
            return; // Does not have a handler for this clock
        }

        handler_itr->second->removeTrigger(trig);
        if (handler_itr->second->getNumTriggers() == 0) {
            clocks_.erase(handler_itr);
        }
    }

private:

    /*!
     * \brief Handles ticks on a particular clock to query counters operating on that clock
     */
    class ClockHandler
    {
        /*!
         * \brief Helper for setting the int_tick_ flag in the ClockHandler class in a way that is
         * exception safe
         */
        struct TickLock {
            ClockHandler& ch_;
            TickLock(ClockHandler& ch) : ch_(ch) { ch.in_tick_ = true; }
            ~TickLock() {
                ch_.in_tick_ = false;
                ch_.handleDeferredRemovals_();
                ch_.handleDeferredAdditions_();
            }
        };

        friend struct TickLock;

    public:

        /*!
         * \brief Constructor
         * \post Sets up callbacks on this clock's ticks directed to clockTick_
         */
        ClockHandler(const Clock* clock) :
            clock_(clock),
            event_("clock_handler_event_" + notNull(clock)->getName(),
                   CREATE_SPARTA_HANDLER(ClockHandler, clockTick_),
                   clock),
            in_tick_(false)
        {
            // Schedule for the top of the tick on the next cycle
            event_.schedule(1);
        }

        /*!
         * \brief Destructor
         */
        ~ClockHandler()
        {;}

        /*!
         * \brief Gets the number of triggers currently observed by this handler
         */
        uint32_t getNumTriggers() const {
            return triggers_.size();
        }

        /*!
         * \brief Add a trigger to this handler
         * \param trig Trigger to add. This must not already be in this handler. Must not be nullptr
         */
        void addTrigger(ManagedTrigger* trig) {
            sparta_assert(trig != nullptr,
                              "Cannot add null trigger to Clock Handler for " << clock_);
            sparta_assert(hasTrigger(trig) == false,
                              "Cannot add Trigger " << trig << " to Clock Handler for " << clock_);

            if(in_tick_){
                addTriggerDefferred_(trig);
            }else{
                addTriggerNow_(trig);
            }
        }

        /*!
         * \brief Does this handler have a particular trigger
         * \param trig Trigger to look for. This handler will never have null triggers
         */
        bool hasTrigger(ManagedTrigger* trig) const {
            // Note: the order of these tests reflects the order of TickLock in that it removes
            // deferred triggers from the deferred to-remove list, then adds triggers from the
            // deferred to-add list
            auto cur_itr = std::find(triggers_.begin(), triggers_.end(), trig);
            if(cur_itr != triggers_.end()){
                auto removed_itr = std::find(to_remove_.begin(), to_remove_.end(), trig);
                if(removed_itr != to_remove_.end()){
                    auto added_itr = std::find(to_add_.begin(), to_add_.end(), trig);
                    if(added_itr != to_add_.end()){
                        return true; // Has trigger, but it is in to-remove list, but it is also in the to-add list
                    }else{
                        return false; // Has trigger, but it is in to-remove list (and not to-add list)
                    }
                }else{
                    return true; // Has trigger, not in to-remove list
                }
            }else{
                auto added_itr = std::find(to_add_.begin(), to_add_.end(), trig);
                if(added_itr != to_add_.end()){
                    return true; // Does not currently trigger, but it is in the to-add list
                }else{
                    return false; // Does not currently trigger and it is not in the to-add list
                }
            }
        }

        /*!
         * \brief Remove a trigger from this handler
         * \param trig Trigger to remove. Has no effect if not found (or if nullptr)
         * \note Removals may be defferred if within a trigger callback.
         * Deferred removals will then be handled at the end of clockTick_
         */
        void removeTrigger(const ManagedTrigger* trig) {
            if(nullptr == trig){
                return;
            }

            if(in_tick_){
                removeTriggerDefferred_(trig);
            }else{
                removeTriggerNow_(trig);
            }
        }

    private:

        /*!
         * \brief Add a trigger at the end of a tick
         * \post trig will not already be contained in this handler (or to_add_ list)
         */
        void addTriggerDefferred_(ManagedTrigger* trig) {
            sparta_assert(in_tick_,
                              "ClockHandler addTriggerDefferred_ called but ClockHandler was not "
                              "currently within a tick");

            to_add_.push_back(trig);
        }

        /*!
         * \brief Add a trigger immediately
         * \param trig Trigger to remove. Has no effect if not found
         * \pre trig will not already be contained in this handler
         */
        void addTriggerNow_(ManagedTrigger* trig) {
            sparta_assert(false == in_tick_,
                              "ClockHandler addTriggerNow_ called but ClockHandler was "
                              "currently within a tick");

            // Ensure it is not already present
            auto itr = std::find(triggers_.begin(), triggers_.end(), trig);
            sparta_assert(itr == triggers_.end(),
                              "Cannot add trigger " << trig << " to a ClockHandler because it is "
                              " already present");

            triggers_.push_back(trig);
        }

        /*!
         * \brief Remove a trigger at the end of a tick
         */
        void removeTriggerDefferred_(const ManagedTrigger* trig) {
            sparta_assert(in_tick_,
                              "ClockHandler removeTriggerDefferred_ called but ClockHandler was not "
                              "currently within a tick");

            to_remove_.push_back(trig);
        }

        /*!
         * \brief Remove a trigger immediately
         * \param trig Trigger to remove. Has no effect if not found
         */
        void removeTriggerNow_(const ManagedTrigger* trig) {
            sparta_assert(false == in_tick_,
                              "ClockHandler removeTriggerNow_ called but ClockHandler was "
                              "currently within a tick");

            auto itr = std::find(triggers_.begin(), triggers_.end(), trig);
            if(itr != triggers_.end()){
                triggers_.erase(itr);
            }
        }

        /*!
         * \brief Handle all deferred removals
         */
        void handleDeferredRemovals_() {
            for(auto tr : to_remove_){
                removeTriggerNow_(tr);
            }

            to_remove_.clear();
        }

        /*!
         * \brief Handle all deferred additions
         */
        void handleDeferredAdditions_() {
            for(auto tr : to_add_){
                addTriggerNow_(tr);
            }

            to_add_.clear();
        }

        /*!
         * \brief Tick event from scheduler. Indicates a clock edge
         */
        void clockTick_() {
            // Toggle in_tick_ and handle deferred removals & additions at end
            // of this function
            TickLock tl(*this);

            for(auto trig : triggers_){
                trig->check();
            }

            // Schedule for next cycle on this event's clock
            event_.schedule(1, clock_);
        }


        /*!
         * \brief Clock being observed
         */
        const Clock* clock_;

        /*!
         * \brief Callback for clock ticks
         */
        TriggerEvent event_;

        /*!
         * \brief Triggers beign checked by this ClockHandler
         */
        std::vector<ManagedTrigger*> triggers_;

        /*!
         * \brief Triggers removed from this ClockHandler during its callbacks
         */
        std::vector<const ManagedTrigger*> to_remove_;

        /*!
         * \brief Triggers being added to this ClockHandler during its callbacks
         */
        std::vector<ManagedTrigger*> to_add_;

        /*!
         * \brief Currently within a tick handler
         */
        bool in_tick_;

    }; // class ClockHandler


    /*!
     * \brief Singleton instance
     */
    static TriggerManager trig_man_singleton_;

    /*!
     * \brief Has this been constructed? If so, must match INITIALIZED_TOKEN.
     * This is to ensure that the class is not accessed before it is statically
     * initialized
     */
    uint32_t constructed_token_;

    /*!
     * \brief Map of ClockHandler objects.
     */
    std::map<const Clock*, std::unique_ptr<ClockHandler>> clocks_;


}; // class TriggerManager

    } // namespace trigger
} // namespace sparta

#endif
