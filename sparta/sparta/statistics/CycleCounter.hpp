// <Counter> -*- C++ -*-


#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/ByteOrder.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/Clock.hpp"


namespace sparta
{
    /*!
     * \brief Represents a cycle counter
     * \note CycleCounters are completely passive and not checkointable
     * \note This is not a subclass because virtual set/increment methods
     * introduce much overhead in Counters.
     *
     * The purpose of this Counter is to start a count at particular
     * point (with a call to startCounting()) and close it at another
     * point (with a call to stopCounting()).  By default, the counter
     * is \b not started.  This type of Counter is used for
     * utilization counts where it's useful to start counting when a
     * threshold is hit and a record of \i how \i long it was at that
     * threshold.
     */
    class CycleCounter : public CounterBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief CycleCounter constructor
         * \param parent parent node. Must not be nullptr.
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param group Group of this counter. Must be a valid TreeNode group
         *              when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         *                  when paired with group.
         * \param desc Description of this node. Required to be a valid
         *             TreeNode description.
         * \param behave Behavior of this counter. This is not
         *               enforced for CycleCounter but used as a hint
         *               for the sparta report and statistics
         *               infrastructure
         * \param visibility Visibility level of this counter
         */
        CycleCounter(TreeNode* parent,
                     const std::string& name,
                     const std::string& group,
                     TreeNode::group_idx_type group_idx,
                     const std::string& desc,
                     CounterBehavior behave,
                     const sparta::Clock * clk,
                     visibility_t visibility) :
            CounterBase(parent,
                        name,
                        group,
                        group_idx,
                        desc,
                        behave,
                        visibility),
            clk_(clk)
        {
            sparta_assert(clk_ != nullptr, "CycleCounter must be given a clock through it's parent node");
        }

        // Alternate constructor
        CycleCounter(TreeNode* parent,
                     const std::string& name,
                     const std::string& desc,
                     CounterBehavior behave,
                     const sparta::Clock * clk,
                     visibility_t visibility) :
            CycleCounter(parent,
                         name,
                         TreeNode::GROUP_NAME_NONE,
                         TreeNode::GROUP_IDX_NONE,
                         desc,
                         behave,
                         clk,
                         visibility)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate Constructor
        CycleCounter(TreeNode* parent,
                     const std::string& name,
                     const std::string& group,
                     TreeNode::group_idx_type group_idx,
                     const std::string& desc,
                     CounterBehavior behave,
                     const sparta::Clock * clk) :
            CycleCounter(parent,
                         name,
                         group,
                         group_idx,
                         desc,
                         behave,
                         clk,
                         DEFAULT_VISIBILITY)
        {
            sparta_assert(clk_ != nullptr, "CycleCounter must be given a clock through it's parent node");
        }

        // Alternate constructor
        CycleCounter(TreeNode* parent,
                     const std::string& name,
                     const std::string& desc,
                     CounterBehavior behave,
                     const sparta::Clock * clk) :
            CycleCounter(parent,
                         name,
                         TreeNode::GROUP_NAME_NONE,
                         TreeNode::GROUP_IDX_NONE,
                         desc,
                         behave,
                         clk,
                         DEFAULT_VISIBILITY)
        {
            // Initialization handled in delegated constructor
        }


        /*!
         * \brief Move constructor
         * \pre See CounterBase move constructor
         */
        CycleCounter(CycleCounter&& rhp) :
            CounterBase(std::move(rhp)),
            clk_(rhp.clk_),
            count_(0),
            start_count_(0),
            counting_(false)
        {
            TreeNode* parent = rhp.getParent();
            if(parent != nullptr){
                parent->addChild(this);
            }
        }

        /*!
         * \brief Destructor
         * \note CycleCounter is not intended to be overloaded
         */
        ~CycleCounter() {}

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name CycleCounter use methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Start counting, taking into account the specified delay
         * \param delay Begin incrementing counter after this number of cycles
         * has elapsed on the clock associated with this Counter (see
         * sparta::CounterBase::getClock)
         * \pre Must not be counting already (see stopCounting)
         */
        void startCounting(uint32_t delay = 0) {
            sparta_assert(counting_ == false);
            mult_ = 1;
            start_count_ = clk_->elapsedCycles() + delay;
            counting_ = true;
        }

        /*!
         * \brief Start counting, taking into account the specified delay
         * \param delay Begin incrementing counter after this number of cycles
         * has elapsed on the clock associated with this Counter (see
         * sparta::CounterBase::getClock)
         * \param add_per_cycle Amount to add to the counter each cycle. This
         * is generally used when this counter is constructed with a behavior of
         * COUNT_INTEGRAL counter. Then the counter is incremented by some
         * value every cycle to effectively take the integral of some value over time.
         * \pre Must not be counting already (see stopCounting)
         */
        void startCountingWithMultiplier(uint32_t add_per_cycle, uint32_t delay = 0) {
            sparta_assert(counting_ == false);
            mult_ = add_per_cycle;
            start_count_ = clk_->elapsedCycles() + delay;
            counting_ = true;
        }


        /*!
         *  Update the current multiplier used for counting without
         *  requiring a stop/start of the counter.
         */
        void updateCountingMultiplier(uint32_t add_per_cycle) {
            if (isCounting()) {
                stopCounting();
            }
            startCountingWithMultiplier(add_per_cycle);
        }

        //! Stop counting and increment internal count, taking into account the specified delay
        void stopCounting(uint32_t delay = 0) {
            sparta_assert(counting_ == true);
            sparta_assert ((clk_->elapsedCycles() + delay) >= start_count_);
            count_ += (clk_->elapsedCycles() + delay - start_count_) * mult_;
            counting_ = false;
        }

        //! Return whether this counter is counting or not.
        bool isCounting() const {
            return counting_;
        }

        //! Return the current multipler
        uint32_t getCurrentMultiplier() const {
            return mult_;
        }


        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Access Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the value of this counter
         * \return Current value of this counter
         * \note Must be overridden if this class is constructed with
         * ref=nullptr
         * \todo Allow indexed accesses if larger counters are supported.
         */
        virtual counter_type get() const override {
            counter_type count = count_;
            if(counting_) {
                Clock::Cycle elapsed = clk_->elapsedCycles();
                if (elapsed > start_count_) {
                    count += (elapsed - start_count_) * mult_;
                }
            }
            return count;
        }
        /*!
         * \brief Cast operator to get value of the counter
         */
        operator counter_type() const {
            return get();
        }

        /*!
         * \brief Comparison against another counter
         */
        bool operator==(const Counter& rhp) const {
            return get() == rhp.get();
        }

        /*!
         * \brief Comparison against another counter
         */
        bool operator==(const CycleCounter& rhp) const {
            return get() == rhp.get();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! CycleCounters track integral values, and are good
        //! candidates for compression
        virtual bool supportsCompression() const override {
            return true;
        }

        //! \name Printing Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << " val:" << std::dec;
            ss << get() << ' ';
            ss << Counter::getBehaviorName(getBehavior());
            ss << " vis:" << getVisibility() << '>';
            return ss.str();
        }

    protected:

        /*!
         * \brief React to child registration
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override {
            (void) child;            throw SpartaException("Cannot add children to a CycleCounter");
        }

    private:

        //! Clock this counter uses for differences
        const sparta::Clock * clk_ = nullptr;

        //! Multiplier (amount added for each cycle)
        uint32_t mult_ = 1;

        //! Counters from which value will be read/calculated
        counter_type count_ = 0;
        counter_type start_count_ = 0;

        //! Counter on or off?
        bool counting_ = false;

    }; // class CycleCounter

} // namespace sparta
