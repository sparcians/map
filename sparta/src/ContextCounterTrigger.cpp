
#include "sparta/trigger/ContextCounterTrigger.hpp"

#include <ostream>
#include <set>
#include <unordered_map>
#include <utility>

#include "sparta/trigger/Comparator.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/statistics/StatInstCalculator.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"

namespace sparta {
class Clock;

namespace trigger {

std::vector<const CounterBase*> getInternalCounters(
    const StatisticDef * stat_def)
{
    std::vector<const CounterBase*> internal_counters;
    for (const auto & sub_stat : stat_def->getSubStatistics()){
        auto tn = dynamic_cast<const CounterBase*>(sub_stat.getNode());
        if (tn == nullptr) {
            throw SpartaException(
                "Invalid StatisticDef substatistic was given to a "
                "ContextCounterTrigger. All substatistics of the given "
                "StatisticDef must be CounterBase objects. The substatistic "
                "found at location ") << sub_stat.getNode()->getLocation() <<
                " is not a CounterBase.";
        }
        internal_counters.emplace_back(tn);
    }
    return internal_counters;
}

const Clock * getSharedClock(const StatisticDef * stat_def)
{
    utils::ValidValue<const Clock*> shared_clock;
    for (const auto & sub_stat : stat_def->getSubStatistics()){
        const Clock * clk = sub_stat.getNode()->getClock();
        if (!shared_clock.isValid()) {
            shared_clock = clk;
        } else {
            sparta_assert(shared_clock.getValue() == clk);
        }
    }
    return shared_clock.isValid() ? shared_clock.getValue() : nullptr;
}

double calculateSumOfInternalCounters(
    const std::vector<const CounterBase*> & counters)
{
    double agg = 0;
    for (const auto & ctr : counters) {
        agg += ctr->get();
    }
    return agg;
}

class ContextCounterTrigger::Impl
{
public:
    Impl(const StatisticDef * stat_def,
         const double trigger_point,
         const std::string & internal_counters_calc_fcn_name) :
        stat_def_(stat_def),
        internal_counters_(getInternalCounters(stat_def)),
        trigger_point_(trigger_point),
        internal_counters_calc_fcn_name_(internal_counters_calc_fcn_name)
    {
        sparta_assert(!internal_counters_.empty());
        predicate_ = trigger::createComparator<double>(">=", trigger_point_);

        if (calc_fcns.find("agg") == calc_fcns.end()) {
            Impl::registerContextCounterCalcFunction(
                "agg", &calculateSumOfInternalCounters);
        }
    }

    void setComparatorAsString(const std::string & comp)
    {
        predicate_ = trigger::createComparator<double>(comp, trigger_point_);

        //No valid use case for specifying an unrecognized comparison
        if (!predicate_) {
            throw SpartaException("Unrecognized comparison given to a ContextCounterTrigger: ") << comp;
        }
    }

    static void registerContextCounterCalcFunction(
        const std::string & name,
        InternalCounterCalcFunction calc_fcn)
    {
        if (calc_fcns.find(name) != calc_fcns.end()) {
            throw SpartaException("There is a user-defined 'is triggered' calculation function called '")
                << name << "' already registered. Calculation functions must be registered with unique names.";
        }
        calc_fcns[name] = calc_fcn;
    }

    static void registerContextCounterAggregateFcn(
        SpartaHandler handler,
        const StatisticDef * stat_def,
        const std::string & method_name,
        const double & aggregated_value)
    {
        const std::string tree_node_location = stat_def->getLocation();
        const std::string handler_locator = tree_node_location + "::" + method_name;
        if (user_defined_aggregator_fcns.find(handler_locator) !=
            user_defined_aggregator_fcns.end()) {
            throw SpartaException("There is a user-defined 'is triggered' calculation function called '")
                << method_name << "' already registered at tree location '" << tree_node_location
                << "'. Calculation functions must be registered with unique names.";
        }
        user_defined_aggregator_fcns[handler_locator].reset(
            new StatInstCalculator(handler, aggregated_value));
        user_defined_aggregator_fcns_by_stat_def_[stat_def].insert(method_name);

        //Use RAII to automatically deregister all of this stat_def's registered
        //aggregation callbacks when it goes out of scope.
        //
        //It is possible that our auto-deregister object has been set already, which
        //happens when one ContextCounter has more than one registered aggregation
        //callback. So we need to check for null or we will inadvertently deregister
        //our aggregation callback too soon (calling .reset on a non-null unique_ptr
        //will trigger the AutoContextCounterDeregistration object's destructor of
        //course).
        if (stat_def->auto_cc_deregister_ == nullptr) {
            stat_def->auto_cc_deregister_.reset(
                new StatisticDef::AutoContextCounterDeregistration(stat_def));
        }
    }

    static void deregisterContextCounterAggregateFcns(
        const StatisticDef * stat_def)
    {
        const auto iter = user_defined_aggregator_fcns_by_stat_def_.find(stat_def);
        if (iter != user_defined_aggregator_fcns_by_stat_def_.end()) {
            const std::string tree_node_location = stat_def->getLocation();
            for (const auto & method_name : iter->second) {
                const std::string handler_locator = tree_node_location + "::" + method_name;
                user_defined_aggregator_fcns.erase(handler_locator);
            }
            user_defined_aggregator_fcns_by_stat_def_.erase(stat_def);
        }
    }

    static std::shared_ptr<StatInstCalculator> findRegisteredContextCounterAggregateFcn(
        const TreeNode * context_node,
        const std::string & context_tree_node_location,
        const std::string & method_name)
    {
        if (!context_node) {
            return nullptr;
        }

        const TreeNode * tn = nullptr;

        static const std::string STAT_DEF = "stat_def.";
        if (context_tree_node_location.find(STAT_DEF) == 0) {
            const std::string pruned_tree_node_location = context_tree_node_location.substr(STAT_DEF.size());
            return findRegisteredContextCounterAggregateFcn(context_node, pruned_tree_node_location, method_name);
        } else {
            tn = context_node->getChild(context_tree_node_location, false);
            if (!tn) {
                auto last_dot_idx = context_tree_node_location.find_last_of(".");
                if (last_dot_idx == std::string::npos) {
                    return nullptr;
                }
                auto pruned_method_name = context_tree_node_location.substr(last_dot_idx + 1);
                auto pruned_tree_node_location = context_tree_node_location.substr(0, last_dot_idx);

                return findRegisteredContextCounterAggregateFcn(
                    context_node,
                    pruned_tree_node_location,
                    pruned_method_name);
            }
        }

        std::string handler_locator = context_tree_node_location + "::" + method_name;
        auto iter = user_defined_aggregator_fcns.find(handler_locator);
        if (iter == user_defined_aggregator_fcns.end()) {
            //We might need to prepend the context node's name
            handler_locator =
                context_node->getName() + "." +
                context_tree_node_location + "::" + method_name;

            iter = user_defined_aggregator_fcns.find(handler_locator);
            if (iter == user_defined_aggregator_fcns.end()) {
                if (method_name.empty()) {
                    //One last chance to find this function! Since all else
                    //has failed, see if we can find a registered default
                    //aggregation method for this node. SPARTA registers
                    //these automatically for all ContextCounterTrigger's.
                    return findRegisteredContextCounterAggregateFcn(
                        context_node, context_tree_node_location, "agg");
                }
                return nullptr;
            }
        }

        std::shared_ptr<StatInstCalculator> calculator;
        calculator = iter->second;
        calculator->setNode(tn);
        return calculator;
    }

    bool isTriggerReached() const
    {
        auto iter = calc_fcns.find(internal_counters_calc_fcn_name_);
        if (iter == calc_fcns.end()) {
            return checkRegisteredMemberFunctionForTriggeredStatus_();
        }
        const double current_value = iter->second(internal_counters_);
        return predicate_->eval(current_value);
    }

    const CounterBase * getCounter() const
    {
        if (internal_counters_.empty()) {
            return nullptr;
        }
        if (internal_counters_.size() == 1) {
            return internal_counters_[0];
        }
        throw SpartaException("You may not call the getCounter() method on this "
                            "ContextCounterTrigger since it has ")
            << internal_counters_.size()
            << " internal counters, not just one.";
        return nullptr;
    }

    ~Impl() = default;

private:
    const StatisticDef * stat_def_ = nullptr;
    std::vector<const CounterBase*> internal_counters_;
    const double trigger_point_;
    const std::string internal_counters_calc_fcn_name_;
    std::unique_ptr<ComparatorBase<double>> predicate_;

    static std::unordered_map<
        std::string, InternalCounterCalcFunction> calc_fcns;

    bool checkRegisteredMemberFunctionForTriggeredStatus_() const {
        const StatInstCalculator & aggregator = getCachedUserDefinedAggregator_();
        const double & current_value = aggregator.getCurrentValue();
        return predicate_->eval(current_value);
    }

    StatInstCalculator & getCachedUserDefinedAggregator_() const {
        if (cached_user_defined_aggregator_ != nullptr) {
            return *cached_user_defined_aggregator_;
        }

        const std::string tree_node_location = stat_def_->getLocation();
        const std::string handler_locator =
            tree_node_location + "::" + internal_counters_calc_fcn_name_;

        auto iter = user_defined_aggregator_fcns.find(handler_locator);
        if (iter == user_defined_aggregator_fcns.end()) {
            std::ostringstream oss;
            oss << "A context counter trigger was given an unrecognized calculation function called '"
                << internal_counters_calc_fcn_name_ << "'. The following functions are the only ones that "
                << "are recognized:\n\t";
            for (const auto & fcn : calc_fcns) {
                oss << fcn.first << "  ";
            }
            throw SpartaException(oss.str());
        }

        cached_user_defined_aggregator_ = iter->second.get();
        return *cached_user_defined_aggregator_;
    }

    static std::unordered_map<
        std::string,
        std::shared_ptr<StatInstCalculator>> user_defined_aggregator_fcns;

    static std::unordered_map<
        const StatisticDef*,
        std::set<std::string>> user_defined_aggregator_fcns_by_stat_def_;

    mutable StatInstCalculator * cached_user_defined_aggregator_ = nullptr;
};

std::unordered_map<std::string, InternalCounterCalcFunction>
    ContextCounterTrigger::Impl::calc_fcns;

std::unordered_map<
    std::string, std::shared_ptr<StatInstCalculator>>
    ContextCounterTrigger::Impl::user_defined_aggregator_fcns;

std::unordered_map<
    const StatisticDef*, std::set<std::string>>
    ContextCounterTrigger::Impl::user_defined_aggregator_fcns_by_stat_def_;

ContextCounterTrigger::ContextCounterTrigger(const std::string & name,
                                             const SpartaHandler & callback,
                                             const StatisticDef * stat_def,
                                             const double trigger_point) :
    ContextCounterTrigger(name, callback, stat_def, trigger_point, "agg")
{
}

ContextCounterTrigger::ContextCounterTrigger(
        const std::string & name,
        const SpartaHandler & callback,
        const StatisticDef * stat_def,
        const double trigger_point,
        const std::string & internal_counter_calc_fcn_name) :
    CounterTrigger(name, callback, getSharedClock(stat_def)),
    impl_(new ContextCounterTrigger::Impl(stat_def, trigger_point, internal_counter_calc_fcn_name))
{
}

void ContextCounterTrigger::setComparatorAsString(const std::string & comp)
{
    impl_->setComparatorAsString(comp);
}

bool ContextCounterTrigger::isTriggerReached_() const
{
    return impl_->isTriggerReached();
}

const CounterBase * ContextCounterTrigger::getCounter() const
{
    return impl_->getCounter();
}

void ContextCounterTrigger::registerContextCounterCalcFunction(
    const std::string & name,
    InternalCounterCalcFunction calc_fcn)
{
    if (name == "agg") {
        throw SpartaException(
            "You may not specify a user-defined 'is triggered' calculation "
            "function called 'agg'. This is a reserved keyword.");
    }
    Impl::registerContextCounterCalcFunction(name, calc_fcn);
}

void ContextCounterTrigger::registerContextCounterAggregateFcn(
    SpartaHandler handler,
    const StatisticDef * stat_def,
    const std::string & method_name,
    const double & aggregated_value)
{
    if (method_name == "agg") {
        throw SpartaException(
            "You may not specify a user-defined 'is triggered' calculation "
            "function called 'agg'. This is a reserved keyword.");
    }
    Impl::registerContextCounterAggregateFcn(
        handler, stat_def, method_name, aggregated_value);
}

void ContextCounterTrigger::deregisterContextCounterAggregateFcns(
    const StatisticDef * stat_def)
{
    Impl::deregisterContextCounterAggregateFcns(stat_def);
}

std::shared_ptr<StatInstCalculator> ContextCounterTrigger::findRegisteredContextCounterAggregateFcn(
    const TreeNode * root,
    const std::string & tree_node_location,
    const std::string & method_name)
{
    return Impl::findRegisteredContextCounterAggregateFcn(
        root, tree_node_location, method_name);
}

ContextCounterTrigger::~ContextCounterTrigger()
{
}

} // namespace trigger
} // namespace sparta
