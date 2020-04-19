// <Audience> -*- C++ -*-


#pragma once

#include <string>
#include <list>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{
    /**
     * \class Audience
     * \brief Class used maintain a list of sparta::Scheduleable objects; used by observation-type classes
     *
     */
    class Audience
    {
        typedef std::list<ScheduleableHandle> ScheduleableHandleList;

    public:

        void setName(const std::string & name) {
            name_ = name;
        }

        void release()
        {
            for (auto &ri : registry_)
            {
                ri->cancel();
            }
            registry_.clear();
        }

        const std::string& getName() const
        {
            return name_;
        }

        void enroll(ScheduleableHandle ev_hand)
        {
            registry_.push_back(ev_hand);
        }

        void enroll(ScheduleableHandle ev_hand, const Clock::Cycle& delay)
        {
            ev_hand->setDelay(delay);
            enroll(ev_hand);
        }

        void withdraw(ScheduleableHandle ev_hand)
        {
            auto ei = std::find(registry_.begin(), registry_.end(), ev_hand);
            if (ei != registry_.end()) {
                registry_.erase(ei);
            }
        }

        void notify()
        {
            for (auto &ri : registry_)
            {
                ri->schedule();
            }
            registry_.clear();
        }

        void delayedNotify(const Clock::Cycle& delay)
        {
            for (auto &ri : registry_)
            {
                ri->addDelay(delay);
                ri->schedule();
            }
            registry_.clear();
        }

    protected:
        std::string             name_;
        ScheduleableHandleList         registry_;
    };
}
