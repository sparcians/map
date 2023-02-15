// <Clock> -*- C++ -*-


#pragma once

/**
 * \file   Clock.hpp
 *
 * \brief File that defines the Clock class
 */

#include <inttypes.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <list>
#include <map>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/utils/Rational.hpp"
#include "sparta/utils/MathUtils.hpp"
#include "simdb_fwd.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticSet.hpp"

namespace simdb {
    class TableRef;
    class ObjectManager;
}  // namespace simdb

namespace sparta
{
        /**
      * \class Clock
      * \brief A representation of simulated time.
      *
      * The clock is the mechanism in which resources provide timed
      * observations and proper scheduling based on time domains.
      * Nothing in simulation should ever use the scheduler directly --
      * it should always go through a Clock.
      */
    class Clock : public TreeNode
    {
    public:
        typedef std::shared_ptr<Clock>   Handle;
        typedef uint32_t                 Period;
        typedef uint64_t                 Cycle;
        typedef double                   Frequency;

    private:
        static bool                      normalized_;
        typedef std::list<Clock*>        RefList;

    public:

        /**
         * \brief Construct a clock
         * \param name The name of the clock
         * \param scheduler The scheduler to use; must be provided
         */
        Clock(const std::string& name, Scheduler *scheduler);

        /**
         * \brief Construct a named clock with a RootTreeNode as its
         *        parent.  This will effeclivelly allow this tree (and
         *        its counters, parameters, notifications, etc.) to be
         *        accessed by sparta clients.
         * \param parent_root RootTreeNode parent of this clock. If nullptr, has
         *                    no effect
         * \param name Name of this clock
         * \param scheduler The scheduler to use; must be provided
         */
        Clock(RootTreeNode* parent_root, const std::string& name, Scheduler *scheduler);

        /*!
         * \brief Construct a named clock with a clock parent and a clock
         *        relative to that parent
         * \param name Name of this clock
         * \param scheduler Pointer to the scheduler to use
         * \param parent_clk Parent clock relative to this clock
         * \param p_rat Numerator to the ratio
         * \param c_rat Denominator to the ratio
         *
         * \note Inherits scheduler pointer from parent clock
         */
        Clock(const std::string& name, const Handle& parent_clk,
              const uint32_t& p_rat = 1, const uint32_t& c_rat = 1);

        /*!
         * \brief Construct with a frequency
         * \note Inherits scheduler pointer from parent clock
         */
        Clock(const std::string& name, const Handle& parent, double frequency_mhz);

        //! \brief Destroy this Clock (deregisters from the Scheduler)
        ~Clock();

        //! \brief Associate this clock with another clock
        void associate(const Handle& parent);

        /*!
          & \brief Set the ratio of the clock
         */
        void setRatio(const uint32_t& p_rat = 1, const uint32_t& c_rat = 1);

        //! Get the clock frequency
        double getFrequencyMhz() const {
            return frequency_mhz_;
        }

        //! Get the clock ratio
        utils::Rational<uint32_t> getRatio() const
        {
            return parent_ratio_;
        }

        //! Calculate the norm
        uint32_t calcNorm(uint32_t partial_norm = 1)
        {
            if (parent_ != nullptr) {
                root_ratio_ = parent_ratio_.inv() * parent_->root_ratio_;
            }

            partial_norm = utils::lcm(partial_norm, root_ratio_.getDenominator());
            for (auto i = children_.begin(); i != children_.end(); ++i) {
                partial_norm = utils::lcm(partial_norm, (*i)->calcNorm(partial_norm));
            }
            return partial_norm;
        }

        /*!
         * \brief Set the period of this clock
         * \pre Clock must not be finalized (see isFinalized)
         * \warning Setting this can cause period to disagree with frequency or
         * ratio. Only a ClockManager should use this in most cases
         */
        void setPeriod(uint32_t norm)
        {
            sparta_assert(!isFinalized(),
                              "Should not be setting period on a sparta::Clock after device tree finalization");
            period_ = uint32_t(root_ratio_ * norm);
        }

        /*!
         * \brief Returns the period of this clock in scheduler ticks
         * \pre Clock should be normalized or have its period explicitly set
         */
        Period getPeriod() const
        {
            return period_;
        }

        /**
         * \brief Given the tick, convert to a Clock::Cycle
         * \param tick The tick to convert
         * \return The converted tick to this Clock's cycle
         */
        Cycle getCycle(const Scheduler::Tick& tick) const
        {
            return (tick / period_);
        }

        /**
         * \brief Get the current cycle (uses current tick from the
         *        Scheduler)
         * \return The current cycle based on current tick
         */
        Cycle currentCycle() const
        {
            return getCycle(scheduler_->getCurrentTick());
        }

        /**
         * \brief Get the current scheduler tick
         * \return The current tick
         */
        Scheduler::Tick currentTick() const
        {
            return scheduler_->getCurrentTick();
        }

        /**
         * \brief Update the elapsed_cycles internal value given the
         * number of ticks
         * \param elapsed_ticks The number of ticks elapsed
         */
        void updateElapsedCycles(const Scheduler::Tick elapsed_ticks)
        {
            elapsed_cycles_ = getCycle(elapsed_ticks);
        }

        /**
         * \brief Return the total elapsed cycles from this Clocks POV
         * \return The elapsed cycles
         */
        Cycle elapsedCycles() const
        {
            return elapsed_cycles_;
        }

        /**
         * \brief Return the tick corresponding to the given cycle
         * \param cycle The cycle to convert
         * \return The tick from the given cycle
         */
        Scheduler::Tick getTick(const Cycle& cycle) const
        {
            return cycle * period_;
        }

        /**
         * \brief Return the tick corresponding to the given cycle
         * \param cycle The (fractional) cycle to convert
         * \return The tick from the given cycle
         */
        Scheduler::Tick getTick(const double& cycle) const
        {
            return static_cast<Scheduler::Tick>(cycle * static_cast<double>(period_));
        }

        /**
         * \brief Convert the given absolute cycle number into the corresponding absolute tick number
         * \param cycle The cycle to convert
         * \return The tick corresponding to the given cycle
         */
        Scheduler::Tick getAbsoluteTick(const Cycle& abs_cycle) const
        {
            return ((abs_cycle - 1) * period_);
        }

        /**
         * \brief Return true if the current tick aligns with a
         *        positive edge of this Clock
         * \return true if "rising edge"; false if between rising edges
         *
         * This method is typically used for sanity checking between
         * SyncPorts.
         */
        bool isPosedge() const
        {
            return ((scheduler_->getCurrentTick() % period_) == 0);
        }

        //! Used for printing the clock information
        explicit operator std::string() const
        {
            std::stringstream ss;
            ss << "<Clock " << getName()
               << " period=" << period_;
            if (frequency_mhz_ != 0.0) {
                ss << " freq=" << frequency_mhz_;
            }
            if (parent_ != nullptr) {
                ss << " (" << std::string(parent_ratio_)
                   << " to " << parent_->getName() << ")";
            }
            ss << ">";
            return ss.str();
        }

        //! Used for printing the clock information
        void print(std::ostream& os) const
        {
            os << "Clock(" << getName() << "):" << std::endl;
            if (parent_ != nullptr) {
                os << "\tRatio to Clock(" << parent_->getName() << "): "
                   << parent_ratio_ << std::endl;
            } else {
                os << "\tROOT Clock" << std::endl;
            }
            os << "\tRatio to ROOT: " << root_ratio_ << std::endl
               << "\tPeriod: " << period_ << std::endl;
            if (frequency_mhz_ != 0.0) {
                os << "\tFrequency: " << frequency_mhz_ << std::endl;
            }
            os << std::endl;
        }

        //! Persist clock hierarchy in the provided database,
        //! treating 'this' sparta::Clock as the hierarchy root.
        //! Returns the database ID of the clock node that was
        //! put into this database.
        simdb::DatabaseID serializeTo(const simdb::ObjectManager & sim_db) const;

        // Overload of TreeNode::stringize
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            return (std::string)*this;
        }

        /**
         * \return The scheduler associated with this clock. Will not be nullptr
         */
        Scheduler* getScheduler() const
        {
            sparta_assert(scheduler_); // should be guaranteed by constructor
            return scheduler_;
        }

        //! \name Instrumentation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns a counter holding the cycle count of this clock
         */
        ReadOnlyCounter& getCyclesROCounter() {
            return cycles_roctr_;
        }

        /*!
         * \brief Returns a counter holding the cycle count of this clock
         */
        const ReadOnlyCounter& getCyclesROCounter() const {
            return cycles_roctr_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:
        Handle                    parent_;              //!< Parent clock (NULL if root)
        Scheduler           *     scheduler_ = nullptr; //!< Scheduler on which this clock operates
        RefList                   children_;            //!< Child clocks
        utils::Rational<uint32_t> parent_ratio_  = 1;   //!< For debugging
        utils::Rational<uint32_t> root_ratio_    = 1;
        Period                    period_        = 1;
        StatisticSet              sset_ = {this};
        const double              frequency_mhz_ = 0.0;
        Cycle                     elapsed_cycles_ = 0;

        class CurrentCycleCounter : public ReadOnlyCounter {
            Clock& clk_;
        public:
            CurrentCycleCounter(Clock& clk, StatisticSet* parent) :
                ReadOnlyCounter(parent,
                                "cycles",
                                "Cycle Count of this Clock",
                                Counter::COUNT_NORMAL),
                clk_(clk)
            {
                // Needed for time calculation down the road in
                // StatisticInstance
                this->setClock(&clk_);
            }

            counter_type get() const override {
                return clk_.currentCycle();
            }
        } cycles_roctr_ = {*this, &sset_};

        // Persist clock hierarchy in the provided database,
        // recursing down through child nodes as necessary.
        // Output argument 'db_ids' tracks database record
        // ID's for each sparta::Clock that was written to
        // the clock hierarchy table. The map keys are
        // sparta::Clock 'this' pointers.
        void recursSerializeToTable_(
            simdb::TableRef & clock_tbl,
            const simdb::DatabaseID parent_clk_id,
            std::map<const Clock*, simdb::DatabaseID> & db_ids) const;
    };

    inline std::ostream& operator<<(std::ostream& os, const sparta::Clock& clk)
    {
        clk.print(os);
        return os;
    }
}  // namespace sparta


namespace sparta
{

/*
 * Return the delay, in ticks, incurred when crossing a clock boundary
 *
 * \param src_delay The sender's delay (in ticks) to schedule from "now"
 * \param src_clk   The sender's clock
 * \param dst_delay The receiver's delay (in ticks) to schedule from "now"
 * \param dst_clk   The receiver's clock
 * \note both clocks must be on the same scheduler. See
 * sparta::Clock::getScheduler
 *
 * \return Relative scheduler tick that the crossing would incur
 */
inline Scheduler::Tick calculateClockCrossingDelay(Scheduler::Tick src_delay, const Clock * src_clk,
                                                   Scheduler::Tick dst_delay, const Clock * dst_clk)
{
    sparta_assert(src_clk, "calculateClockCrossingDelay requires a non-null src_clk");
    sparta_assert(dst_clk, "calculateClockCrossingDelay requires a non-null dst_clk");
    sparta_assert(src_clk->getScheduler() == dst_clk->getScheduler(),
                      "calculateClockCrossingDelay requires src_clk and dst_clk to operate on "
                      "the same scheduler. src = " << src_clk->getScheduler() << " and dst = "
                      << dst_clk->getScheduler());
    auto scheduler = src_clk->getScheduler();
    sparta_assert(scheduler,
                      "calculateClockCrossingDelay requires src_clk (" << *src_clk << ") to "
                      "have a non-null scheduler");

    sparta::Scheduler::Tick current_tick = scheduler->getCurrentTick();
    sparta::Scheduler::Tick num_delay_ticks = 0;

    // 1. Snap to source positive edge (assert match)
    // 2. Add source delay in source clock
    // 3. Add destination delay in source clock
    // 4. Snap to destination positive edge

    // Note: A snap to destination positive edge between source delay and
    // destination delay is not required. If dst_delay is integer, it has
    // no effect. If dst_delay is float, it acts as implicit round up.
    // Similarly, a snap to source positive edge would have the same effect
    // of being benign or rounding up the src_delay.

    sparta::Scheduler::Tick last_src_clk_posedge_tick =
        current_tick / src_clk->getPeriod() * src_clk->getPeriod();
    sparta_assert(last_src_clk_posedge_tick == current_tick);

    num_delay_ticks += src_delay + dst_delay;

    sparta::Scheduler::Tick raw_event_arrival_tick = (current_tick + num_delay_ticks);
    sparta::Scheduler::Tick raw_dst_clk_posedge_tick =
        raw_event_arrival_tick / dst_clk->getPeriod() * dst_clk->getPeriod();
    if (raw_event_arrival_tick != raw_dst_clk_posedge_tick) {
        sparta_assert(raw_event_arrival_tick > raw_dst_clk_posedge_tick);
        num_delay_ticks += dst_clk->getPeriod() - (raw_event_arrival_tick - raw_dst_clk_posedge_tick);
    }

    return num_delay_ticks;
}

/*
 * Return the delay, in ticks, incurred when crossing a clock boundary in
 * the reverse direction
 *
 * \param dst_arrival_tick The tick in the future on which the receiver receives the data
 * \param src_delay The sender's delay (in ticks) to schedule from "now"
 * \param src_clk   The sender's clock
 * \param dst_delay The receiver's delay (in ticks) to schedule from "now"
 * \param dst_clk   The receiver's clock
 * \note both clocks must be on the same scheduler. See
 * sparta::Clock::getScheduler
 *
 * \return Relative scheduler tick that the reverse crossing would incur
 */
inline Scheduler::Tick calculateReverseClockCrossingDelay(Scheduler::Tick dst_arrival_tick,
                                                          Scheduler::Tick src_delay, const Clock * src_clk,
                                                          Scheduler::Tick dst_delay, const Clock * dst_clk)
{
    sparta_assert(src_clk, "calculateReverseClockCrossingDelay requires a non-null src_clk");
    sparta_assert(dst_clk, "calculateReverseClockCrossingDelay requires a non-null dst_clk");
    sparta_assert(src_clk->getScheduler() == dst_clk->getScheduler(),
                      "calculateReverseClockCrossingDelay requires src_clk and dst_clk to operate on "
                      "the same scheduler. src = " << src_clk->getScheduler() << " and dst = "
                      << dst_clk->getScheduler());
    auto scheduler = src_clk->getScheduler();
    sparta_assert(scheduler,
                      "calculateReverseClockCrossingDelay requires src_clk (" << *src_clk << ") to "
                      "have a non-null scheduler");

    sparta::Scheduler::Tick relative_ticks_before_arrival = 0;

    // 1. Snap to destination positive edge (assert match)
    sparta::Scheduler::Tick raw_dst_clk_posedge_tick =
        dst_arrival_tick / dst_clk->getPeriod() * dst_clk->getPeriod();
    sparta_assert(dst_arrival_tick == raw_dst_clk_posedge_tick);

    // 2. Subtract source delay in source clock
    // 3. Subtract destination delay in source clock
    relative_ticks_before_arrival += src_delay + dst_delay;
    sparta::Scheduler::Tick raw_event_sent_tick = (dst_arrival_tick - relative_ticks_before_arrival);

    // 4. Snap to previous source positive edge
    sparta::Scheduler::Tick raw_src_clk_posedge_tick =
        raw_event_sent_tick / src_clk->getPeriod() * src_clk->getPeriod();

    if (raw_event_sent_tick != raw_src_clk_posedge_tick) {
        sparta_assert(raw_event_sent_tick > raw_src_clk_posedge_tick);
        relative_ticks_before_arrival += (raw_event_sent_tick - raw_src_clk_posedge_tick);
    }

    return relative_ticks_before_arrival;
}


}  // namespace sparta


#define SPARTA_CLOCK_BODY                         \
    namespace sparta {                            \
        bool Clock::normalized_ = false;        \
    }
