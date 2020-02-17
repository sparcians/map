// <StatisticSet> -*- C++ -*-


/**
 * \file   StatisticSet.hpp
 *
 * \brief  File that defines the StatisticSet class
 */

#ifndef __STATISTIC_SET_H__
#define __STATISTIC_SET_H__

#include <iostream>

#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief Set of StatisticDef and CounterBase-derived objects for
     * visiblility through a sparta Tree
     */
    class StatisticSet : public TreeNode
    {
    public:

        //! Type for holding stat defs
        typedef std::vector<StatisticDef*> StatisticVector;

        //! Type for holding Counters
        typedef std::vector<CounterBase*> CounterVector;

        //! \brief Name of all StatisticSet nodes
        static constexpr char NODE_NAME[] = "stats";

        /*!
         * \brief Constructor
         * \param parent parent node
         * \note The constructed StatisticSet will be named
         * StatisticSet::NODE_NAME. Therefore, only one StatisticSet may exist as
         * a child of any given node
         */
        StatisticSet(TreeNode* parent) :
            TreeNode(NODE_NAME,
                     TreeNode::GROUP_NAME_BUILTIN,
                     TreeNode::GROUP_IDX_NONE,
                     "Statistic and Counter Set")
        {
            if(parent){
                setExpectedParent_(parent);
                parent->addChild(this);
            }
        }

        /*!
         * \brief Destructor
         */
        ~StatisticSet()
        {;}

        // Overload of TreeNode::stringize
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << ' ' << (stats_.size()) << " stats, "
               << (ctrs_.size()) << " counters>";
            return ss.str();
        }

        //! \name Statistics
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the number of counters in this Set
         */
        uint32_t getNumStatisticDefs() const { return stats_.size(); }

        /*!
         * \brief Gets the vector of StatisticDefs contained by this set.
         * \note There is no non-const version of this method. Modifying this
         * vector externally should never be allowed
         */
        const StatisticVector& getStatisticDefs() const { return stats_; }

        /*!
         * \brief Retrieves a child that is a StatisticDef with the given
         * dotted path.
         * \note no pattern matching supported in this method
         * \note Generally, only immediate children can be fields.
         * \throw SpartaException if child which is a StatisticDef is not found
         */
        StatisticDef* getStatisticDef(const std::string& name) {
            return getChildAs<StatisticDef>(name);
        };

        /*!
         * \brief Allocates a StatisticDef which is owned by this StatisticSet
         * and deleted at its destruction
         * \param __args Variable arguments which satisfy a StatisticDef
         * Constructor having the parent parameter already provided.
         * \return Newly allocated StatisticDef managed by this set. Do NOT
         * attempt to delete this object. It will be deleted upon destruction of
         * this StatisticSet
         * \pre StatisticSet must not be finalized
         *
         * Example:
         * \code
         * // Given: sparta::StatisticSet set;
         * set.createStatisticDef("my_stat", "group", idx, "My Stat");
         * // or
         * set.createStatisticDef("my_stat", "My Stat");
         * \endcode
         */
        template<typename... _Args>
        StatisticDef & createStatisticDef(_Args&&... __args) {
            if(isFinalized()){
                throw SpartaException("Cannot create a new StatisticDef once a StatisticSet is finalized. "
                                    "Error with: ")
                    << getLocation();
            }
            owned_stats_.emplace_back(new StatisticDef(this, __args...));
            return *(static_cast<StatisticDef *>(owned_stats_.back().get()));
        }


        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Counters
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the number of counters in this Set
         */
        uint32_t getNumCounters() const {
            return ctrs_.size();
        }

        /*!
         * \brief Gets the vector of Counters contained by this set.
         * \note There is no non-const version of this method. Modifying this
         * vector externally should never be allowed.
         */
        const CounterVector& getCounters() const {
            return ctrs_;
        }

        /*!
         * \brief Retrieves a child that is a Counter with the given
         * dotted path.
         * \note no pattern matching supported in this method
         * \warn This method should be considered slow. Cache counters of
         * interest instead of looking them up in performance-critical code.
         * \throw SpartaException if child which is a counter is not found
         */
        const CounterBase* getCounter(const std::string& name) const {
            return getChildAs<CounterBase>(name);
        }

        /*!
         * \brief Retrieves a child that is a Counter with the given
         * dotted path.
         * \note no pattern matching supported in this method
         * \warn This method should be considered slow. Cache counters of
         * interest instead of looking them up in performance-critical code.
         * \throw SpartaException if child which is a counter is not found
         */
        CounterBase* getCounter(const std::string& name) {
            return getChildAs<CounterBase>(name);
        }

        /*!
         * \brief Retrieves a child that is a Counter with the given
         * dotted path.
         * \note no pattern matching supported in this method
         * \warn This method should be considered slow. Cache counters of
         * interest instead of looking them up in performance-critical code.
         * \throw SpartaException if child which is a counter is not found
         */
        template<class CounterT>
        const CounterT* getCounterAs(const std::string& name) const {
            return getChildAs<CounterT>(name);
        }

        /*!
         * \brief Retrieves a child that is a Counter with the given
         * dotted path (non-const).
         * \note no pattern matching supported in this method
         * \warn This method should be considered slow. Cache counters of
         * interest instead of looking them up in performance-critical code.
         * \throw SpartaException if child which is a counter is not found
         */
        template<class CounterT>
        CounterT* getCounterAs(const std::string& name) {
            return getChildAs<CounterT>(name);
        }

        /*!
         * \brief Allocates a Counter which is owned by this StatisticSet and
         * deleted at its destruction
         * \tparam CounterT counter type to construct (Counter, CycleCounter,
         * ReadOnlyCounter)
         * \param __args Variable arguments which satisfy a Counter Constructor
         * having the parent parameter already provided.
         * \return Newly allocated Counter managed by this set. Do NOT attempt
         * to delete this object. It will be deleted upon destruction of this
         * StatisticSet
         * \pre StatisticSet must not be finalized
         *
         * Example:
         * \code
         * // Given: sparta::StatisticSet set:
         * set.createCounter<sparta::Counter>("my_counter", "group", idx, "My Counter", sparta::Counter::COUNT_NORMAL);
         *
         * // or maybe a different counter type:
         * set.createCounter<sparta::CycleCounter>("my_counter", "My Counter", sparta::Counter::COUNT_NORMAL);
         * \endcode
         */
        template<class CounterT, typename... _Args>
        CounterT & createCounter(_Args&&... __args) {
            if(isFinalized()){
                throw SpartaException("Cannot create a new Counter once a StatisticSet is finalized. "
                                    "Error with: ")
                    << getLocation();
            }
            owned_ctrs_.emplace_back(new CounterT(this, __args...));
            return *((CounterT *)owned_ctrs_.back().get());
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief React to a child registration
         * \param child TreeNode child that must be downcastable to a
         * sparta::StatisticDef or a sparta::CounterBase. This is a borrowed
         * reference - child is *not* copied. Child lifetime must exceed that of
         * this StatisticSet instance.
         * \pre Must not be finalized
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override {
            if(isFinalized()){
                throw SpartaException("Cannot add a child Counter once a StatisticSet is finalized. "
                                    "Error with: ")
                    << getLocation();
            }

            StatisticDef* stat = dynamic_cast<StatisticDef*>(child);
            if(nullptr != stat){
                // Add stat to stats_ list for tracking.
                stats_.push_back(stat);
                return;
            }

            CounterBase* ctr = dynamic_cast<CounterBase*>(child);
            if(nullptr != ctr){
                // Add Counter to ctrs_ list for tracking.
               ctrs_.push_back(ctr);
               return;
            }

            throw SpartaException("Cannot add TreeNode child ")
                << child->getName() << " to StatisticSet " << getLocation()
                << " because the child is not a CounterBase or StatisticDef";
        }

        /*!
         * \brief Vector of unique pointers to statistics
         */
        typedef std::vector<std::unique_ptr<StatisticDef>> OwnedStatisticVector;

        /*!
         * \brief All stats allocated by this set. These stats are deleted
         * at destruction of this StatisticSet
         */
        OwnedStatisticVector owned_stats_;

        /*!
         * \brief All stats contained by this set whether allocated by this
         * set or not (superset of owned_stats_)
         */
        StatisticVector stats_;

        /*!
         * \brief Vector of unique pointers to counter bases
         */
        typedef std::vector<std::unique_ptr<CounterBase>> OwnedCounterVector;

        /*!
         * \brief All Counters allocated by this set. These ctrs are deleted
         * at destruction of this StatisticSet
         */
        OwnedCounterVector owned_ctrs_;

        /*!
         * \brief Set of known Counter children of this set
         * \note These counters are *not* scoped to this StatisticSet and are not
         * deallocated within the destructor
         *
         * This is a superset of owned_ctrs_
         */
        CounterVector ctrs_;
    };

} // namespace sparta

// __STATISTIC_SET_H__
#endif
