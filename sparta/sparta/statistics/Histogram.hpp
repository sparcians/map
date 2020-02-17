// <Histogram.h> -*- C++ -*-


/**
 * \file Histogram.hpp
 * \brief Histogram implementation using sparta Counters
 */

#ifndef __SPARTA_HISTOGRAM_H__
#define __SPARTA_HISTOGRAM_H__

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
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
 * \brief Histogram base class for uint64_t values.
 *
 * A histogram is usually thought of as having a lower limit, upper limit,
 * and number of bins.  This histogram class requires lower and upper limits,
 * but instead of number of bins, it requires the user to specify
 * number of values per bin.  The number of bins is then calculated as below:
 *     number_of_bins = (upper_limit - lower_limit) / values_per_bin + 1
 *
 * This is possible because this histogram only deals with positive integer
 * values.
 *
 * This class is the base class for two different Histograms:  one which is
 * a TreeNode, and one which is not.
 */
class HistogramBase
{
protected:
    /*!
     * \brief HistogramBase constructor
     * \param lower_val the lower value of the histogram. Values lower than
     * lower_val go into the underflow bin.
     * \param upper_val the upper value of the histogram. Values higher than
     * upper_val go into the overflow bin.
     * \param num_vals_per_bin Number of values per bin. Must be power of two
     * for fast devision.
     */
    HistogramBase(uint64_t lower_val,
                  uint64_t upper_val,
                  uint32_t num_vals_per_bin) :
        lower_val_(lower_val),
        upper_val_(upper_val),
        num_vals_per_bin_(num_vals_per_bin)
    {
        sparta_assert_context(upper_val > lower_val,
                            "Histogram: upper value must be greater than lower value");
        sparta_assert_context(utils::is_power_of_2(num_vals_per_bin),
                            "Histogram: num_vals_per_bin must be power of 2");
        idx_shift_amount_ = utils::floor_log2(num_vals_per_bin);  // for quick devide
        double actual_num_bins = (upper_val - lower_val)/num_vals_per_bin + 1;
        num_bins_ = (uint64_t) actual_num_bins;
        sparta_assert_context(actual_num_bins == num_bins_,
                            "Histogram: Actual number of bins (" << actual_num_bins
                            << ") is not an integer");
    }

public:
    /*!
     * \brief Add a value to histogram
     * \param val New value to add
     * \post Correct bin will be incremented
     * \post Total will be incremented
     */
    void addValue(uint64_t val)
    {
        ++(*total_values_);
        *running_sum_ += val;

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
        if (max_counters_.size()) {
            updateMaxValues_(val);
        }

    }

    /*!
     * \brief Calculate Standard Deviation of counts in bins.
     *  This API also takes into account the count in underflow
     *  and overflow bins.
     */
    double getStandardDeviation() const {

        // Summation of counts in normal bins plus underflow bin and
        // overflow bin.
        const double sum = std::accumulate(bin_.begin(), bin_.end(), 0.0)
            + (*underflow_bin_) + (*overflow_bin_);

        // Total number of bins is number of regular bins
        // plus one for underflow bin and plus one for
        // overflow bin.
        const std::size_t total_num_bins = bin_.size() + 2;

        const double mean = sum / total_num_bins;
        double accum = 0.0;
        std::for_each(bin_.begin(), bin_.end(), [&](const sparta::Counter& c) {
            auto c_get = c.get();
            accum += pow(c_get - mean, 2);
        });
        accum += (*underflow_bin_ - mean) * (*underflow_bin_ - mean);
        accum += (*overflow_bin_ - mean) * (*overflow_bin_ - mean);
        return std::sqrt(accum / (total_num_bins - 1));
    }

    /*!
     * \brief Calculate the mean bin count of all the bins.
     *  This API also takes into account the count in underflow
     *  and overflow bins.
     */
    double getMeanBinCount() const {

        // Summation of counts in normal bins plus underflow bin and
        // overflow bin.
        const double sum = std::accumulate(bin_.begin(), bin_.end(), 0.0)
            + (*underflow_bin_) + (*overflow_bin_);

        // Total number of bins is number of regular bins
        // plus one for underflow bin and plus one for
        // overflow bin.
        const std::size_t total_num_bins = bin_.size() + 2;

        return  sum / total_num_bins;
    }

    /*!
     * \brief Return aggregate of this histogram
     */
    const sparta::Counter& getAggValues() const{
        return *total_values_;
    }

    /*!
     * \brief Return vector of regular bin counts.
     */
    const std::vector<sparta::Counter>& getRegularBin() const{
        return bin_;
    }

    /*!
     * \brief Return count of underflow bin.
     */
    const sparta::Counter& getUnderflowBin() const{
        return *underflow_bin_;
    }

    /*!
     * \brief Return count of overflow bin.
     */
    const sparta::Counter& getOverflowBin() const{
        return *overflow_bin_;
    }

    /*!
     * \brief Return underflow probability.
     */
    double getUnderflowProbability() const{
        return ((static_cast<double>(*underflow_bin_))/(static_cast<double>(*total_values_)));
    }

    /*!
     * \brief Return overflow probability.
     */
    double getOverflowProbability() const{
        return ((static_cast<double>(*overflow_bin_))/(static_cast<double>(*total_values_)));
    }

    /*!
     * \brief Return vector of probabilities regular bins.
     */
    const std::vector<double>& recomputeRegularBinProbabilities() const{
        bin_prob_vector_.clear();
        for(const auto& b : bin_){
            bin_prob_vector_.emplace_back(((static_cast<double>(b))/(static_cast<double>(*total_values_))));
        }
        return bin_prob_vector_;
    }

    uint64_t getHistogramUpperValue() const { return upper_val_; }
    uint64_t getHistogramLowerValue() const { return lower_val_; }
    uint32_t getNumBins() const { return num_bins_; }
    uint32_t getNumValuesPerBin() const { return num_vals_per_bin_; }

protected:

    /*!
     *  Keep track of the maximum 'N' values seen
     *  \param val The value currently being added to the histogram
     */
    void updateMaxValues_(uint64_t val) {

        // If the new value is less than everything already tracked, then
        // there's nothing to update
        auto min_it = max_values_.begin();
        if (*min_it >= val) {
            return;
        }

        max_values_.erase(min_it);
        max_values_.insert(val);

        uint32_t idx = 0;
        for (auto it = max_values_.begin(); it != max_values_.end(); it++) {
            sparta_assert(idx < max_counters_.size());
            max_counters_[idx].set(*it);
            idx++;
        }
    }

    /*!
     * \brief Render the cumulative values of this histogram for use in
     * standalone model
     */
    std::string getDisplayStringCumulative_(const std::string & name) const
    {
        std::stringstream str;
        str << std::dec;
        uint64_t running_sum = *underflow_bin_;
        str << "\t" <<  name << "[ UF ] = " << running_sum << std::endl;
        uint64_t start_val = lower_val_;
        uint64_t end_val  = start_val + num_vals_per_bin_ - 1;
        for (uint32_t i=0; i<num_bins_; ++i) {
            if (end_val > upper_val_)
                end_val = upper_val_;
            running_sum +=  (bin_[i]);
            str << "\t" << name
                << "[ " << start_val << "-" << end_val << " ] = "
                << running_sum << std::endl;
            end_val  += num_vals_per_bin_;
        }
        running_sum += *overflow_bin_;
        str << "\t" << name << "[ OF ] = " << running_sum << std::endl;
        return str.str();
    }

    /*!
     *  Initializes statistics within the histogram
     *  \param sset The statistic set to add all histogram stats into
     *  \param stat_prefix String used as a prefix for all generated stat names
     *  \param bin_vis Visibility of the bin / total / OF / UF stats
     *  \param prob_vis Visibility of the probability stats
     *  \param num_max_values Track the max 'num_max_values' seen as separate counters
     */
    void initializeStats_(StatisticSet * sset,
                          const std::string & stat_prefix = "",
                          InstrumentationNode::Visibility bin_vis = InstrumentationNode::VIS_NORMAL,
                          InstrumentationNode::Visibility prob_vis = InstrumentationNode::VIS_NORMAL,
                          uint32_t num_max_values = 0,
                          InstrumentationNode::Visibility max_vis = InstrumentationNode::VIS_SUMMARY)

    {
        total_values_.reset(new sparta::Counter(sset,
                                       stat_prefix + "total",
                                       "Total values added to the histogram",
                                       Counter::COUNT_NORMAL,
                                       bin_vis));

        running_sum_.reset(new sparta::Counter(sset,
                                            stat_prefix + "sum",
                                            "Sum of all values added to the histogram",
                                            Counter::COUNT_NORMAL,
                                            bin_vis));

        // Reserve to use emplacement without tree child reordering
        bin_.reserve(num_bins_);

        underflow_bin_ = &sset->createCounter<sparta::Counter>(stat_prefix + "UF",
                                                              "underflow bin",
                                                              Counter::COUNT_NORMAL,
                                                              bin_vis);
        underflow_probability_.reset(new StatisticDef(sset,
                                                      stat_prefix + "UF_probability",
                                                      "Probability of underflow",
                                                      sset,
                                                      stat_prefix + "UF" + "/" + stat_prefix + "total",
                                                      StatisticDef::VS_FRACTIONAL,
                                                      prob_vis));
        uint64_t start_val = lower_val_;
        uint64_t end_val   = start_val + num_vals_per_bin_ - 1;
        for (uint32_t i=0; i<num_bins_; ++i) {
            if (end_val > upper_val_)
                end_val = upper_val_;
            std::stringstream str;
            str << stat_prefix << "bin_" << start_val << "_" << end_val;
            bin_.emplace_back(sparta::Counter(sset,
                                            str.str(),
                                            str.str() + " histogram bin",
                                            sparta::Counter::COUNT_NORMAL,
                                            bin_vis));
            probabilities_.emplace_back(new StatisticDef(sset,
                                                         str.str() + "_probability",
                                                         str.str() + " bin probability",
                                                         sset,
                                                         str.str() + "/" + stat_prefix + "total",
                                                         StatisticDef::VS_FRACTIONAL,
                                                         prob_vis));
            start_val = end_val + 1;
            end_val  += num_vals_per_bin_;
        }
        overflow_bin_  = &sset->createCounter<sparta::Counter>(stat_prefix + "OF",
                                                              stat_prefix + "overflow bin",
                                                              Counter::COUNT_NORMAL,
                                                              bin_vis);
        overflow_probability_.reset(new StatisticDef(sset,
                                                     stat_prefix + "OF_probability",
                                                     "Probability of overflow",
                                                     sset,
                                                     stat_prefix + "OF" + "/" + stat_prefix + "total",
                                                     StatisticDef::VS_FRACTIONAL,
                                                     prob_vis));;

        average_.reset(new StatisticDef(sset,
                                        stat_prefix + "average",
                                        "Average of all values added to the histogram",
                                        sset,
                                        stat_prefix + "sum" + "/" + stat_prefix + "total",
                                        StatisticDef::VS_ABSOLUTE,
                                        sparta::InstrumentationNode::VIS_NORMAL));;

        if (num_max_values > 0) {
            max_counters_.reserve(num_max_values);
            for (uint32_t idx = 0; idx < num_max_values; idx++) {
                std::stringstream mvtext;
                mvtext << "maxval" << idx;
                max_counters_.emplace_back(sset,
                                           stat_prefix + mvtext.str(),
                                           stat_prefix + " maximum value",
                                           Counter::COUNT_LATEST,
                                           max_vis);
                max_counters_[idx].set(0); // Counters can't have -1, so use '0' for uninitialized :-/
                max_values_.insert(0);
            }
        }
    }


private:
    const uint64_t lower_val_; //!< Lowest value captured in normal bins
    const uint64_t upper_val_; //!< Highest value vaptured in normal bins
    const uint32_t num_vals_per_bin_; //!< Number of values captured by each bin

    std::unique_ptr<sparta::Counter> total_values_; //!< Total number of values
    std::unique_ptr<sparta::Counter> running_sum_; //!< Sum of all values that have been logged
    sparta::Counter* underflow_bin_; //!< Bin for all underflow
    sparta::Counter* overflow_bin_; //!< Bin for all overflow
    std::vector<sparta::Counter> bin_; //!< Regular bins
    std::unique_ptr<sparta::StatisticDef> underflow_probability_; //!< Probability of underflow
    std::unique_ptr<sparta::StatisticDef> overflow_probability_; //!< Probability of overflow
    std::vector<std::unique_ptr<sparta::StatisticDef>> probabilities_; //!< Probabilities of each normal bin
    std::unique_ptr<sparta::StatisticDef> average_; //!< Average of all values in the histogram

    std::vector<sparta::Counter> max_counters_;
    std::multiset<uint64_t>         max_values_;

    uint32_t num_bins_; //!< Number of bins

    /*!
     * \brief Number of bits which cannot distinguish between bins for a given input value
     */
    uint32_t idx_shift_amount_;
    mutable std::vector<double> bin_prob_vector_;
};

//////////////////////////////////////////////////////////////////////

class HistogramStandalone : public HistogramBase
{
public:
    /*!
     *  Create a standalone histogram
     *  \param sset Statistic set to add this histogram's stats into
     *  \param stat_prefix String prefix to prepend to all internally
     *  generated stat names
     *  \param lower_val Minimum value in the histogram
     *  \param upper_val Maximum value in the histogram
     *  \param num_vals_per_bin Number of values per bin
     *  \param num_max_values Track the top 'num_max_values' maximum values
     *  \param bin_vis Visbility of the bin / OF / UF / total stats
     *  \param prob_vis Visibility of the probability stats
     */
    HistogramStandalone(StatisticSet * sset,
                        const std::string & stat_prefix,
                        uint64_t lower_val,
                        uint64_t upper_val,
                        uint32_t num_vals_per_bin,
                        uint32_t num_max_vals,
                        InstrumentationNode::Visibility bin_vis = InstrumentationNode::VIS_NORMAL,
                        InstrumentationNode::Visibility prob_vis = InstrumentationNode::VIS_NORMAL,
                        InstrumentationNode::Visibility max_vis = InstrumentationNode::VIS_NORMAL) :
        HistogramBase(lower_val, upper_val, num_vals_per_bin)
    {
        initializeStats_(sset, stat_prefix, bin_vis, prob_vis, num_max_vals, max_vis);
    }

};

//////////////////////////////////////////////////////////////////////

/**
 *  This is the original 'Histogram' class.  Most functionality has been
 *  moved into the 'HistogramBase' class
 */
class HistogramTreeNode : public TreeNode, public HistogramBase
{
public:

    /*!
     * \brief Not default constructable
     */
    HistogramTreeNode() = delete;

    /*!
     * \brief Not copy-constructable
     */
    HistogramTreeNode(const HistogramTreeNode&) = delete;

    /*!
     * \brief Not move-constructable
     */
    HistogramTreeNode(HistogramTreeNode&&) = delete;

    /*!
     * \brief Not assignable
     */
    void operator=(const HistogramTreeNode&) = delete;

    /*!
     * \brief HistogramTreeNode constructor
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
    HistogramTreeNode(TreeNode* parent_treenode,
                      const std::string & histogram_name,
                      const std::string & description,
                      uint64_t lower_val,
                      uint64_t upper_val,
                      uint32_t num_vals_per_bin,
                      InstrumentationNode::Visibility bin_vis = InstrumentationNode::VIS_NORMAL,
                      InstrumentationNode::Visibility prob_vis = InstrumentationNode::VIS_NORMAL) :
        TreeNode(histogram_name, description),
        HistogramBase(lower_val, upper_val, num_vals_per_bin),
        sset_(this)
    {
        initializeStats_(parent_treenode, 0, bin_vis, prob_vis,
                         InstrumentationNode::VIS_SUMMARY);
    }

    HistogramTreeNode(TreeNode * parent_treenode,
                      const std::string & histogram_name,
                      const std::string & description,
                      const uint64_t lower_val,
                      const uint64_t upper_val,
                      const uint32_t num_vals_per_bin,
                      const uint32_t num_max_values,
                      const InstrumentationNode::Visibility bin_vis,
                      const InstrumentationNode::Visibility prob_vis) :
        TreeNode(histogram_name, description),
        HistogramBase(lower_val, upper_val, num_vals_per_bin),
        sset_(this)
    {
        initializeStats_(parent_treenode, num_max_values, bin_vis, prob_vis,
                         InstrumentationNode::VIS_SUMMARY);
    }

    std::string getDisplayStringCumulative() const {
        return getDisplayStringCumulative_(getName());
    }


private:
    void initializeStats_(TreeNode * parent_treenode,
                          const uint32_t num_max_values,
                          const InstrumentationNode::Visibility bin_vis,
                          const InstrumentationNode::Visibility prob_vis,
                          const InstrumentationNode::Visibility max_vis)
    {
        if(parent_treenode){
            setExpectedParent_(parent_treenode);
        }

        HistogramBase::initializeStats_(
            &sset_, "", bin_vis, prob_vis, num_max_values, max_vis);

        if(parent_treenode){
            parent_treenode->addChild(this);
        }
    }

    sparta::StatisticSet sset_; //!< StatisticSet node

}; // class HistogramTreeNode

// Maintain backwards compatibility with the legacy Histogram class that
// was always a TreeNode
typedef HistogramTreeNode Histogram;

}; // namespace sparta
#endif // __SPARTA_HISTOGRAM_H__
