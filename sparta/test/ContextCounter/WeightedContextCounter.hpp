
#ifndef __WEIGHTED_CONTEXT_COUNTER_H__
#define __WEIGHTED_CONTEXT_COUNTER_H__

#include "sparta/statistics/ContextCounter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

namespace sparta {

/*!
 * \brief This is an example context counter subclass used to show
 * how users may supply their own "aggregated value calculation"
 * method via the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN macro.
 *
 * This class stores context weights which it uses in conjunction
 * with the internal counters' values to calculate a weighted
 * average of all its contexts' counters.
 */
template <class CounterT>
class WeightedContextCounter : public sparta::ContextCounter<CounterT>
{
public:
    template <class... CounterTArgs>
    WeightedContextCounter(sparta::StatisticSet * stat_set,
                           const std::string & name,
                           const std::string & desc,
                           const size_t num_contexts,
                           CounterTArgs && ...args) :
        sparta::ContextCounter<CounterT>(stat_set,
                                       name,
                                       desc,
                                       static_cast<uint32_t>(num_contexts),
                                       "Testing",
                                       std::forward<CounterTArgs>(args)...),
        weights_(this->numContexts(), 1)
    {
        REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(
            WeightedContextCounter<CounterT>, weightedAvg_, calculated_average_);

        REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(
            WeightedContextCounter<CounterT>, max_, maximum_);
    }

    const CounterT & context(const uint32_t idx) const {
        return sparta::ContextCounter<CounterT>::context(idx);
    }

    CounterT & context(const uint32_t idx) {
        return sparta::ContextCounter<CounterT>::context(idx);
    }

    void assignContextWeights(const std::vector<double> & weights) {
        if (!weights.empty()) {
            if (weights.size() > 1 && weights.size() != this->numContexts()) {
                throw SpartaException("Invalid weights passed to WeightedContextCounter. The ")
                    << "weights vector passed in had " << weights.size() << " values in it, "
                    << "but this context counter has " << this->numContexts() << " contexts in it.";
            }
            weights_ = weights;
        }
        if (weights_.size() == 1) {
            const double weight = weights_[0];
            weights_ = std::vector<double>(this->numContexts(), weight);
        }
    }

    double calculateWeightedAverage() {
        weightedAvg_();
        return calculated_average_;
    }

private:
    void weightedAvg_() {
        calculated_average_ = 0;
        auto weight_iter = weights_.begin();
        for (const auto & internal_ctr : *this) {
            calculated_average_ += internal_ctr.get() * *weight_iter++;
        }
        calculated_average_ /= this->numContexts();
    }

    void max_() {
        sparta_assert(this->numContexts() > 0);
        maximum_ = std::numeric_limits<double>::min();
        for (const auto & internal_ctr : *this) {
            maximum_ = std::max(maximum_, static_cast<double>(internal_ctr.get()));
        }
    }

    std::vector<double> weights_;
    double calculated_average_;
    double maximum_;
};

} // namespace sparta

#endif
