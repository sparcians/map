// <Counter> -*- C++ -*-


#ifndef __COUNTER_H__
#define __COUNTER_H__

#include <iostream>
#include <sstream>
#include <limits>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/CounterBase.hpp"


namespace sparta
{
    /*!
     * \brief Represents a counter of type counter_type (uint64_t).
     * 2 and greater than 0 with a ceiling specified.
     * A ceiling value that is a power of two is optimized to
     * perform operations faster.
     * \note Counter write performance is critical, so this should not be
     * subclassed because alost all reasons for subclassing would involve
     * changing the set/increment/operator= methods.
     */
    class Counter final : public CounterBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Counter constructor
         * \param parent parent node. Must not be nullptr
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param group Group of this counter. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param behave Behavior of this counter. This is partly enforced.
         * Counters with COUNT_LATEST behavior can be set and incremented.
         * Other counter behaviors can only be incremented.
         * \param visibility InstrumentationNode visibility level
         * \post value is initalized to 0
         */
        Counter(TreeNode* parent,
                const std::string& name,
                const std::string& group,
                TreeNode::group_idx_type group_idx,
                const std::string& desc,
                CounterBehavior behave,
                visibility_t visibility) :
            CounterBase(parent,
                        name,
                        group,
                        group_idx,
                        desc,
                        behave,
                        visibility),
            val_(0)
        { }

        //! Alternate constructor
        Counter(TreeNode* parent,
                const std::string& name,
                const std::string& group,
                TreeNode::group_idx_type group_idx,
                const std::string& desc,
                CounterBehavior behave) :
            Counter(parent,
                    name,
                    group,
                    group_idx,
                    desc,
                    behave,
                    CounterBase::DEFAULT_VISIBILITY)
        {
            // Initialization handled in constructor delegation
        }

        //! Alternate constructor
        Counter(TreeNode* parent,
                const std::string& name,
                const std::string& desc,
                CounterBehavior behave,
                visibility_t visibility) :
            Counter(parent,
                    name,
                    TreeNode::GROUP_NAME_NONE,
                    TreeNode::GROUP_IDX_NONE,
                    desc,
                    behave,
                    visibility)
        {
            // Initialization handled in constructor delegation
        }

        //! Alternate constructor
        Counter(TreeNode* parent,
                const std::string& name,
                const std::string& desc,
                CounterBehavior behave) :
            Counter(parent,
                    name,
                    TreeNode::GROUP_NAME_NONE,
                    TreeNode::GROUP_IDX_NONE,
                    desc,
                    behave)
        {
            // Initialization handled in constructor delegation
        }

        /*!
         * \brief Copy construction not allowed
         */
        Counter(const Counter& rhp) = delete;

        /*!
         * \brief Move constructor
         * \pre See CounterBase move constructor
         */
        Counter(Counter&& rhp) :
            CounterBase(std::move(rhp)),
            val_(rhp.val_)
        {
            TreeNode* parent = rhp.getParent();
            if(parent != nullptr){
                parent->addChild(this);
            }
        }

        /*!
         * \brief Destructor
         * \note Counter is not intended to be overloaded
         */
        ~Counter() {}

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Access Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Sets a new value
         * \param val Value to write
         * \note This is only allowed if behavior is COUNT_LATEST (see getBehavior)
         * \throw SpartaException if the behavior prohibits this counter from being written directly.
         * \todo Allow indexed accesses if larger counters are supported
         *
         * Prohibiting direct write for COUNT_NORMAL and COUNT_INTEGRAL
         * behaviors prevents the counter from becoming smaller and discourages
         * clients from caching the value
         */
        counter_type set(counter_type val) {
            if(__builtin_expect(getBehavior() != COUNT_LATEST, 0) == true){
                throw SpartaException("Cannot write a new counter value for ")
                    << getLocation() << " because its behavior is not COUNT_LATEST. "
                    << "Other behaviors should only support incrementing or adding";
            }

            val_ = val;
            return val;
        }

        //! Sets a new value
        Counter& operator=(counter_type val) {
            set(val);
            return *this;
        }

        //! Sets a new value
        Counter& operator=(const Counter& rhp) {
            set(rhp.get());
            return *this;
        }

        /*!
         * \brief Increments the value with overflow detection
         * \param add Amount by which to increment. Must be >= 0.
         * \return The final value after incrementing
         * \note This is only allowed if behavior is COUNT_NORMAL or COUNT_INTEGRAL or COUNT+LATEST
         * \throw SpartaException if the behavior prohibits this counter from being incremented
         * \todo Allow indexed accesses if larger counters are supported
         */
        counter_type increment(counter_type add) {
            //See if we need to throw any increment exceptions.
            static_assert(std::numeric_limits<counter_type>::is_signed == false,
                          "Counter type expected to be unsigned. If counter type "
                          "is changed or made templated, ths following test should "
                          "probably be enabled and increment should be changed to accept an unsigned value only.");

            // Tempting idea, but very costly in performance.
            // Specifically, the assert is expensive.  :(
            //
            // static_assert(sizeof(counter_type) == sizeof(uint64_t),
            //               "Counter type expected to be the size of an unsigned long (64-bits)");
            // const bool ret = __builtin_uaddl_overflow(val_, add, &val_);
            // sparta_assert(ret != true, "Encountered an overflowing Counter: " << getLocation());

            val_ += add;
            return val_;
        }

        /*!
         * \brief Pre-increments the value with overflow detection
         * \note This is only allowed if behavior is COUNT_NORMAL or COUNT_INTEGRAL
         * \throw SpartaException if the behavior prohibits this counter from being incremented
         * \todo Allow indexed accesses if larger counters are supported
         */
        counter_type operator++() {
            return ++val_;
        }

        /*!
         * \brief Post-increments the value with overflow detection
         * \note This is only allowed if behavior is COUNT_NORMAL or COUNT_INTEGRAL
         * \throw SpartaException if the behavior prohibits this counter from being incremented
         * \todo Allow indexed accesses if larger counters are supported
         */
        counter_type operator++(int) {
            return val_++;
        }

        //! \brief Increment this value withi overflow detection
        //! \param add Always a positive number
        counter_type operator+=(counter_type add){
            return increment(add);
        }

        /*!
         * \brief Gets the value of this counter
         * \return Current value of this counter
         * \todo Allow indexed accesses if larger counters are supported.
         */
        counter_type get() const override {
            return val_;
        }

        //! Gets the value
        operator counter_type() const {
            return get();
        }

        /*!
         * \brief Compare value to value of another counter
         */
        bool operator==(const Counter& rhp) const {
            return get() == rhp.get();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! Counters track integral values, and are good
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
            ss << getBehaviorName(getBehavior());
            ss << " vis:" << getVisibility();
            stringizeTags(ss);
            ss  << '>';
            return ss.str();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief React to child registration
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override {
            (void) child;
            throw SpartaException("Cannot add children to a Counter");
        }

    private:

        /*!
         * \brief Current value of the counter
         */
        uint64_t val_;
    };

} // namespace sparta

// __COUNTER_H__
#endif
