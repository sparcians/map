
#ifndef __THREADED_CONTEXT_COUNTER_H__
#define __THREADED_CONTEXT_COUNTER_H__

#include "sparta/statistics/ContextCounter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

namespace sparta {

/*!
 * \brief This is an example context counter subclass used to show
 * how users may supply their own "aggregated value calculation"
 * method via the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN macro.
 *
 * This class tracks which internal counters were incremented
 * since the last cycle, and increments the aggregate value if
 * all internal counters were incremented since the last cycle.
 *
 * Example:
 *         t0  t1  t2  t3   Aggregate
 * Cycle0   0   0   0   0           0
 * Cycle1   1   0   1   1           0
 * Cycle2   2   1   2   2           1
 * Cycle3   2   1   2   2           0
 * Cycle4   3   1   2   3           0
 * Cycle5   4   2   3   4           1
 * Cycle6   7   4   4   9           1
 */
template <class CounterT>
class ThreadedContextCounter : public sparta::ContextCounter<CounterT>
{
public:
    template <class... CounterTArgs>
    ThreadedContextCounter(sparta::StatisticSet * stat_set,
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
        prev_cycles_current_counts_{num_contexts, 0},
        scheduler_(stat_set->getScheduler(true))
    {
        REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(
            ThreadedContextCounter<CounterT>,
            numActive_,
            num_cycles_where_all_contexts_were_active_);

        auto cycle_callback = CREATE_SPARTA_HANDLER(ThreadedContextCounter<CounterT>, numActive_);
        auto clk = stat_set->getClock();
        sparta_assert(clk, "StatisticSet with a null clock given to a ThreadedContextCounter");
        cycle_trig_.reset(new trigger::CycleTrigger(
            "ThreadedContextCounter_cycle_callback", cycle_callback, clk));
        cycle_trig_->setRelative(clk, 1);
    }

    const CounterT & context(const uint32_t idx) const {
        return sparta::ContextCounter<CounterT>::context(idx);
    }

    CounterT & context(const uint32_t idx) {
        return sparta::ContextCounter<CounterT>::context(idx);
    }

private:
    //This method will get called once per cycle, and we will compare
    //the internal counters' current values compared to the previous
    //cycle's values. If all values have incremented since the previous
    //cycle, then we will consider all contexts to be 'active', and
    //increment the 'num_cycles_where_all_contexts_were_active_' value.
    //
    //Note that this method is called by two different scheduled events:
    //  1. Our own CycleTrigger callback, hit once every cycle.
    //  2. Called once per report update asking for our current aggregate
    //     value.
    void numActive_() {
        if (scheduler_->getCurrentTick() > last_analyzed_tick_) {
            compareCurrentCounterValuesToPrevCycleCounterValues_();
        }
        last_analyzed_tick_ = scheduler_->getCurrentTick();

        //Reschedule the cycle trigger 1 cycle into the future
        if (!cycle_trig_->isActive()) {
            cycle_trig_->set();
        }
    }

    //Once each cycle, compare the internal counters' current values
    //with their values in the previous cycle. If all counter values
    //have incremented, then all threads are inferred to be active
    //in this cycle, and our 'num_cycles_where_all_contexts_were_active_'
    //value will increase by 1.
    void compareCurrentCounterValuesToPrevCycleCounterValues_() {
        size_t ctr_idx = 0;
        size_t num_active_contexts = 0;
        for (const auto & internal_ctr : *this) {
            if (internal_ctr.get() > prev_cycles_current_counts_[ctr_idx]) {
                ++num_active_contexts;
            }
            prev_cycles_current_counts_[ctr_idx] = internal_ctr.get();
            ++ctr_idx;
        }
        if (num_active_contexts == this->numContexts()) {
            ++num_cycles_where_all_contexts_were_active_;
        }
    }

    std::unique_ptr<trigger::CycleTrigger> cycle_trig_;
    std::vector<CounterBase::counter_type> prev_cycles_current_counts_;
    double num_cycles_where_all_contexts_were_active_ = 0;
    Scheduler::Tick last_analyzed_tick_ = 0;
    Scheduler *const scheduler_;
};

} // namespace sparta

#endif
