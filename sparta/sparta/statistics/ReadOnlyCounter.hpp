// <Counter> -*- C++ -*-


#ifndef __READ_ONLY_COUNTER_H__
#define __READ_ONLY_COUNTER_H__

#include "sparta/utils/ByteOrder.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief Represents a non-writable and non-observable counter with a
     * very similar interface to sparta::Counter. The value of this counter
     * In most cases, a normal counter should be used. However, if a value must
     * be stored as an integer outside of counter for any reason, a ReadOnly
     * counter can be used to wrap that value and expose it to sparta Report and
     * statistics infrastructure.
     * \note ReadOnlyCounters are completely passive and not checkointable
     * \note This is not a subclass because virtual set/increment methods
     * introduce much overhead in Counters.
     */
    class ReadOnlyCounter : public CounterBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief ReadOnlyCounter constructor
         * \param parent parent node. Must not be nullptr. Must have an ArchData
         * accessible through getArchData_
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param group Group of this counter. Must be a valid TreeNode group
         * when paired with group_idx.
         * \param group_idx Group index. Must be a valid TreeNode group_idx
         * when paired with group.
         * \param desc Description of this node. Required to be a valid
         * TreeNode description.
         * \param behave Behavior of this counter. This is not enforced for
         * ReadOnlyCounter but used as a hint for the sparta report and statistics
         * infrastructure
         * \param visibility Visibility level of this counter
         */
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        TreeNode::group_idx_type group_idx,
                        const std::string& desc,
                        CounterBehavior behave,
                        const counter_type* ref,
                        visibility_t visibility) :
            CounterBase(parent,
                        name,
                        group,
                        group_idx,
                        desc,
                        behave,
                        visibility),
            ref_(ref)
        { }

        //! Alternate constructor with no variable reference
        //! \note Used, this counter must be subclassed and get() must be
        //! overridden
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        TreeNode::group_idx_type group_idx,
                        const std::string& desc,
                        CounterBehavior behave,
                        visibility_t visibility) :
            ReadOnlyCounter(parent,
                            name,
                            group,
                            group_idx,
                            desc,
                            behave,
                            nullptr,
                            visibility)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate constructor
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        CounterBehavior behave,
                        const counter_type* ref,
                        visibility_t visibility) :
            ReadOnlyCounter(parent,
                            name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            desc,
                            behave,
                            ref,
                            visibility)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate constructor
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        CounterBehavior behave,
                        visibility_t visibility) :
            ReadOnlyCounter(parent,
                            name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            desc,
                            behave,
                            nullptr,
                            visibility)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate Constructor
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        TreeNode::group_idx_type group_idx,
                        const std::string& desc,
                        CounterBehavior behave,
                        const counter_type* ref) :
            ReadOnlyCounter(parent,
                            name,
                            group,
                            group_idx,
                            desc,
                            behave,
                            ref,
                            DEFAULT_VISIBILITY)
        { }

        //! Alternate constructor with no variable reference
        //! \note Used, this counter must be subclassed and get() must be
        //! overridden
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        TreeNode::group_idx_type group_idx,
                        const std::string& desc,
                        CounterBehavior behave) :
            ReadOnlyCounter(parent,
                            name,
                            group,
                            group_idx,
                            desc,
                            behave,
                            nullptr,
                            DEFAULT_VISIBILITY)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate constructor
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        CounterBehavior behave,
                        const counter_type* ref) :
            ReadOnlyCounter(parent,
                            name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            desc,
                            behave,
                            ref,
                            DEFAULT_VISIBILITY)
        {
            // Initialization handled in delegated constructor
        }

        // Alternate constructor
        ReadOnlyCounter(TreeNode* parent,
                        const std::string& name,
                        const std::string& desc,
                        CounterBehavior behave) :
            ReadOnlyCounter(parent,
                            name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            desc,
                            behave,
                            nullptr,
                            DEFAULT_VISIBILITY)
        {
            // Initialization handled in delegated constructor
        }

        /*!
         * \brief Move constructor
         * \pre See CounterBase move constructor
         */
        ReadOnlyCounter(ReadOnlyCounter&& rhp) :
            CounterBase(std::move(rhp)),
            ref_(rhp.ref_)
        {
            TreeNode* parent = rhp.getParent();
            if(parent != nullptr){
                parent->addChild(this);
            }
        }

        /*!
         * \brief Destructor
         * \note ReadOnlyCounter is not intended to be overloaded
         */
        ~ReadOnlyCounter() {}

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
            sparta_assert(ref_ != nullptr,
                              "Canot 'get()' on ReadOnlyCounter "
                              << getLocation() << " because it has a null ref_ "
                              "pointer. If constructed without a variale "
                              "reference, the get() method must be overridden.");
            return *ref_;
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
        bool operator==(const CounterBase& rhp) const {
            return get() == rhp.get();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! ReadOnlyCounters track integral values, and are good
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
            (void) child;
            throw SpartaException("Cannot add children to a ReadOnlyCounter");
        }

    private:

        /*!
         * \brief Counter from which value will be read
         */
        const counter_type* const ref_;

    }; // class ReadOnlyCounter

} // namespace sparta

// __READ_ONLY_COUNTER_H__
#endif
