// <Clock> -*- C++ -*-

#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

#include <utility>

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
}
