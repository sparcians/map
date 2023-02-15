// <Clock> -*- C++ -*-

#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

#include <utility>

#include "simdb/ObjectManager.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/ObjectRef.hpp"

namespace sparta {
    Clock::Clock(const std::string& name, Scheduler *scheduler) :
        TreeNode(name, "Clock"),
        scheduler_(scheduler)
    {
        sparta_assert(scheduler_ != nullptr);
        scheduler_->registerClock(this);
    }

    Clock::Clock(RootTreeNode* parent_root, const std::string& name, Scheduler *scheduler) :
        Clock(name, scheduler)
    {
        if(parent_root){
            parent_root->addChild(this);
        }
    }

    Clock::Clock(const std::string& name, const Handle& parent_clk,
                 const uint32_t& p_rat, const uint32_t& c_rat) :
        Clock(name, parent_clk->getScheduler())
    {
        sparta_assert(parent_clk != nullptr);
        parent_ = parent_clk;

        associate(parent_);
        setRatio(p_rat, c_rat);
    }

    /*!
     * \brief Construct with a frequency
     * \note Inherits scheduler pointer from parent clock
     */
    Clock::Clock(const std::string& name, const Handle& parent, double frequency_mhz) :
        TreeNode(name, "Clock"),
        parent_(parent),
        scheduler_(parent_->getScheduler()),
        frequency_mhz_(frequency_mhz)
    {
        sparta_assert(frequency_mhz != 0);
        associate(parent);
        setRatio(1, 1); // Must be a valid ratio or Rational.h will assert

        sparta_assert(scheduler_ != nullptr);
        scheduler_->registerClock(this);
    }

    //! \brief Destroy this Clock (deregisters from the Scheduler)
    Clock::~Clock() {
        // Deregister from the scheduler
        scheduler_->deregisterClock(this);
    }

    void Clock::associate(const Handle& parent)
    {
        sparta_assert(parent_ == nullptr || parent_ == parent,
                      "Cannot associate a clock with a new parent once it already has a parent");
        parent_ = parent;
        parent->addChild(this); // TreeNode::addChild
        parent->children_.push_back(this);
    }

    void Clock::setRatio(const uint32_t& p_rat, const uint32_t& c_rat)
    {
        parent_ratio_ = utils::Rational<uint32_t>(p_rat, c_rat);
        root_ratio_   = parent_ratio_.inv();
        period_       = 1;
        normalized_   = false;
    }

    //! Persist clock hierarchy in the provided database,
    //! treating 'this' sparta::Clock as the hierarchy root.
    simdb::DatabaseID Clock::serializeTo(const simdb::ObjectManager & sim_db) const
    {
        auto clock_tbl = sim_db.getTable("ClockHierarchy");
        if (!clock_tbl) {
            return 0;
        }

        std::map<const Clock*, simdb::DatabaseID> db_ids;

        sim_db.safeTransaction([&]() {
                                   recursSerializeToTable_(*clock_tbl, 0, db_ids);
                               });

        auto iter = db_ids.find(this);
        sparta_assert(iter != db_ids.end());
        return iter->second;
    }

    //! Persist clock hierarchy in the provided database,
    //! recursing down through child nodes as necessary.
    void Clock::recursSerializeToTable_(
        simdb::TableRef & clock_tbl,
        const simdb::DatabaseID parent_clk_id,
        std::map<const Clock*, simdb::DatabaseID> & db_ids) const
    {
        auto row = clock_tbl.createObjectWithArgs(
            "ParentClockID", parent_clk_id,
            "Name", getName(),
            "Period", getPeriod(),
            "FreqMHz", frequency_mhz_,
            "RatioToParent", static_cast<double>(getRatio()));

        for (const auto child : children_) {
            child->recursSerializeToTable_(clock_tbl, row->getId(), db_ids);
        }

        db_ids[this] = row->getId();
    }

}
