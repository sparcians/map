// <StateHistogram.h> -*- C++ -*-


/**
 * \file StateHistogram.hpp
 * \brief StateHistogram implementation using sparta Counters
 */

#ifndef __SPARTA_STATE_HISTOGRAM_H__
#define __SPARTA_STATE_HISTOGRAM_H__

#include <iostream>
#include <string>
#include <vector>
#include "sparta/utils/MathUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/simulation/Resource.hpp"

namespace sparta
{
/*!
 * \brief StateHistogram class for uint64_t values.
 *
 * A histogram is usually thought of has  having a lower limit, upper limit,
 * and number of bins.  This histogram class requires lower and upper limits,
 * but instead of number of bins, it requires the user to specify
 * number of values per bin.  The number of bins is then calculated as below:
 *     number_of_bins = (upper_limit - lower_limit) / values_per_bin + 1
 *
 * This is possible because this histogram only deals with positive integer
 * values.
 *
 */
template <class StateEnumType>
class StateHistogram : public TreeNode
{
public:

    /*!
     * \brief Not default constructable
     */
    StateHistogram() = delete;

    /*!
     * \brief Not copy-constructable
     */
    StateHistogram(const StateHistogram&) = delete;

    /*!
     * \brief Not move-constructable
     */
    StateHistogram(StateHistogram&&) = delete;

    /*!
     * \brief Not assignable
     */
    void operator=(const StateHistogram&) = delete;

    /*!
     * \brief StateHistogram constructor
     * \param parent_treenode parent node. Must not be nullptr
     * \param histogram_name Name of this histograms. Used as name of the
     * TreeNode representing this histogram
     * \param description Description of this histogran. Stored as TreeNode
     * description
     * \param idle_value the value to capture when none nothing was updated
     * (default = StateEnumType::__FIRST)
     */
    StateHistogram(TreeNode* parent_treenode,
                   std::string histogram_name,
                   std::string description,
                   const StateEnumType idle_value = StateEnumType::__FIRST) :
        TreeNode(histogram_name, description),
        lower_val_(static_cast<uint64_t>(StateEnumType::__FIRST)),
        upper_val_(static_cast<uint64_t>(StateEnumType::__LAST) - 1),
        idle_value_(static_cast<uint64_t>(idle_value)),
        stats_(this)
    {
        if(parent_treenode){
            setExpectedParent_(parent_treenode);
        }

        sparta_assert_context(upper_val_ > lower_val_,
                            "StateHistogram: upper value must be greater than lower value");
        num_bins_ = upper_val_ - lower_val_ + 1;

        // Reserve to use emplacement without tree child reordering
        bin_.reserve(num_bins_);

        uint64_t val = lower_val_;
        std::string total_str;

        for (uint32_t i = 0; i < num_bins_; ++i) {
            std::stringstream str;
            str << "bin_" << std::string(typename sparta::utils::Enum<StateEnumType>::Value(static_cast<StateEnumType>(val)));

            bin_.emplace_back(sparta::CycleCounter(&stats_,
                                                 str.str(),
                                                 str.str() + " histogram bin",
                                                 sparta::Counter::COUNT_NORMAL,
                                                 parent_treenode->getClock()));

            probabilities_.emplace_back(new StatisticDef(&stats_,
                                                         str.str() + "_probability",
                                                         str.str() + " bin probability",
                                                         &stats_,
                                                         str.str() + "/total"));
            ++val;

            if (total_str == "") {
                total_str += str.str();
            } else {
                total_str += " + " + str.str();
            }
        }

        total_.reset(new StatisticDef(&stats_,
                                      "total",
                                      "Aggregated total",
                                      &stats_,
                                      total_str));

        if(parent_treenode){
            parent_treenode->addChild(this);
        }

        // Start capturing the idle value
        startCounting_(idle_value_);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name StateHistogram use methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    void setState(const StateEnumType new_state)
    {
        uint32_t new_val = static_cast<uint32_t>(new_state);
        if (new_val != curr_value_) {
            stopCounting_(curr_value_);
            startCounting_(new_val);
        }
    }

    void setNextState(const StateEnumType new_state)
    {
        uint32_t new_val = static_cast<uint32_t>(new_state);
        if (new_val != curr_value_) {
            stopCounting_(curr_value_, 1);
            startCounting_(new_val, 1);
        }
    }

    StateEnumType getState() const
    {
        return static_cast<StateEnumType>(curr_value_);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    uint64_t getHistogramUpperValue() const { return upper_val_; }
    uint64_t getHistogramLowerValue() const { return lower_val_; }
    uint32_t getNumBins() const { return num_bins_; }
    uint32_t getNumValuesPerBin() const { return 1; }

    /*!
     * \brief Render the cumulative values of this histogram for use in
     * standalone model
     */
    std::string getDisplayStringCumulative() const
    {
        std::stringstream str;
        str << std::dec;
        uint64_t val = lower_val_;
        for (uint32_t i = 0; i < num_bins_; ++i) {
            str << "\t" << getName()
                << "[ " << std::string(typename sparta::utils::Enum<StateEnumType>::Value(static_cast<StateEnumType>(val))) << "] = "
                << bin_[i] << std::endl;
            ++val;
        }
        return str.str();
    }

private:
    /*!
     * \brief Start counting, taking into account the specified delay
     * \param delay Begin incrementing counter after this number of cycles
     * has elapsed on the clock associated with this Counter (see
     * sparta::CounterBase::getClock)
     * \pre Must not be counting already (see stopCounting)
     */
    void startCounting_(uint32_t val, uint32_t delay = 0) {
        sparta_assert(val >= lower_val_);
        sparta_assert(val <= upper_val_);

        uint32_t idx = val - lower_val_;
        sparta_assert((bin_.at(idx)).isCounting() == false);
        (bin_.at(idx)).startCounting(delay);

        curr_value_ = val;
    }

    /*!
     * \brief Stop counting and increment internal count, taking into account the specified delay
     * \param val Value to capture.  Class will determine which bin to increment
     * \param delay Begin incrementing counter after this number of cycles
     * has elapsed on the clock associated with this Counter (see
     * sparta::CounterBase::getClock)
     * \pre Must be counting already (see startCounting)
     */
    void stopCounting_(uint32_t val, uint32_t delay = 0) {
        sparta_assert(val >= lower_val_);
        sparta_assert(val <= upper_val_);

        uint32_t idx = val - lower_val_;
        sparta_assert((bin_.at(idx)).isCounting() == true);
        (bin_.at(idx)).stopCounting(delay);
    }

    const uint64_t lower_val_; //!< Lowest value captured in normal bins
    const uint64_t upper_val_; //!< Highest value vaptured in normal bins
    const uint32_t idle_value_; //!< Value to capture when nothing is captured

    sparta::StatisticSet stats_; //!< StatisticSet node
    std::unique_ptr<sparta::StatisticDef> total_; //!< Total values
    std::vector<sparta::CycleCounter> bin_; //!< Regular bins
    std::vector<std::unique_ptr<sparta::StatisticDef>> probabilities_; //!< Probabilities of each normal bin

    uint32_t num_bins_; //!< Number of bins
    uint32_t curr_value_ = 0; //!< Last value updated
}; // class StateHistogram

}; // namespace sparta
#endif // __SPARTA_STATE_HISTOGRAM_H__
