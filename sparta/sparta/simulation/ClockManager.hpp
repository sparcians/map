// <ClockManager> -*- C++ -*-


/*!
 * \file ClockManager.hpp
 * \brief Manages building a clock tree
 */

#pragma once

#include <cstdint>
#include <list>
#include <iostream>
#include <memory>
#include <string>

#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
class RootTreeNode;

    /**
     * \class ClockManager
     * \brief Manages building a clock tree
     */
    class ClockManager
    {
        typedef std::list<Clock::Handle>  ClockList;

    public:

        ClockManager(Scheduler * scheduler) :
            any_clock_with_explicit_freq_(false),
            scheduler_(scheduler)
        { }

        ~ClockManager()
        {
            if(croot_) {
                croot_->enterTeardown_(); // Allow deletion of Clock nodes in dtor
            }
        }

        /*!
         * \brief Construct a root clock
         * \param parent A root object representing the top of the clock tree.
         * Because Clock is not a RootTreeNode, it must be attached to
         * existing tree
         * \param name Name of the root clock
         * \pre Must not have already called makeRoot
         * \note Clock must be part of a tree to allow sparta clients access to
         * its instrumentation.
         * \todo Implement special case for RootClock that avoids this extra
         * level
         */
        Clock::Handle makeRoot(RootTreeNode* parent = nullptr, const std::string& name = "Root")
        {
            sparta_assert(croot_.get() == nullptr,
                              "Cannot makeRoot on a ClockManager which already has a root");
            croot_ = Clock::Handle(new Clock(parent, name, scheduler_));
            clist_.push_back(croot_);
            return croot_;
        }

        Clock::Handle getRoot() const
        {
            return croot_;
        }

        /*!
         * \brief Create a new clock with a given ratio to a parent clock
         */
        Clock::Handle makeClock(const std::string& name, const Clock::Handle &parent,
                                const uint32_t &p_rat, const uint32_t &c_rat)
        {
            Clock::Handle c = Clock::Handle(new Clock(name, parent, p_rat, c_rat));
            clist_.push_back(c);
            return c;
        }

        /*!
         * \brief Create a new clock with a 1:1 ratio to a parent clock
         */
        Clock::Handle makeClock(const std::string& name, const Clock::Handle &parent)
        {
            return makeClock(name, parent, 1, 1);
        }

        /*!
         * \brief Create a new clock with a given frequency
         */
        Clock::Handle makeClock(const std::string & name,
                                const Clock::Handle &parent, double frequency_mhz)
        {
            any_clock_with_explicit_freq_ = true;
            Clock::Handle c = Clock::Handle(new Clock(name, parent, frequency_mhz));
            clist_.push_back(c);
            return c;
        }

        uint32_t normalize()
        {
            // Special case when we're dealing with clocks that have an
            // explicit frequency
            if (any_clock_with_explicit_freq_) {
                normalize_frequencies_();
                return uint32_t(1);
            }

            // Calculate the normalization factor
            uint32_t norm_ = croot_->calcNorm();

            // Set the clock periods, based on the normalization factor
            for (auto i = clist_.begin(); i != clist_.end(); ++i) {
                (*i)->setPeriod(norm_);
            }

            // Skip through all TreeNode phases to finalized
            croot_->enterConfig_();
            croot_->enterFinalizing_();
            croot_->finalizeTree_();
            croot_->enterFinalized_();

            return norm_;
        }

        void print(std::ostream &os) const
        {
            for (auto i = clist_.begin(); i != clist_.end(); ++i) {
                (*i)->print(os);
            }
        }

        /*!
         * Return the clock period given a frequency.
         *
         * \param frequency_mhz - Frequency in MHz to find the period for
         *
         * Ideally this is the only location that a timebase is used, but I
         * don't know if that's reality throught sparta.
         */
        static uint64_t getClockPeriodFromFrequencyMhz(double frequency_mhz) {
            return static_cast<uint64_t>(((1.0 / frequency_mhz) * 1000. * 1000.0));
        }

    private:

        // Normalize the frequencies for clocks when the frequencies were
        // set explicitly.
        void normalize_frequencies_() {

            for (auto i = clist_.begin(); i != clist_.end(); ++i) {
                double frequency_mhz = (*i)->getFrequencyMhz();
                sparta_assert(frequency_mhz > 0.0 || i == clist_.begin());
                if (frequency_mhz == 0.0) {
                    continue;
                }

                uint64_t period = getClockPeriodFromFrequencyMhz(frequency_mhz);
                (*i)->setPeriod(period);
            }
        }

        Clock::Handle   croot_;
        ClockList       clist_;
        bool            any_clock_with_explicit_freq_;
        Scheduler*      scheduler_ = nullptr;
    }; // class ClockManager

    std::ostream &operator<<(std::ostream &os, const ClockManager &m);
};
