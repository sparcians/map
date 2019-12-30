
#ifndef __SPARTA_STATISTIC_DEF_TRIGGER_H__
#define __SPARTA_STATISTIC_DEF_TRIGGER_H__

#include "sparta/trigger/ManagedTrigger.hpp"
#include "sparta/trigger/Comparator.hpp"
#include "sparta/statistics/Expression.hpp"
#include "sparta/statistics/StatisticDef.hpp"

namespace sparta {
namespace trigger {

/*!
 * \brief Given any StatisticDef object and a target value, this
 * trigger will invoke the SpartaHandler given to the constructor
 * when the StatisticDef's expression evaluates to the target_value.
 */
class StatisticDefTrigger : public ManagedTrigger
{
public:
    StatisticDefTrigger(const std::string & name,
                        const SpartaHandler & callback,
                        const StatisticDef * stat_def,
                        const double target_value) :
        ManagedTrigger(name, stat_def->getClock()),
        callback_(callback),
        target_value_(target_value)
    {
        std::vector<const TreeNode*> unused_vector;
        realized_expression_ = stat_def->realizeExpression(unused_vector);
        predicate_ = trigger::createComparator<double>("==", target_value_);
    }

    /*!
     * \brief By default, the StatisticDef's current value will be compared
     * against the target value using '==' (exactly equal). Use this method
     * to change the comparator. Allowed comparator strings include:
     *   ==, !=, >=, <=, >, <
     */
    void setComparatorAsString(const std::string & comp) {
        predicate_ = trigger::createComparator<double>(comp, target_value_);
        if (!predicate_) {
            //No valid use case for specifying an unrecognized comparison
            throw SpartaException("Unrecognized comparison given to a StatisticDefTrigger: ") << comp;
        }
    }

private:
    /*!
     * \brief This method evaluates the StatisticDef's current value against
     * the target_value that was given to this StatisticDefTrigger constructor.
     */
    bool isTriggerReached_() const override {
        const double current_value = realized_expression_.evaluate();
        return predicate_->eval(current_value);
    }

    /*!
     * \brief When the StatisticDef's current value matches the target value,
     * this method will get invoked and the client's SpartaHandler will be
     * called.
     */
    void invokeTrigger_() override {
        callback_();
    }

    SpartaHandler callback_;
    const double target_value_;
    statistics::expression::Expression realized_expression_;
    std::unique_ptr<trigger::ComparatorBase<double>> predicate_;
};

} // namespace trigger
} // namespace sparta

#endif
