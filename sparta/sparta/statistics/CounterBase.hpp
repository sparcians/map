// <Counter> -*- C++ -*-


#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/ByteOrder.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief The base class for all Counters
     *
     */
    class CounterBase : public InstrumentationNode
    {
    public:

        //! \name Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Counter value type
         */
        typedef uint64_t counter_type;

        /*!
         * \brief Behavior of this counter
         * \note If a new behavior is added, add it to getBehaviorName
         */
        enum CounterBehavior {

            /*!
             * \brief Counter counts the number of times something happens like
             * one would expect. This is a weakly monotonically increasing
             * value.
             *
             * Counters with this behavior can be read by the SPARTA reporting
             * system as a delta over some time range to accurately represent
             * the behavior of that counter over that time range.
             *
             * These counters can be incremented or added to, but never set
             * directly. This restriction enforces the monotonically-increasing
             * requirement.
             */
            COUNT_NORMAL   = 1,

            /*!
             * \brief Counter intended to increase each cycle by some variable
             * X.

             * Using a counter in this way effectively takes the integral of X.
             * If X were the number of entries in some queue, which was
             * 3, 6, 1, 1, 1 over 5 cycles, this counter would be incremented by
             * 3, then 6, then 1, 1, and finally 1 again, resulting in 12. In
             * post-processing the delta of this type of counter (which is a
             * discrete integral) over any range of time can be differentiated
             * to get the average value of X over that range of time.
             *
             * Consider using this behavior with a sparta::CycleCounter to
             * automatically add values each cycle.
             *
             * Counters with this behavior can be read by the SPARTA reporting
             * system as a delta over some time range to accurately represent
             * the behavior of that counter over that time range.
             *
             * These counters can be incremented or added to, but never set
             * directly. This restriction enforces the monotonically-increasing
             * requirement.
             */
            COUNT_INTEGRAL = 2,

            /*!
             * \brief Counter holds the latest value (from most recent
             * activity) and can increase or decrease at any time.
             *
             * This type of counter is meant to represent values are not simply
             * counting the number of times something happened or the integral
             * of some variable.
             *
             * If a value needs to be set or cleared at some point, this
             * behavior is needed. However, this is an infrequent need since
             * SPARTA provides the COUNT_INTEGRAL behavior for dealing with some
             * types of variables.
             *
             * Always consider using other counter types before this. They more
             * accuratly represent the underlying behavior and are more useful
             * to SPARTA's reporting system. When looking at behavior over a range
             * of time, SPARTA's reporting system must treat COUNT_LATEST counter
             * values as samples instead of using the delta over that time range
             * (as with COUNT_NORMAL and COUNT_INTEGRAL). This can lead to an
             * inaccurate representation of average behavior over a time range
             */
            COUNT_LATEST   = 3
        };

        ////////////////////////////////////////////////////////////////////////
        //! @}

    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief CounterBase constructor
         * \param parent parent node. Must not be nullptr
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param group Group of this counter. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param behave Behavior of this counter. This is not enforced for
         * CounterBase but used as a hint for the sparta report and statistics
         * infrastructure
         */
        CounterBase(TreeNode* parent,
                    const std::string& name,
                    const std::string& group,
                    TreeNode::group_idx_type group_idx,
                    const std::string& desc,
                    CounterBehavior behave,
                    visibility_t visibility) :
            InstrumentationNode(nullptr,
                                name,
                                group,
                                group_idx,
                                desc,
                                InstrumentationNode::TYPE_COUNTER,
                                visibility
                                ),
            behave_(behave)
        {
            setExpectedParent_(parent);

            ensureParentIsValid_(parent);

            parent->addChild(this);
        }

        // Alternate constructor
        CounterBase(TreeNode* parent,
                    const std::string& name,
                    const std::string& group,
                    TreeNode::group_idx_type group_idx,
                    const std::string& desc,
                    CounterBehavior behave) :
            CounterBase(parent,
                        name,
                        group,
                        group_idx,
                        desc,
                        behave,
                        DEFAULT_VISIBILITY)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate constructor
        CounterBase(TreeNode* parent,
                    const std::string& name,
                    const std::string& desc,
                    CounterBehavior behave) :
            CounterBase(parent,
                        name,
                        TreeNode::GROUP_NAME_NONE,
                        TreeNode::GROUP_IDX_NONE,
                        desc,
                        behave)
        {
            // Initialization handled in delegated constructor
        }

        /*!
         * \brief Copy construction not allowed
         */
        CounterBase(const CounterBase& rhp) = delete;

        /*!
         * \brief Move constructor
         * \pre See InstrumentationNode move constructor
         */
        CounterBase(CounterBase&& rhp) :
            InstrumentationNode(std::move(rhp)),
            behave_(rhp.behave_)
        {;}

        /*!
         * \brief Destructor
         */
        virtual ~CounterBase() {}

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Const Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! Gets the behavior for this counter specified at construction
        CounterBehavior getBehavior() const { return behave_; }

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
        virtual counter_type get() const = 0;

        /*!
         * \brief Cast operator to get value of the counter
         */
        operator counter_type() const {
            return get();
        }

        //! Counters are normally good candidates for compression,
        //! but this is a base class that could be implemented by
        //! a subclass outside of SPARTA. We should say the default
        //! behavior is to *not* compress counters, and let the
        //! subclasses that live in SPARTA override this and say
        //! "yes, I support compression".
        virtual bool supportsCompression() const {
            return false;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Printing Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const = 0;

        /*!
         * \brief Returns a string containing the name of the given behavior
         */
        static std::string getBehaviorName(CounterBehavior behave) {
            switch(behave){
            case COUNT_NORMAL:
                return "normal";
            case COUNT_INTEGRAL:
                return "integral";
            case COUNT_LATEST:
                return "current";
            }
            throw SpartaException("unknown counter behavior: ") << behave;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief React to child registration
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) {
            (void) child;
            throw SpartaException("Cannot add children to a CounterBase");
        }

    private:

        /*!
         * \brief Ensures that the parent node is a StatisticSet
         * \param parent Node to test for validity
         * \throw SpartaException if node is not a StatisticSet
         * \note Uses dynamic_cast so parent must have its StatisticSet base
         * constructed before being used as an argument to this function
         */
        void ensureParentIsValid_(TreeNode* parent);

        /*!
         * \brief Behavior of this counter
         */
        const CounterBehavior behave_;

    }; // class CounterBase

} // namespace sparta
