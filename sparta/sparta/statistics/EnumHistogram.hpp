// <EnumHistogram.hpp> -*- C++ -*-


/**
 * \file EnumHistogram.hpp
 * \brief HistogramEnum implementation using sparta Counters
 */

#ifndef __SPARTA_HISTOGRAMENUM_H__
#define __SPARTA_HISTOGRAMENUM_H__

#include <iostream>
#include <string>
#include <vector>
#include "sparta/utils/MathUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/simulation/Resource.hpp"

namespace sparta
{
/*!
 * \brief HistogramEnum class for sparta::utils::Enum.
 *
 * A histogram is usually thought of has  having a lower limit, upper limit,
 * and number of bins.  This histogram class requires lower and upper limits,
 * but instead of number of bins, it requires the user to specify
 * number of values per bin.  The number of bins is then calculated as below:
 *     number_of_bins = (upper_limit - lower_limit) / values_per_bin + 1
 *
 * This is possible because this histogram only deals with positive integer
 * values.
 */
template <class EnumType>
class HistogramEnum : public TreeNode
{
public:
    typedef EnumType value_type;

    /*!
     * \brief Not default constructable
     */
    HistogramEnum() = delete;

    /*!
     * \brief Not copy-constructable
     */
    HistogramEnum(const HistogramEnum&) = delete;

    /*!
     * \brief Not move-constructable
     */
    HistogramEnum(HistogramEnum&&) = delete;

    /*!
     * \brief Not assignable
     */
    void operator=(const HistogramEnum&) = delete;

    /*!
     * \brief HistogramEnum constructor
     * \param parent_treenode parent node. Must not be nullptr
     * \param histogram_name Name of this histograms. Used as name of the
     * TreeNode representing this histogram
     * \param description Description of this histogran. Stored as TreeNode
     * description
     * \param lower_val the lower value of the histogram. Values lower than
     * lower_val go into the underflow bin.
     * \param upper_val the upper value of the histogram. Values higher than
     * upper_val go into the overflow bin.
     * \param num_vals_per_bin Number of values per bin. Must be power of two
     * for fast devision.
     */
    HistogramEnum(TreeNode* parent_treenode,
                  std::string histogram_name,
                  std::string description
                  ) :
        TreeNode(histogram_name,
                 description),
        lower_val_(static_cast<uint64_t>(EnumType::__FIRST)),
        upper_val_(static_cast<uint64_t>(EnumType::__LAST ) - 1),
        num_vals_per_bin_(1),
        stats_(this),
        total_(&stats_,
               "total",
               "Total values added to the histogram",
               Counter::COUNT_NORMAL)
    {
        if(parent_treenode){
            setExpectedParent_(parent_treenode);
        }

        sparta_assert_context(upper_val_ > lower_val_,
                            "Histogram: upper value must be greater than lower value");
        sparta_assert_context(utils::is_power_of_2(num_vals_per_bin_),
                            "Histogram: num_vals_per_bin must be power of 2");
        idx_shift_amount_ = utils::floor_log2(num_vals_per_bin_);  // for quick devide
        double actual_num_bins = (upper_val_ - lower_val_)/num_vals_per_bin_ + 1;
        num_bins_ = (uint64_t) actual_num_bins;
        sparta_assert_context(actual_num_bins == num_bins_,
                            "Histogram: Actual number of bins (" << actual_num_bins
                            << ") is not an integer");

        // Reserve to use emplacement without tree child reordering
        bin_.reserve(num_bins_);

        underflow_bin_ = &stats_.createCounter<sparta::Counter>("UF",
                                                              "underflow bin",
                                                              Counter::COUNT_NORMAL);
        underflow_probability_.reset(new StatisticDef(&stats_,
                                                      "UF_probability",
                                                      "Probability of underflow",
                                                      &stats_,
                                                      "UF/total"));
        uint64_t start_val = lower_val_;
        uint64_t end_val   = start_val + num_vals_per_bin_ - 1;
        for (uint32_t i=0; i<num_bins_; ++i) {
            if (end_val > upper_val_)
                end_val = upper_val_;
            std::stringstream str;
            str << "bin_" << std::string(typename sparta::utils::Enum<EnumType>::Value(static_cast<EnumType>(start_val))) << "_" << end_val;
            bin_.emplace_back(sparta::Counter(&stats_,
                                            str.str(),
                                            str.str() + " histogram bin",
                                            sparta::Counter::COUNT_NORMAL));
            probabilities_.emplace_back(new StatisticDef(&stats_,
                                                         str.str() + "_probability",
                                                         str.str() + " bin probability",
                                                         &stats_,
                                                         str.str() + "/total"));
            start_val = end_val + 1;
            end_val  += num_vals_per_bin_;
        }
        overflow_bin_  = &stats_.createCounter<sparta::Counter>("OF",
                                                              "overflow bin",
                                                              Counter::COUNT_NORMAL);
        overflow_probability_.reset(new StatisticDef(&stats_,
                                                     "OF_probability",
                                                     "Probability of overflow",
                                                     &stats_,
                                                     "OF/total"));

        if(parent_treenode){
            parent_treenode->addChild(this);
        }
    }

    /*!
     * \brief Add a value to histogram
     * \param val New value to add
     * \post Correct bin will be incremented
     * \post Total will be incremented
     */
    void addValue(EnumType enum_val)
    {
        uint64_t val = static_cast<uint64_t>(enum_val);
        ++total_;

        if (val < lower_val_) {
            ++ (*underflow_bin_);
        }
        else if (val > upper_val_) {
            ++ (*overflow_bin_);
        }
        else {
            uint32_t idx = (val - lower_val_) >> idx_shift_amount_;
            ++ (bin_.at(idx));
        }
    }

    uint64_t getHistogramUpperValue() const { return upper_val_; }
    uint64_t getHistogramLowerValue() const { return lower_val_; }
    uint32_t getNumBins() const { return num_bins_; }
    uint32_t getNumValuesPerBin() const { return num_vals_per_bin_; }

    /*!
     * \brief Render the cumulative values of this histogram for use in
     * standalone model
     */
    std::string getDisplayStringCumulative() const
    {
        std::stringstream str;
        str << std::dec;
        uint64_t running_sum = *underflow_bin_;
        str << "\t" <<  getName() << "[ UF ] = " << running_sum << std::endl;
        uint64_t start_val = lower_val_;
        uint64_t end_val  = start_val + num_vals_per_bin_ - 1;
        for (uint32_t i=0; i<num_bins_; ++i) {
            if (end_val > upper_val_)
                end_val = upper_val_;
            running_sum +=  (bin_[i]);
            str << "\t" << getName()
                << "[ " << std::string(typename sparta::utils::Enum<EnumType>::Value(static_cast<EnumType>(start_val))) << "-"
                << std::string(typename sparta::utils::Enum<EnumType>::Value(static_cast<EnumType>(end_val))) << " ] = "
                << running_sum << std::endl;
            end_val  += num_vals_per_bin_;
        }
        running_sum += *overflow_bin_;
        str << "\t" << getName() << "[ OF ] = " << running_sum << std::endl;
        return str.str();
    }

private:
    const uint64_t lower_val_; //!< Lowest value captured in normal bins
    const uint64_t upper_val_; //!< Highest value vaptured in normal bins
    const uint32_t num_vals_per_bin_; //!< Number of values captured by each bin

    sparta::StatisticSet stats_; //!< StatisticSet node
    sparta::Counter total_; //!< Total values
    sparta::Counter* underflow_bin_; //!< Bin for all underflow
    sparta::Counter* overflow_bin_; //!< Bin for all overflow
    std::vector<sparta::Counter> bin_; //!< Regular bins
    std::unique_ptr<sparta::StatisticDef> underflow_probability_; //!< Probability of underflow
    std::unique_ptr<sparta::StatisticDef> overflow_probability_; //!< Probability of overflow
    std::vector<std::unique_ptr<sparta::StatisticDef>> probabilities_; //!< Probabilities of each normal bin

    uint32_t num_bins_; //!< Number of bins

    /*!
     * \brief Number of bits which cannot distinguish between bins for a given input value
     */
    uint32_t idx_shift_amount_;

}; // class HistogramEnum

}; // namespace sparta
#endif // __SPARTA_HISTOGRAMENUM_H__
