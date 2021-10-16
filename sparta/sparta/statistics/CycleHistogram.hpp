// <CycleHistogram.h> -*- C++ -*-


/**
 * \file CycleHistogram.hpp
 * \brief CycleHistogram implementation using sparta CycleCounter
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include "sparta/utils/MathUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta
{
    /*!
     * \brief CycleHistogramBase class for uint64_t values.
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
    class CycleHistogramBase
    {
    public:

        /*!
         * \brief Not default constructable
         */
        CycleHistogramBase() = delete;

        /*!
         * \brief Not copy-constructable
         */
        CycleHistogramBase(const CycleHistogramBase&) = delete;

        /*!
         * \brief Not move-constructable
         */
        CycleHistogramBase(CycleHistogramBase&&) = delete;

        /*!
         * \brief Not assignable
         */
        void operator=(const CycleHistogramBase&) = delete;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name CycleHistogramBase use methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Add a value to histogram for one cycle, defaulting back to
         * idle value
         * \param val New value to add
         * \post Correct bin will be incremented
         */
        void addValue(uint64_t val)
        {
            stopCounting_(last_value_);
            startCounting_(val);
            stopCounting_(val, 1);
            startCounting_(idle_value_, 1);

            updateMaxValues_(val);
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
            std::for_each(bin_.begin(), bin_.end(), [&](const sparta::CycleCounter& c) {
                auto c_get = c.get();
                accum += pow(c_get - mean, 2);
            });
            accum += pow((*underflow_bin_ - mean), 2);
            accum += pow((*overflow_bin_ - mean), 2);
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
         * \brief Return aggregate clock cycles of this histogram
         */
        const sparta::CycleCounter& getAggCycles() const{
            return *total_;
        }

        /*!
         * \brief Return vector of regular bin counts.
         */
        const std::vector<sparta::CycleCounter>& getRegularBin() const{
            return bin_;
        }

        /*!
         * \brief Return count of underflow bin.
         */
        const sparta::CycleCounter& getUnderflowBin() const{
            return *underflow_bin_;
        }

        /*!
         * \brief Return count of overflow bin.
         */
        const sparta::CycleCounter& getOverflowBin() const{
            return *overflow_bin_;
        }

        /*!
         * \brief Return underflow probability.
         */
        double getUnderflowProbability() const{
            return ((static_cast<double>(*underflow_bin_))/(static_cast<double>(*total_)));
        }

        /*!
         * \brief Return overflow probability.
         */
        double getOverflowProbability() const{
            return ((static_cast<double>(*overflow_bin_))/(static_cast<double>(*total_)));
        }

        /*!
         * \brief Return vector of probabilities regular bins.
         */
        const std::vector<double>& recomputeRegularBinProbabilities() const{
            bin_prob_vector_.clear();
            for(const auto& b : bin_){
                bin_prob_vector_.emplace_back(((static_cast<double>(b))/(static_cast<double>(*total_))));
            }
            return bin_prob_vector_;
        }

        /*!
         * \brief Set a value to histogram until a new value is set
         * \param val New value to set
         * \post Correct bin will be incremented
         */
        void setValue(uint64_t val)
        {
            if (last_value_ != val) {
                stopCounting_(last_value_);
                startCounting_(val);

                updateMaxValues_(val);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        uint64_t getHistogramUpperValue() const { return upper_val_; }
        uint64_t getHistogramLowerValue() const { return lower_val_; }
        uint64_t getNumBins() const { return num_bins_; }
        uint64_t getNumValuesPerBin() const { return num_vals_per_bin_; }

    protected:

        /*!
         * \brief CycleHistogramBase constructor
         * \param lower_val the lower value of the histogram. Values lower than
         * lower_val go into the underflow bin.
         * \param upper_val the upper value of the histogram. Values higher than
         * upper_val go into the overflow bin.
         * \param num_vals_per_bin Number of values per bin. Must be power of two
         * for fast devision.
         * \param idle_value the value to capture when none nothing was updated
         * (default = 0)
         */
        CycleHistogramBase(const uint64_t lower_val,
                           const uint64_t upper_val,
                           const uint64_t num_vals_per_bin,
                           const uint64_t idle_value = 0) :
            lower_val_(lower_val),
            upper_val_(upper_val),
            num_vals_per_bin_(num_vals_per_bin),
            idle_value_(idle_value)
        {
            sparta_assert_context(upper_val > lower_val,
                                "CycleHistogramBase: upper value must be greater than lower value");
            sparta_assert_context(utils::is_power_of_2(num_vals_per_bin),
                                "CycleHistogramBase: num_vals_per_bin must be power of 2");
            idx_shift_amount_ = utils::floor_log2(num_vals_per_bin);  // for quick divide
            double actual_num_bins = (upper_val - lower_val)/num_vals_per_bin + 1;
            num_bins_ = (uint64_t) actual_num_bins;
            sparta_assert_context(actual_num_bins == num_bins_,
                                "CycleHistogramBase: Actual number of bins (" << actual_num_bins
                                << ") is not an integer");
        }

        /*!
         * \brief Render the cumulative values of this histogram for use in
         * standalone model
         */
        std::string getDisplayStringCumulative_(const std::string &name) const
        {
            std::stringstream str;
            str << std::dec;
            uint64_t running_sum = *underflow_bin_;
            str << "\t" <<  name << "[ UF ] = " << running_sum << std::endl;
            uint64_t start_val = lower_val_;
            uint64_t end_val  = start_val + num_vals_per_bin_ - 1;
            for (uint64_t i = 0; i < num_bins_; ++i) {
                if (end_val > upper_val_) {
                    end_val = upper_val_;
                }
                running_sum +=  (bin_[i]);
                str << "\t" << name
                    << "[ " << start_val << "-" << end_val << " ] = "
                    << running_sum << '\n';
                end_val  += num_vals_per_bin_;
            }
            running_sum += *overflow_bin_;
            str << "\t" << name << "[ OF ] = " << running_sum << std::endl;
            return str.str();
        }

        /*!
         * \brief Start counting, taking into account the specified delay
         * \param delay Begin incrementing counter after this number of cycles
         * has elapsed on the clock associated with this Counter (see
         * sparta::CounterBase::getClock)
         * \pre Must not be counting already (see stopCounting)
         */
        void startCounting_(uint64_t val, uint64_t delay = 0)
        {
            if (val < lower_val_) {
                sparta_assert((*underflow_bin_).isCounting() == false);
                (*underflow_bin_).startCounting(delay);
            }
            else if (val > upper_val_) {
                sparta_assert((*overflow_bin_).isCounting() == false);
                (*overflow_bin_).startCounting(delay);
            }
            else {
                uint64_t idx = (val - lower_val_) >> idx_shift_amount_;
                sparta_assert((bin_.at(idx)).isCounting() == false);
                (bin_.at(idx)).startCounting(delay);
            }

            last_value_ = val;
        }

        /*!
         * \brief Stop counting and increment internal count, taking into account the specified delay
         * \param val Value to capture.  Class will determine which bin to increment
         * \param delay Begin incrementing counter after this number of cycles
         * has elapsed on the clock associated with this Counter (see
         * sparta::CounterBase::getClock)
         * \pre Must be counting already (see startCounting)
         */
        void stopCounting_(uint64_t val, uint64_t delay = 0)
        {
            if (val < lower_val_) {
                sparta_assert((*underflow_bin_).isCounting() == true);
                (*underflow_bin_).stopCounting(delay);
            }
            else if (val > upper_val_) {
                sparta_assert((*overflow_bin_).isCounting() == true);
                (*overflow_bin_).stopCounting(delay);
            }
            else {
                const uint64_t idx = (val - lower_val_) >> idx_shift_amount_;
                sparta_assert((bin_.at(idx)).isCounting() == true);
                (bin_.at(idx)).stopCounting(delay);
            }
        }

        /*!
         *  Keep track of the maximum value seen
         *  \param val The value currently being added to the histogram
         */
        void updateMaxValues_(uint64_t val)
        {
            if (val > max_value_->get()) {
                max_value_->set(val);
            }
        }

        void initializeStats_(StatisticSet* sset,
                              const Clock* clk,
                              const std::string &name,
                              const std::string &description,
                              const InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
                              const InstrumentationNode::visibility_t stat_vis_detailed = InstrumentationNode::AUTO_VISIBILITY,
                              InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
                              InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY,
                              const std::vector<std::string>& histogram_state_names = {})
        {
            // Reserve to use emplacement without tree child reordering
            bin_.reserve(num_bins_);

            const std::string name_total = name.empty() ? std::string("total") : (name + "_total");

            // Setup the underflow bins
            const std::string name_uf = name.empty() ? std::string("UF") : (name + "_UF");
            underflow_bin_ = &sset->createCounter<sparta::CycleCounter>(name_uf,
                                                                      "underflow bin",
                                                                      Counter::COUNT_NORMAL,
                                                                      clk);
            underflow_probability_.reset(new StatisticDef(sset,
                                                          name_uf + "_probability",
                                                          "Probability of underflow",
                                                          sset,
                                                          name_uf + " / " + name_total));

            utils::ValidValue<InstrumentationNode::visibility_t> count_visibility;

            // Setup the normal bins
            uint64_t start_val = lower_val_;
            uint64_t end_val   = start_val + num_vals_per_bin_ - 1;
            std::string weighted_total_str = "( " + std::to_string(lower_val_) + " * " + underflow_bin_->getName() + " )";
            std::string weighted_total_nonzero_str;
            std::string count0_statistic_str;
            for (uint64_t i = 0; i < num_bins_; ++i)
            {
                if (end_val > upper_val_) {
                    end_val = upper_val_;
                }
                std::ostringstream str(name, std::ios_base::ate);
                if (!name.empty()) {
                    str << "_";
                }
                if (SPARTA_EXPECT_TRUE(start_val == end_val)) {
                    if (str.str().empty()) {
                        str << "cycle_";
                    }

                    // This vector could be passed to this method from derived EnumCycleHistogram.
                    // If this container is not empty, then we put actual enum constant names in
                    // statistic definition strings, instead of the generic 0, 1, 2, ..., n.
                    if(histogram_state_names.empty()){
                        str << "count" << start_val;
                    }
                    else{

                        // Here we are looping over the range of the regular bins of the histogram.
                        // The loop index is stored in variable i.
                        // histogram_state_names[0] will have the the stringified name of the enum constant
                        // with value 0.
                        // histogram_state_names[1] will have the the stringified name of the enum constant
                        // with value 1.
                        // Same goes for when i = 1, 2, 3, ..., n.
                        // So, instead of publishing numbers like 0, 1, 2, we publish names like
                        // UOPSTATE::READY, UOPSTATE::WAIT, UOPSTATE::RETIRE etc.
                        str << "count" << histogram_state_names[i];
                    }
                } else {
                    str << "bin_"  << start_val << "_" << end_val;
                }
                if (count0_statistic_str.empty() && start_val == 0) {
                    count0_statistic_str = str.str();
                }

                InstrumentationNode::visibility_t visibility = stat_vis_detailed;
                if (i == 0 || i == (num_bins_ - 1)) {
                    if(stat_vis_general == InstrumentationNode::AUTO_VISIBILITY) {
                        visibility = InstrumentationNode::CONTAINER_DEFAULT_VISIBILITY;
                    } else {
                        visibility = stat_vis_general;
                    }
                } else { // we are internal counts for different levels of utilization.
                    if (stat_vis_detailed == InstrumentationNode::AUTO_VISIBILITY) {
                        visibility = InstrumentationNode::CONTAINER_DEFAULT_VISIBILITY;
                    } else {
                        visibility = stat_vis_detailed;
                    }
                }
                if (i == num_bins_ - 1) {
                    count_visibility = visibility;
                }

                bin_.emplace_back(sparta::CycleCounter(sset,
                                                       str.str(),
                                                       name.empty() ? std::string("cycle_count") : name, i,
                                                       description + " histogram bin",
                                                       sparta::Counter::COUNT_NORMAL,
                                                       clk, visibility));
                probabilities_.emplace_back(new StatisticDef(sset,
                                                             str.str() + "_probability",
                                                             str.str() + " bin probability",
                                                             sset,
                                                             str.str() + "/ " + name_total,
                                                             sparta::StatisticDef::VS_FRACTIONAL,
                                                             visibility));

                if (SPARTA_EXPECT_TRUE(num_vals_per_bin_ == 1)) {
                    weighted_total_str += "+ ( " + std::to_string(start_val) + " * " + str.str() + " )";
                    if (!count0_statistic_str.empty()) {
                        if (i == 1) {
                            weighted_total_nonzero_str = "( " + std::to_string(start_val) + " * " + str.str() + " )";
                        } else if (i > 1) {
                            weighted_total_nonzero_str += "+ ( " + std::to_string(start_val) + " * " + str.str() + " )";
                        }
                    }
                }

                start_val = end_val + 1;
                end_val += num_vals_per_bin_;
            }

            // Setup the overflow bins
            const std::string name_of = name.empty() ? std::string("OF") : (name + "_OF");
            overflow_bin_ = &sset->createCounter<sparta::CycleCounter>(name_of,
                                                                     "overflow bin",
                                                                     Counter::COUNT_NORMAL,
                                                                     clk);
            overflow_probability_.reset(new StatisticDef(sset,
                                                         name_of + "_probability",
                                                         "Probability of overflow",
                                                         sset,
                                                         name_of + " / " + name_total));

            weighted_total_str += " + ( " + std::to_string(upper_val_) + " * " + overflow_bin_->getName() + " )";
            if (!count0_statistic_str.empty()) {
                weighted_total_nonzero_str += " + ( " + std::to_string(upper_val_) + " * " + overflow_bin_->getName() + " )";
            }

            if(stat_vis_avg == InstrumentationNode::AUTO_VISIBILITY) {
                stat_vis_avg = InstrumentationNode::DEFAULT_VISIBILITY;
            }
            // Compute the total of all bins
            total_.reset(new CycleCounter(sset,
                                          name_total,
                                          description,
                                          sparta::Counter::COUNT_NORMAL,
                                          clk, InstrumentationNode::VIS_SUPPORT));

            // Setup to collect the maximum value
            if(stat_vis_max == InstrumentationNode::AUTO_VISIBILITY) {
                stat_vis_max = InstrumentationNode::DEFAULT_VISIBILITY;
            }
            max_value_.reset(new Counter(sset,
                                         name.empty() ? std::string("max_value") : (name + "_max"),
                                         "The maximum value in the histogram",
                                         CounterBase::COUNT_LATEST,
                                         stat_vis_max));

            // Compute weighted average if single value per bin
            if (SPARTA_EXPECT_TRUE(num_vals_per_bin_ == 1)) {
                weighted_average_.reset(new StatisticDef(sset,
                                                         name.empty() ? std::string("weighted_avg") : (name + "_weighted_avg"),
                                                         "Weighted average",
                                                         sset,
                                                         "( " + weighted_total_str + " ) / " + name_total,
                                                         sparta::StatisticDef::VS_ABSOLUTE,
                                                         stat_vis_avg));

                if (!count0_statistic_str.empty()) {
                    const std::string nonzero_weighted_avg_stat_def_name =
                        name.empty() ? std::string("weighted_nonzero_avg") : (name + "_weighted_nonzero_avg");

                    const std::string nonzero_weighted_avg_stat_def_equation =
                        "( " +         weighted_total_nonzero_str             + " ) " +
                        "                          /                                " +
                        "( " +    name_total  + " - " +  count0_statistic_str + " )";

                    weighted_non_zero_average_.reset(new StatisticDef(sset,
                                                                      nonzero_weighted_avg_stat_def_name,
                                                                      "Weighted nonzero average",
                                                                      sset,
                                                                      nonzero_weighted_avg_stat_def_equation,
                                                                      sparta::StatisticDef::VS_ABSOLUTE,
                                                                      stat_vis_avg));
                } else {
                    const std::string nonzero_weighted_avg_stat_def_name =
                        name.empty() ? std::string("weighted_nonzero_avg") : (name + "_weighted_nonzero_avg");

                    weighted_non_zero_average_.reset(new StatisticDef(
                                                                      sset,
                                                                      nonzero_weighted_avg_stat_def_name,
                                                                      "Weighted nonzero average",
                                                                      sset,
                                                                      weighted_average_->getExpression(),
                                                                      sparta::StatisticDef::VS_ABSOLUTE,
                                                                      stat_vis_avg));
                }
            }

            if (!bin_.empty()) {
                std::ostringstream fullness_equation;
                fullness_equation << bin_.back().getName() + " + " + name_of;
                const std::string fullness_equation_str = fullness_equation.str();

                const std::string full_name = name.empty() ? std::string("full") : (name + "_full");
                fullness_.reset(new StatisticDef(sset,
                                                 full_name,
                                                 "Fullness",
                                                 sset,
                                                 fullness_equation_str,
                                                 sparta::StatisticDef::VS_ABSOLUTE,
                                                 count_visibility.getValue()));

                const std::string full_prob_name = full_name + "_probability";
                fullness_probability_.reset(new StatisticDef(sset,
                                                             full_prob_name,
                                                             "Fullness probability",
                                                             sset,
                                                             full_name + " / " + name_total,
                                                             sparta::StatisticDef::VS_FRACTIONAL,
                                                             count_visibility.getValue()));
            }
        }

        const uint64_t lower_val_; //!< Lowest value captured in normal bins
        const uint64_t upper_val_; //!< Highest value vaptured in normal bins
        const uint64_t num_vals_per_bin_; //!< Number of values captured by each bin
        const uint64_t idle_value_; //!< Value to capture when nothing is captured

        std::unique_ptr<sparta::CycleCounter> total_; //!< Total values
        sparta::CycleCounter* underflow_bin_; //!< Bin for all underflow
        sparta::CycleCounter* overflow_bin_; //!< Bin for all overflow
        std::vector<sparta::CycleCounter> bin_; //!< Regular bins
        std::unique_ptr<sparta::StatisticDef> underflow_probability_; //!< Probability of underflow
        std::unique_ptr<sparta::StatisticDef> overflow_probability_; //!< Probability of overflow
        std::vector<std::unique_ptr<sparta::StatisticDef>> probabilities_; //!< Probabilities of each normal bin
        std::unique_ptr<sparta::StatisticDef> weighted_non_zero_average_;
        std::unique_ptr<sparta::Counter> max_value_; //!< The maximum value in the histogram
        std::unique_ptr<sparta::StatisticDef> weighted_average_; //!< The weighted average
        std::unique_ptr<sparta::StatisticDef> fullness_; //!< Sum of the max bin and the overflow bin
        std::unique_ptr<sparta::StatisticDef> fullness_probability_; //!< Probability of the histogram being in a full state

        uint64_t num_bins_; //!< Number of bins
        uint64_t idx_shift_amount_; //!< Number of bits which cannot distinguish between bins for a given input value
        uint64_t last_value_ = 0; //!< Last value updated
        mutable std::vector<double> bin_prob_vector_;
    }; // class CycleHistogramBase

    /*!
     * \brief CycleHistogramStandalone class for uint64_t values.
     *
     * A histogram is usually thought of has having a lower limit, upper limit,
     * and number of bins.  This histogram class requires lower and upper limits,
     * but instead of number of bins, it requires the user to specify
     * number of values per bin.  The number of bins is then calculated as below:
     *     number_of_bins = (upper_limit - lower_limit) / values_per_bin + 1
     *
     * This is possible because this histogram only deals with positive integer
     * values.
     *
     */
    class CycleHistogramStandalone final : public CycleHistogramBase
    {
    public:

        /*!
         * \brief Not default constructable
         */
        CycleHistogramStandalone() = delete;

        /*!
         * \brief Not copy-constructable
         */
        CycleHistogramStandalone(const CycleHistogramStandalone&) = delete;

        /*!
         * \brief Not move-constructable
         */
        CycleHistogramStandalone(CycleHistogramStandalone&&) = delete;

        /*!
         * \brief Not assignable
         */
        void operator=(const CycleHistogramStandalone&) = delete;

        /*!
         * \brief CycleHistogramStandalone constructor
         * \param sset Statistic set to add this histogram's stats into
         * \param clk Clock* for CycleCounter
         * \param histogram_name Name of this histograms.
         * \param description Description of this histogran.
         * \param lower_val the lower value of the histogram. Values lower than
         * lower_val go into the underflow bin.
         * \param upper_val the upper value of the histogram. Values higher than
         * upper_val go into the overflow bin.
         * \param num_vals_per_bin Number of values per bin. Must be power of two
         * for fast devision.
         * \param idle_value the value to capture when none nothing was updated
         * (default = 0)
         * \param stat_vis_general Sets the visibility of the stat
         *                         counters for the 0th and last index
         *                         of the utilization counts, so the
         *                         empty and full counts.
         * \param stat_vis_detailed Sets the visibility of the stat
         *                          counts inbetween 0 and the last
         *                          index. i.e. more detailed than the
         *                          general stats.
         *
         * \warning By default the stat_vis_* options are set to
         *          VIS_SPARTA_DEFAULT, for this structure
         *          AUTO_VISIBILITY resolves to
         *          CONTAINER_DEFAULT_VISIBILITY which at the time of
         *          writing this comment is set to VIS_HIDDEN. If you
         *          rely on the stats from this container you should
         *          explicity set the visibility.
         */
        CycleHistogramStandalone(StatisticSet * sset,
                                 const Clock *  clk,
                                 const std::string &name,
                                 const std::string &description,
                                 const uint64_t lower_val,
                                 const uint64_t upper_val,
                                 const uint64_t num_vals_per_bin,
                                 const uint64_t idle_value = 0,
                                 const InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
                                 const InstrumentationNode::visibility_t stat_vis_detailed = InstrumentationNode::AUTO_VISIBILITY,
                                 const InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
                                 const InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY) :
            CycleHistogramBase(lower_val, upper_val, num_vals_per_bin, idle_value),
            name_(name)
            {
                initializeStats_ (sset, clk, name, description, stat_vis_general, stat_vis_detailed, stat_vis_max, stat_vis_avg);

                total_->startCounting();
                // Start capturing the idle value
                startCounting_(idle_value_);
                updateMaxValues_(idle_value_);
            }

        /*!
         * \brief Render the cumulative values of this histogram for use in
         * standalone model
         */
        std::string getDisplayStringCumulative() const
        {
            return getDisplayStringCumulative_(name_);
        }

    private:

        const std::string name_;

    }; // class CycleHistogramStandalone

    /*!
     * \brief CycleHistogramTreeNode class for uint64_t values.
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
    class CycleHistogramTreeNode final : public TreeNode, public CycleHistogramBase
    {
    public:

        /*!
         * \brief Not default constructable
         */
        CycleHistogramTreeNode() = delete;

        /*!
         * \brief Not copy-constructable
         */
        CycleHistogramTreeNode(const CycleHistogramTreeNode&) = delete;

        /*!
         * \brief Not move-constructable
         */
        CycleHistogramTreeNode(CycleHistogramTreeNode&&) = delete;

        /*!
         * \brief Not assignable
         */
        void operator=(const CycleHistogramTreeNode&) = delete;

        /*!
         * \brief CycleHistogramTreeNode constructor
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
         * \param idle_value the value to capture when none nothing was updated
         * (default = 0)
         * \param stat_vis_general Sets the visibility of the stat
         *                         counters for the 0th and last index
         *                         of the utilization counts, so the
         *                         empty and full counts.
         * \param stat_vis_detailed Sets the visibility of the stat
         *                          counts inbetween 0 and the last
         *                          index. i.e. more detailed than the
         *                          general stats.
         *
         * \warning By default the stat_vis_* options are set to
         *          VIS_SPARTA_DEFAULT, for this structure
         *          AUTO_VISIBILITY resolves to
         *          CONTAINER_DEFAULT_VISIBILITY which at the time of
         *          writing this comment is set to VIS_HIDDEN. If you
         *          rely on the stats from this container you should
         *          explicity set the visibility.
         */
        CycleHistogramTreeNode(TreeNode* parent_treenode,
                               const std::string &histogram_name,
                               const std::string &description,
                               const uint64_t lower_val,
                               const uint64_t upper_val,
                               const uint64_t num_vals_per_bin,
                               const uint64_t idle_value = 0,
                               const InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
                               const InstrumentationNode::visibility_t stat_vis_detailed = InstrumentationNode::AUTO_VISIBILITY,
                               const InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
                               const InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY) :
            TreeNode(histogram_name, description),
            CycleHistogramBase(lower_val, upper_val, num_vals_per_bin, idle_value),
            stats_(this)
            {
                sparta_assert(parent_treenode && parent_treenode->getClock());
                setExpectedParent_(parent_treenode);

                initializeStats_ (&stats_, parent_treenode->getClock(), std::string(), description,
                                  stat_vis_general, stat_vis_detailed, stat_vis_max, stat_vis_avg);

                parent_treenode->addChild(this);

                total_->startCounting();
                // Start capturing the idle value
                startCounting_(idle_value_);
                updateMaxValues_(idle_value_);
            }

        /*!
         * \brief Render the cumulative values of this histogram for use in
         * standalone model
         */
        std::string getDisplayStringCumulative() const
        {
            return getDisplayStringCumulative_(getName());
        }

    private:

        sparta::StatisticSet stats_; //!< StatisticSet node

    }; // class CycleHistogramTreeNode

    using CycleHistogram = CycleHistogramTreeNode;

}; // namespace sparta
