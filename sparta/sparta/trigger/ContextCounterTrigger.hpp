
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

namespace sparta {

class StatInstCalculator;
class CounterBase;
class StatisticDef;
class TreeNode;

namespace trigger {

typedef std::function<
    double(const std::vector<const CounterBase*> &)> InternalCounterCalcFunction;

/*!
 * \brief Given an expression such as:
 *
 *   "stat_def.core0.dispatch.stats.count_insts_per_unit.agg >= 15k"
 *
 * Parse this into a ContextCounterTrigger object with:
 *      statistic def path:   'core0.dispatch.stats.count_insts_per_unit'
 *      internal counter evaluation function name:  'agg'
 *      target: 15k
 *
 * In order to trigger the SpartaHandler at the correct time:
 *   1. The internal counters (substatistics of StatisticDef* stat_def)
 *      will be given to a computation function that combines their
 *      counts into one value (aggregate / sum by default)
 *   2. That value will be compared against the trigger_point value
 *   3. If the calculated value is >= the trigger_point value, the
 *      provided SpartaHandler will be invoked *once*, and never again
 */
class ContextCounterTrigger : public CounterTrigger
{
public:
    /*!
     * \brief Use this constructor if the trigger only needs to use
     * the default "agg" function when evaluating the trigger condition.
     *
     * In other words,
     *
     *   current_value = aggregate(counter0 + ... + counterN)
     *   if (current_value matches trigger predicate) {
     *       invokeSpartaHandler();
     *   }
     */
    ContextCounterTrigger(
        const std::string & name,
        const SpartaHandler & callback,
        const StatisticDef * stat_def,
        const double trigger_point);

    /*!
     * \brief Use this constructor if the trigger should use a non-default
     * calculation function when evaluating the trigger condition. This user-
     * supplied method must be registered by a unique name using the method
     * "registerContextCounterCalcFunction()" in this class.
     *
     * In pseudocode,
     *
     *   sparta::trigger::ContextCounterTrigger::registerContextCounterCalcFunction(
     *       "avg", &myContextCounterAveragingFunction);
     *
     *   current_value = myContextCounterAveragingFunction(counter0 + ... + counterN)
     *   if (current_value matches trigger predicate) {
     *       invokeSpartaHandler();
     *   }
     */
    ContextCounterTrigger(
        const std::string & name,
        const SpartaHandler & callback,
        const StatisticDef * stat_def,
        const double trigger_point,
        const std::string & internal_counter_calc_fcn_name);

    /*!
     * \brief If the default (current >= target) comparison is not appropriate,
     * you may switch the comparator with this method. Valid comparator strings
     * include '==', '!=', '>=', '<=', '>', and '<'
     */
    void setComparatorAsString(const std::string & comp);

    /*!
     * \brief If you want to use the constructor that takes a fifth argument
     * "internal_counter_calc_fcn_name", you must write that method's C++ code
     * and then register that method with this API.
     *
     * The registered method here, let's say {"foo", &myFoo}, will be called
     * at every scheduler tick until the method returns a value that compares
     * to TRUE against the target value.
     *
     * For example, here is a trigger waiting until myFoo() returns exactly 3.14,
     * at which point the provided SpartaHandler will be invoked:
     *
     *    double myFoo(const std::vector<const CounterBase*> & ctrs) {
     *        //...
     *        return 3.14;
     *    }
     *
     *    //...
     *    ContextCounterTrigger::registerContextCounterCalcFunction("foo", &myFoo);
     *
     *    //...
     *    ContextCounterTrigger trigger("MyCCTrigger", CREATE_SPARTA_HANDLER(...), sdefn, 3.14, "foo");
     *    trigger->setComparatorAsString("==");
     */
    static void registerContextCounterCalcFunction(
        const std::string & name,
        InternalCounterCalcFunction calc_fcn);

    /*!
     * \brief Register a context counter aggregate function that is a member function
     * of a user-supplied StatisticDef subclass, such as ContextCounter<T> or even
     * a context counter subclass.
     *
     * It is recommended that instead of calling this method directly, you register
     * your aggregation methods using the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN
     * macro. See <sparta>/example/CoreModel/weighted_context_counter_report_triggers.yaml
     * to see example pseudo-code.
     */
    static void registerContextCounterAggregateFcn(
        SpartaHandler handler,
        const StatisticDef * stat_def,
        const std::string & method_name,
        const double & aggregated_value);

    /*!
     * \brief Deregister all context counter aggregate functions that were previously
     * registered with the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN macro.
     */
    static void deregisterContextCounterAggregateFcns(const StatisticDef * stat_def);

    /*!
     * \brief Determine if a context counter aggregate function has been
     * registered for user-defined aggregate calculation.
     *
     * Example usage:
     *   context_node = (top)
     *   context_tree_node_location = "core0.dispatch.stats.weighted_count_insts_per_unit"
     *   method_name = "max_"
     *
     * Some tree node locations may be prefixed with "stat_def.", so this
     * function will take care of pruning the prefix if needed.
     *
     * Tree node locations might also already have the method name appended to them,
     * for example:
     *
     *   context_node = (top)
     *   context_tree_node_location = "core0.dispatch.stats.weighted_count_insts_per_unit.max_"
     *
     * This will be the case if you used the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN
     * macro to register your callback. See the 'registerContextCounterAggregateFcn()'
     * doxygen above. It is recommended that you always register your callback with this
     * macro, so the method name is usually going to already be appended to the context
     * tree node location. This is why the 'method_name' variable in this function defaults
     * to empty ("") - it will get stripped from the 'context_tree_node_location' and handled
     * internally if needed.
     *
     * \return A wrapper class around two things:
     *   1. The user-defined calculation callback
     *   2. The TreeNode base class for the context counter class which implements this
     *      calculation method
     *
     * \note Returns nullptr if no such method has been registered.
     */
    static std::shared_ptr<StatInstCalculator> findRegisteredContextCounterAggregateFcn(
        const TreeNode * context_node,
        const std::string & context_tree_node_location,
        const std::string & method_name = "");

    ~ContextCounterTrigger();

private:
    virtual bool isTriggerReached_() const override;
    virtual const CounterBase * getCounter() const override;

    class Impl;

    std::shared_ptr<Impl> impl_;
};

} // namespace trigger
} // namespace sparta

