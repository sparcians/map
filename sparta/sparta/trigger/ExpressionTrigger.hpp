// <ExpressionTrigger> -*- C++ -*-

/**
 * \file ExpressionTrigger.hpp
 *
 */

#pragma once

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/iter_find.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>
#include <math.h>
#include <cstddef>
#include <cstdint>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/algorithm/string/detail/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/detail/basic_pointerbuf.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/lexical_cast/bad_lexical_cast.hpp>
#include <boost/range/const_iterator.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <set>
#include <string>
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"
#include "sparta/trigger/StatisticDefTrigger.hpp"
#include "sparta/trigger/Comparator.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/statistics/StatisticDef.hpp"

namespace sparta::app {
class Simulation;
}  // namespace sparta::app

namespace sparta {

class SubContainer;
class Clock;

namespace app {
    class SimulationConfiguration;
}

namespace trigger {

template <typename T>
utils::ValidValue<T> parseParameter_(const TreeNode * context,
                                     const std::string & param_path,
                                     const std::string & full_expression,
                                     const bool allow_zero) {
    utils::ValidValue<T> parsed;

    const ParameterBase * prm = context->getChildAs<const ParameterBase*>(
        param_path, false);
    if (prm == nullptr) {
        return parsed;
    }

    const std::string prm_value_str = prm->getValueAsString();
    size_t npos = 0;
    try {
        parsed = utils::smartLexicalCast<T>(prm_value_str, npos);
    } catch (...) {
        npos = 0;
    }

    if (npos != std::string::npos) {
        parsed.clearValid();
        return parsed;
    }

    if (!allow_zero && parsed.getValue() == 0) {
        std::ostringstream oss;
        oss << "Parameter '" << param_path << "' used in expression '" << full_expression
            << " cannot be used since it has a value of 0. Zero-value trigger points "
            << "are disallowed.";
        throw SpartaException(oss.str());
    }

    return parsed;
}

class SkippedAnnotatorBase;

/*!
 * \brief This class can be used to parse "trigger expressions"
 * and create the required triggers for you under the hood. Expressions
 * can be of the form:
 *
 *      //Counter trigger
 *      "core0.rob.stats.total_number_retired >= 1000"
 *
 *      //Notification source
 *      "notif.notification_channel_name >= 80"
 *
 *      //Referenced (or named) trigger
 *      "t1.start"
 *
 * Using these supported comparisons:
 *
 *             ==, !=, >=, <=, >, <
 *
 * Composite / aggregate behavior is also supported (&&,||):
 *
 *             "t0.start && t1.start"
 *             "notif.something_cool < 57 || t1.stop"
 *
 * You may use more than two &&:
 *
 *             "t0.start && t1.start && ..."
 *
 * You may use more than two ||:
 *
 *             "t0.start || t1.start || ..."
 *
 * Using a combination of && and || must be done with parentheses:
 *
 *             "t0.start && (t1.start || notif.check_this_value <= 35)"
 *
 * Whether aggregate or standalone, the callback you provide
 * the constructor will be executed just once when all conditions
 * of the given expression have been met.
 */
class ExpressionTrigger
{
public:
    ExpressionTrigger(const std::string & name,
                      const SpartaHandler & callback,
                      const std::string & expression,
                      TreeNode * context,
                      const std::shared_ptr<SubContainer> & report_container) :
        name_(name),
        callback_(callback),
        original_expression_(expression),
        context_(context),
        report_container_(report_container)
    {
        this->pruneExpression_(original_expression_);
        this->parseExpression_(name, original_expression_, context);
    }

    typedef std::function<void(const std::string &)> StringPayloadTrigCallback;
    void switchToStringPayloadCallback(
        StringPayloadTrigCallback callback,
        const std::string & string_payload)
    {
        string_payload_cb_ = std::make_pair(callback, string_payload);
    }

    /*!
     * \brief Even though expressions can be composed of any number of underlying
     * trigger objects, this class still needs the ability to act as just a single
     * CounterTrigger object - if for no other reason than supporting legacy
     * diagnostic printouts.
     *
     * An expression such as: "core0.rob.stats.total_number_retired >= 1400"
     * can be resolved as just one counter trigger, so if you need that CounterTrigger
     * in your callback for any reason, switch to this signature.
     *
     * HOWEVER, an expression such as: "notif.user_channel >= 88 || core1.stop" does not
     * work with the "single trigger callback" because it will not be just one trigger that
     * initiates the client's code. Not only is such an expression composite (i.e. not just
     * one trigger) any referenced / named triggers could themselves be composite expressions.
     *
     * \return This method returns TRUE if successful - the callback you gave the constructor
     * will be discarded and the new one used - and FALSE otherwise - the original SpartaHandler
     * signature you gave the constructor will be used.
     */
    typedef std::function<void(const CounterTrigger*)> SingleCounterTrigCallback;
    bool switchToSingleCounterTriggerCallbackIfAble(SingleCounterTrigCallback cb)
    {
        if (!supports_single_ct_trig_cb_) {
            return false;
        } else {
            single_ct_trig_callback_ = cb;
        }
        return true;
    }

    bool hasFired() const
    {
        return has_fired_;
    }

    /*!
     * \brief Triggers can be uniquely defined by <entity.event> e.g. "t0.start",
     * "core1.stop", etc. and while "start" and "stop" are recognized keywords,
     * anything else in front of the '.' is just a string - shorthand to be able
     * to refer to that trigger later without creating a new one
     */
    void setReferenceEvent(const std::string & tag, const std::string & event);

    /*!
     * \brief Each component of an expression trigger will hit this one
     * "notify()" method when they are individually triggered. Based on
     * the expression's policy (&&, ||, or no policy) the client callback
     * may get triggered in turn.
     */
    void notify()
    {
        this->decrement_();
    }

    /*!
     * During the callback invocation, you may ask the expression trigger
     * to be rescheduled for future callbacks. Calling this method outside
     * of the SpartaHandler that was initially given to this object's constructor
     * will throw an exception.
     */
    void reschedule()
    {
        if (waiting_on_ != 0) {
            throw SpartaException("ExpressionTrigger '")
                << original_expression_ << "' cannot be rescheduled "
                << "since it is currently active. You may only called this "
                << "method from inside the trigger's callback (the SpartaHandler "
                << "given to the constructor)";
        }

        for (auto & trigger : source_counter_triggers_) {
            trigger->set();
            ++waiting_on_;
        }

        for (auto & trigger : source_subclass_triggers_) {
            trigger->set();
            ++waiting_on_;
        }

        has_fired_ = (waiting_on_ == 0);
    }

    /*!
     * \brief Inform notification triggers to keep invoking client
     * callbacks until told otherwise (notification triggers are
     * single-fire by default)
     */
    void stayActive()
    {
        for (auto & trigger : source_notification_triggers_) {
            trigger->stayActive();
        }
    }

    /*!
     * \brief Inform notification triggers to simply early return
     * whenever their expression is true (which would otherwise
     * invoke the client callback)
     *
     * (Intended to be used in tandem with 'awaken')
     */
    void suspend()
    {
        for (auto & trigger : source_notification_triggers_) {
            trigger->suspend();
        }
    }

    /*!
     * \brief Inform notification triggers to resume invoking
     * client callbacks whenever their expression is true
     *
     * (Intended to be used in tandem with 'suspend')
     */
    void awaken()
    {
        for (auto & trigger : source_notification_triggers_) {
            trigger->awaken();
        }
        if (waiting_on_ == 0) {
            waiting_on_ = source_notification_triggers_.size();
            has_fired_ = false;
        }
    }

    /*!
     * \brief Periodic / repeating triggers can produce a lot of messages.
     * Disable this trigger's status printout with this method.
     */
    void disableMessages()
    {
        invoke_callback_message_str_.clear();
    }

    /*!
     * \brief Return the trigger's expression string
     */
    virtual std::string toString() const
    {
        std::ostringstream expression;

        if (supports_single_ct_trig_cb_) {
            expression << source_counter_triggers_[0]->getTriggerPoint() << ",";
            expression << "counter="
                       << source_counter_triggers_[0]->getCounter()->getLocation();
        } else {
            expression << "'" << original_expression_ << "'";
        }

        return expression.str();
    }

    void setTriggeredNotificationSource(
        const std::shared_ptr<sparta::NotificationSource<std::string>> & on_triggered_notifier)
    {
        on_triggered_notifier_ = on_triggered_notifier;
    }

    std::string getNegatedExpression() const
    {
        if (!expression_can_be_negated_) {
            return "";
        }

        std::ostringstream oss;
        const std::string negated_policy = (policy_ == Policy::ALL) ? " || " : " && ";
        for (size_t idx = 0; idx < source_notification_triggers_.size(); ++idx) {
            oss << source_notification_triggers_[idx]->getNegatedExpression();
            if (idx != source_notification_triggers_.size() - 1) {
                oss << negated_policy;
            }
        }

        return oss.str();
    }

    std::shared_ptr<sparta::trigger::SkippedAnnotatorBase> getSkippedAnnotator() {
        return skipped_annotator_;
    }

    /*!
     * \brief Expression triggers are composed of counter triggers, cycle triggers,
     * time triggers, and notification triggers under the hood.
     */
    struct ExpressionTriggerInternals {
        size_t num_counter_triggers_ = 0;
        size_t num_cycle_triggers_ = 0;
        size_t num_time_triggers_ = 0;
        size_t num_notif_triggers_ = 0;
    };
    const ExpressionTriggerInternals & getInternals();

    /*
     * \brief Helper which splits expressions like these:
     *           "entityA >= 90"
     *           "entityB != 45"
     *
     * Into these:
     *           operands = {"entityA", "90"}; comparison_str = ">="
     *           operands = {"entityB", "45"}; comparison_str = "!="
     *
     * Returns true if the given expression could be parsed, false
     * otherwise. A common reason why the parse would fail is if you
     * had an expression like this:
     *           "entityC *= 400"
     *
     * Which is not valid because "*=" is not one of {==, !=, >=, <=, >, <}
     */
    static bool splitComparisonExpression(
        const std::string & expression,
        std::pair<std::string, std::string> & operands,
        std::string & comparison_str)
    {
        return ExpressionTrigger::splitAroundDelimiter_(
            expression, operands, comparison_str);
    }

    virtual ~ExpressionTrigger();

protected:
    /*!
     * \brief Constructor for subclasses. Note that the expression string passed
     * in will not be parsed for any reason. It is only used to print out the
     * 'on triggered' status message, "The following expression has evaluated to
     * TRUE..."
     */
    ExpressionTrigger(const std::string & name,
                      const SpartaHandler & callback,
                      const std::string & expression) :
        name_(name),
        callback_(callback),
        original_expression_(expression)
    {
        this->pruneExpression_(original_expression_);
    }

    /*!
     * \brief Prune any part of the expression that is understood to be
     * metadata, and not actually part of the expression which controls
     * the trigger condition.
     */
    void pruneExpression_(std::string & expression)
    {
        auto pos = expression.find("->");
        if (pos != std::string::npos) {
            std::string sub_expression = expression.substr(pos+2);
            boost::algorithm::trim(sub_expression);
            if (sub_expression.find("post.") == 0) {
                on_triggered_notif_string_ = sub_expression.substr(5);
            }
            expression = expression.substr(0, pos);
            boost::algorithm::trim(expression);
        }
    }

    /*!
     * \brief Return the original expression that was given to this trigger,
     * before any pruning may have taken place
     */
    const std::string & getOriginalExpression() const {
        return original_expression_;
    }

    /*!
     * \brief Let subclasses specialize parsing routines for more specific
     * trigger expressions, but add their parsed triggers to this base
     * class to work with && and || operations like everyone else.
     */
    void addTimeTrigger_(const uint64_t target_value, const Clock * clk);
    void addCounterTrigger_(const CounterBase * ctr, const uint64_t target_value);
    void addCycleTrigger_(const sparta::Clock * clk, const uint64_t target_value);

    void addContextCounterTrigger_(
        const StatisticDef * stat_def,
        const uint64_t target_value,
        const std::string & calc_func_name = "agg");

    static std::vector<std::string> separateByDelimiter_(
        const std::string & expression,
        const std::string & delim)
    {
        std::vector<std::string> operands;
        boost::algorithm::iter_split(operands, expression, boost::algorithm::first_finder(delim));
        return operands;
    }

private:
    ExpressionTrigger() = delete;
    ExpressionTrigger(const ExpressionTrigger &) = delete;
    ExpressionTrigger & operator=(const ExpressionTrigger &) = delete;

    // Deactivate all of the internal trigger objects that build up
    // our entire trigger expression. This renders this ExpressionTrigger
    // effectively dead, but without invoking our destructor. Called by
    // the ExpiringExpressionTrigger class to overcome valgrind failures.
    void deactivateAllInternals_() {
        for (auto & trig : source_counter_triggers_) {
            // CounterTrigger : public SingleTrigger
            trig->deactivate();
        }

        for (auto & trig : source_notification_triggers_) {
            // ExpressionTrigger::NotificationTrigger (nested class)
            trig->suspend();
        }

        for (auto & trig : statistic_def_triggers_) {
            // StatisticDefTrigger : public ManagedTrigger
            //   -> ManagedTrigger friends ExpressionTrigger class
            trig->deactivate_();
        }

        for (auto & trig : source_subclass_triggers_) {
            // SingleTrigger (base class)
            trig->deactivate();
        }

        for (auto & trig : internal_expression_triggers_) {
            // ExpressionTrigger
            trig->deactivateAllInternals_();
        }

        for (auto & trig : dependent_triggers_) {
            // ExpressionTrigger
            trig->deactivateAllInternals_();
        }

        // Dependent triggers are raw back pointers, not smart
        // pointers. This does not invoke any destructors.
        dependent_triggers_.clear();
    }

    virtual void fillInTriggerInternals_(
        ExpressionTriggerInternals & internals) const {
        (void) internals;
    }

    void buildMultiExpressionTrigger_(const std::string & name,
                                      const std::string & expression,
                                      TreeNode * context)
    {
        auto replace_innermost_sub_expression = [](std::string & original_expression,
                                                   std::string & sub_expression,
                                                   std::string & replaced_with) -> bool {

            const size_t inner_right_paren_idx = original_expression.find(')');
            if (inner_right_paren_idx == std::string::npos) {
                return false;
            }

            size_t inner_left_paren_idx = inner_right_paren_idx;
            while (original_expression[inner_left_paren_idx] != '(') {
                --inner_left_paren_idx;
            }

            if (inner_left_paren_idx == 0 && inner_right_paren_idx == original_expression.size() - 1) {
                return false;
            }

            const size_t sub_expression_length = (inner_right_paren_idx - inner_left_paren_idx) + 1;
            sub_expression = original_expression.substr(inner_left_paren_idx, sub_expression_length);
            if (sub_expression.size() < 3 || sub_expression.size() == original_expression.size()) {
                return false;
            }

            static uint64_t auto_inc_index = 1;
            std::ostringstream uuid_oss;
            uuid_oss << "random_uuid_no_boost_" << auto_inc_index++;
            replaced_with = uuid_oss.str();

            boost::replace_all(original_expression, sub_expression, replaced_with);

            sub_expression = sub_expression.substr(1, sub_expression.size() - 2);
            return true;
        };

        std::string replaced_expression(expression);
        std::string sub_expression;
        std::string replaced_with;

        typedef std::pair<std::string, std::string> ExpressionPlaceholders;
        std::queue<ExpressionPlaceholders> placeholders;

        while (replace_innermost_sub_expression(replaced_expression, sub_expression, replaced_with)) {
            placeholders.push({sub_expression, replaced_with});
        }

        if (placeholders.empty()) {
            throw SpartaException("You may not use && and || in the same trigger "
                                "expression without first grouping terms with "
                                "parentheses, e.g. '(A && B) || C'");
        }

        //The string "replaced_expression" now contains a very simple expression to parse out,
        //built up of other expression triggers *that haven't been created yet*.
        //
        //Say we had an original compound / multi-expression string like this:
        //                     "(A && B) || (C && D)"
        //
        //On each pass, it would look like this:
        //                     - - - - - - - - - - - -       placeholders        = { }
        //                     "X        ||  (C && D)"       placeholders.back() = { "A && B", X }
        //                     "X        ||         Y"       placeholders.back() = { "C && D", Y }
        //
        //So all we have to do is start at the front of the placeholders queue, and do this:
        //      1. Use the placeholder.first as the expression - create a trigger from it
        //      2. Tell that new trigger that its reference tag is placeholder.second
        //
        //Pop the queue, and keep going. Each trigger from the second placeholder onward will
        //keep finding reference triggers they can reuse - because we are making them ourselves
        //just in time!

        SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);

        while (!placeholders.empty()) {
            std::unique_ptr<ExpressionTrigger> trigger(new ExpressionTrigger(
                name, cb, placeholders.front().first, context, report_container_));

            trigger->disableMessages();
            trigger->setReferenceEvent(placeholders.front().second, "internal");
            internal_expression_triggers_.emplace_back(trigger.release());
            placeholders.pop();
        }

        //Any remaining outer parentheses can be removed - if present, the expression is now something like
        //         replaced_expression - '(A && B && C)'
        //which should be treated in the last trigger expression as 'A && B && C', no parentheses
        if (replaced_expression[0] == '(' && replaced_expression[replaced_expression.size() - 1] == ')') {
            replaced_expression = replaced_expression.substr(1, replaced_expression.size() - 2);
        }

        //Whatever is leftover in the "replaced_expression" string is the last to
        //consume these placeholders
        std::unique_ptr<ExpressionTrigger> trigger(new ExpressionTrigger(
            name, cb, replaced_expression, context, report_container_));
        trigger->disableMessages();
        internal_expression_triggers_.emplace_back(trigger.release());

        //Now that the expression has been replaced piece by piece with these
        //intermediate UUID tags, determine how many other triggers this last
        //ExpressionTrigger is waiting on
        auto and_operands = ExpressionTrigger::separateByDelimiter_(replaced_expression, "&&");
        auto or_operands  = ExpressionTrigger::separateByDelimiter_(replaced_expression, "||");

        //If the replaced expression still has && and || in it, the above code
        //would have thrown an exception already:
        //  if (placeholders.empty()) { ... }
        //
        //And note that the expression 'A && B' when split by delimiter '||' would
        //return or_operands.size() == 1. In other words, it returns the string
        //'replaced_expression' right back to you in a vector of size 1, not an
        //empty vector. Same goes for and_operands.size() == 1 if 'A || B' was
        //split by delimiter '&&'.
        sparta_assert(and_operands.size() <= 1 || or_operands.size() <= 1);

        if (and_operands.size() > 1) {
            waiting_on_ = and_operands.size();
        } else {
            waiting_on_ = 1;
        }

        supports_single_ct_trig_cb_ = false;
        expression_can_be_negated_ = false;
    }

    /*!
     * \brief Split apart expressions into smaller pieces - each of which
     * resolves to a trigger object, be it one that we create here and own,
     * or one that we simply reference (some other expression owns it)
     */
    void parseExpression_(const std::string & name,
                          const std::string & expression,
                          TreeNode * context)
    {
        auto and_operands = ExpressionTrigger::separateByDelimiter_(expression, "&&");
        auto or_operands  = ExpressionTrigger::separateByDelimiter_(expression, "||");

        //Expressions such as "A && (B || C)"
        if (and_operands.size() > 1 && or_operands.size() > 1) {
            this->buildMultiExpressionTrigger_(name, expression, context);
        }

        //All other expressions (these do not have any combination of && and || at the same time)
        else {
            std::string no_whitespace(expression);
            boost::erase_all(no_whitespace, " ");

            and_operands = ExpressionTrigger::separateByDelimiter_(no_whitespace, "&&");
            or_operands  = ExpressionTrigger::separateByDelimiter_(no_whitespace, "||");

            std::vector<std::string> operands;
            if (and_operands.size() > 1) {
                operands.swap(and_operands);
                policy_ = Policy::ALL;
            }
            if (or_operands.size() > 1) {
                //Sanity check (duplicate of throw condition above)
                sparta_assert(operands.empty());
                operands.swap(or_operands);
                policy_ = Policy::ANY;
            }

            if (operands.empty()) {
                //There are no &&/|| in this expression... both and/or_operands better be the same!
                sparta_assert(and_operands == or_operands);
                operands = and_operands;

                //Sanity check (split operations MUST mean we have a X&&Y / X||Y situation, which we don't)
                sparta_assert(operands.size() == 1);
            }

            for (const auto & operand : operands) {
                this->addTriggerForExpression_(operand);
            }
        }

        this->populateInvokeCallbackMessageStr_();
    }

    /*!
     * \brief The incoming expression here is going to resolve to exactly
     * one trigger as far as we are concerned:
     *
     *       CounterTrigger       - we create it, and the Scheduler
     *                              hits our notify() method directly
     *
     *       NotificationTrigger  - we create it, and when it receives a
     *                              matching payload on its channel, will
     *                              call our notify() method
     *
     *       Referenced trigger   - this is actually another ExpressionTrigger
     *                            - looked up by <entity.event> (throws if unfound)
     *                            - ExpressionTrigger's have dependent ExpressionTrigger's...
     *                               - when a referenced trigger evaluates to TRUE, it will
     *                                 call its dependents' notify() methods - that's us!
     *
     *       StatisticDef trigger - we create it, and it will reside in the
     *                              TriggerManager singleton until the underlying
     *                              statistic expression evaluates to the target value,
     *                              which will result in our notify() method being called
     */
    void addTriggerForExpression_(const std::string & expression)
    {
        const size_t init_num_source_triggers = waiting_on_;

        //In terms of text parsing, "most specific" to "least specific" (priority
        //of the parser) goes:
        //
        //  Notification trigger   -> "notif.user_channel > 50"
        //
        //  Referenced trigger     -> "t1.stop"
        //                         -> Even though 'this' trigger does not own one, ...
        //                         -> ...expression triggers can notify each other!
        //
        //  StatisticDef trigger   -> "stat_def.core0.dispatch... >= 450"
        //
        //  ContextCounter trigger -> "stat_def.core0.rob.stats...agg < 1500"
        //
        //  Counter trigger        -> "core0.rob.stats.total_number_retired >= 300"
        const bool valid =
            this->tryAddNotificationTrigger_(expression) ||
            this->tryAddReferencedTrigger_(expression) ||
            this->tryAddStatisticDefTrigger_(expression) ||
            this->tryAddContextCounterTrigger_(expression) ||
            this->tryAddCounterTrigger_(expression);

        if (!valid) {
            SpartaException e("The following trigger expression could not be parsed: '");
            e << expression << "'\nPossible Reasons:\n";
            e << "\tLeft hand side is not a NotificationSource\n";
            e << "\tLeft hand side is not a Reference back to an defined expression\n";
            e << "\tLeft hand side is not a StatisticDef\n";
            e << "\tLeft hand side is not a ContextCounter\n";
            e << "\tLeft hand side is not a Counter\n";
            e << "\tLeft hand side is not found in the simulation tree\n";
            e << "\tOther:  Is the trigger expression private?\n";
            throw e;
        }

        ++waiting_on_;

        //Sanity check
        sparta_assert(waiting_on_ == init_num_source_triggers + 1,
                    "One of the ExpressionTrigger::tryAdd*Trigger_() methods "
                    "is adding more than one trigger to this class instance!");
    }

    /*!
     * \brief Add a trigger created by a subclass (the original expression
     * was not parsed by this base class at all)
     */
    void addTrigger_(std::unique_ptr<SingleTrigger> trigger)
    {
        source_subclass_triggers_.emplace_back(trigger.release());
        ++waiting_on_;
    }

    /*!
     * \brief Given an expression such as "notif.user_var_of_interest != 97", parse
     * it into one NotificationTrigger object with:
     *      channel: 'user_var_of_interest'
     *      target:  97
     *      policy:  !=
     *
     * \return TRUE if successful, FALSE otherwise
     */
    bool tryAddNotificationTrigger_(const std::string & expression)
    {
        static const std::string NOTIF_TAG = "notif.";

        if (expression.find(NOTIF_TAG) == 0) {
            std::string sub_expression(expression.substr(NOTIF_TAG.size()));

            std::string comparison_str;
            std::pair<std::string, std::string> operands;
            if(!ExpressionTrigger::splitAroundDelimiter_(sub_expression, operands, comparison_str)) {
                std::ostringstream oss;
                oss << "Unable to parse the following notification: '"
                    << sub_expression << "'. " << std::endl
                    << "Notification expressions should be of the form:\n\t"
                    << "channel operation target  (for example, 'channel_name <= 89')" << std::endl
                    << "where the operation must be one of the following:\n\t";
                for (size_t idx = 0; idx < supported_comparisons_.size(); ++idx) {
                    oss << supported_comparisons_[idx];
                    if (idx != supported_comparisons_.size() - 1) {
                        oss << ", ";
                    }
                }
                oss << std::endl;
                throw SpartaException(oss.str());
            }

            //Given a subexpression of the form e.g. "user_channel > 55"
            //parse out the channel name and its target value
            const std::string channel = operands.first;

            //Since we already saw the "notif." keyword in the expression, failures to
            //parse should throw an immediate exception. Any other "generic" parse failures
            //from the rest of the expression can be confusing
            //
            //   ex.   'notif.my_user_channel != top.core1.params.spelling_mistake'
            //                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            //                              (misspelled or otherwise invalid parameter path)
            //
            //If such an expression gives another parsing function the chance to make sense
            //of it - despite it clearly being a notification expression here - the error can
            //make little sense:
            //
            //   CounterTrigger's only support '>=' since they respond to monotonically increasing
            //   counter values. No other comparison makes sense.
            //
            bool strict_throw = false;

            uint64_t target_value = 0;
            try {
                size_t npos = 0;
                target_value = utils::smartLexicalCast<uint64_t>(operands.second, npos);
                if (npos != std::string::npos) {
                    strict_throw = true;
                }
            } catch(...) {
                auto vv_target = parseParameter_<uint64_t>(
                    context_, operands.second, expression, false);
                if (vv_target.isValid()) {
                    target_value = vv_target.getValue();
                } else {
                    strict_throw = true;
                }
            }

            if (strict_throw) {
                throw SpartaException("The following trigger expression could "
                                    "not be parsed: '") << expression << "'";
            }

            //Do not silence NotificationSource<T> exceptions...
            std::unique_ptr<NotificationTrigger> trigger(
                new NotificationTrigger(channel, target_value, context_));

            //Assign whatever comparison (==, >, etc.) was given in the definition file
            trigger->setComparatorAsString(comparison_str);

            //Valid notification trigger
            trigger->addDependentExpression(this);
            source_notification_triggers_.emplace_back(trigger.release());

            supports_single_ct_trig_cb_ = false;
            return true;
        }
        return false;
    }

    /*!
     * \brief Given an expression such as "t1.start", see if we can resolve
     * it to an existing expression trigger. If so, add ourselves ('this')
     * to that other trigger's list of dependents.
     *
     * \return TRUE if successful, FALSE otherwise
     */
    bool tryAddReferencedTrigger_(const std::string & expression);

    /*!
     * \brief Given an expression such as "core0.rob.stats.total_number_retired >= 900"
     * parse this into a CounterTrigger object with:
     *      path:   'core0.rob.stats.total_number_retired'
     *      target: 900
     *
     * \return TRUE if successful, FALSE otherwise
     */
    bool tryAddCounterTrigger_(const std::string & expression)
    {
        std::string comparison_str;
        std::pair<std::string, std::string> operands;
        if(!ExpressionTrigger::splitAroundDelimiter_(expression, operands, comparison_str)) {
            return false;
        }

        if (comparison_str != ">=") {
            throw SpartaException("CounterTrigger's only support '>=' since they respond to "
                                "monotonically increasing counter values. No other comparison "
                                "makes sense.");
        }

        //Given an expression of the form e.g. "core0.rob.stats.total_number_retired >= 2500"
        //parse out the counter path and its target value (trigger point)

        //Path
        const std::string counter_path = operands.first;
        const CounterBase * ctr = context_->getChildAs<const CounterBase*>(counter_path, false);
        if (!ctr) {
            return false;
        }

        //Target
        uint64_t trigger_point;
        try {
            size_t npos = 0;
            trigger_point = utils::smartLexicalCast<uint64_t>(operands.second, npos);
            if (npos != std::string::npos) {
                return false;
            }
        } catch(...) {
            auto vv_target = parseParameter_<uint64_t>(
                context_, operands.second, expression, false);
            if (vv_target.isValid()) {
                trigger_point = vv_target.getValue();
            } else {
                return false;
            }
        }

        //Valid counter trigger
        SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);
        source_counter_triggers_.emplace_back(
            new CounterTrigger(name_, cb, ctr, trigger_point));
        supports_single_ct_trig_cb_ &= (source_counter_triggers_.size() == 1);
        expression_can_be_negated_ = false;
        return true;
    }

    /*!
     * \brief Given an expression such as:
     *     "stat_def.core0.dispatch.stats.count_insts_per_unit.agg >= 15k"
     *
     * Parse this into a ContextCounterTrigger object with:
     *      statistic def path:   'core0.dispatch.stats.count_insts_per_unit'
     *      internal counter evaluation function name:  'agg'
     *      target: 15k
     *
     * \return TRUE if successful, FALSE otherwise
     */
    bool tryAddContextCounterTrigger_(const std::string & expression)
    {
        static const std::string STAT_DEF_TAG = "stat_def.";

        if (expression.find(STAT_DEF_TAG) == 0) {
            std::string sub_expression(expression.substr(STAT_DEF_TAG.size()));

            std::string comparison_str;
            std::pair<std::string, std::string> operands;
            if(!ExpressionTrigger::splitAroundDelimiter_(sub_expression, operands, comparison_str)) {
                std::ostringstream oss;
                oss << "Unable to parse the following statistic definition expression: '"
                    << sub_expression << "'. " << std::endl
                    << "StatisicDef expressions should be of the form:\n\t"
                    << "stat_def.path.calc_function_name comparison target\n"
                    << "For example:\n\t"
                    << "stat_def.core0.dispatch.stats.count_insts_per_unit.agg      >=     15k\n\t"
                    << "stat_def.[  path to the StatisticDef tree node   ].[func] [comp] [target]\n"
                    << "where the comparison operator must be one of the following:\n\t";
                for (size_t idx = 0; idx < supported_comparisons_.size(); ++idx) {
                    oss << supported_comparisons_[idx];
                    if (idx != supported_comparisons_.size() - 1) {
                        oss << ", ";
                    }
                }
                oss << std::endl;
                throw SpartaException(oss.str());
            }

            //Given a subexpression of the form "core0.dispatch.stats.count_insts_per_unit.agg >= 15k"
            //parse out the stat def path, the internal counter calculation function name, and its
            //target value
            const std::string path_plus_func = operands.first;

            std::vector<std::string> split;
            boost::split(split, path_plus_func, boost::is_any_of("."));

            std::string calc_func_name = "agg";
            if (split.size() > 1) {
                calc_func_name = split.back();
                split.pop_back();
            }

            std::ostringstream oss;
            for (size_t idx = 0; idx < split.size(); ++idx) {
                oss << split[idx];
                if (idx != split.size() - 1) {
                    oss << ".";
                }
            }

            const std::string stat_def_path = oss.str();
            uint64_t target_value = 0;
            bool strict_throw = false;

            try {
                size_t npos = 0;
                target_value = utils::smartLexicalCast<uint64_t>(operands.second, npos);
                if (npos != std::string::npos) {
                    strict_throw = true;
                }
            } catch(...) {
                auto vv_target = parseParameter_<uint64_t>(
                    context_, operands.second, expression, false);
                if (vv_target.isValid()) {
                    target_value = vv_target.getValue();
                } else {
                    strict_throw = true;
                }
            }

            if (strict_throw) {
                throw SpartaException("The following trigger expression could "
                                    "not be parsed: '") << expression << "'";
            }

            auto stat_def = context_->getChildAs<const StatisticDef*>(stat_def_path, false);
            if (stat_def == nullptr) {
                return false;
            }

            //Valid context counter trigger
            SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);
            std::unique_ptr<ContextCounterTrigger> trigger(
                new ContextCounterTrigger(name_, cb, stat_def, target_value, calc_func_name));

            //Assign whatever comparison (==, >, etc.) was given in the definition file
            trigger->setComparatorAsString(comparison_str);
            source_counter_triggers_.emplace_back(trigger.release());
            supports_single_ct_trig_cb_ = false;
            return true;
        }
        return false;
    }

    /*!
     * \brief Given an expression such as:
     *     "stat_def.core0.rob.stats.ReorderBuffer_utilization_weighted_avg >= 16.5"
     *
     * Parse this into a StatisticDefTrigger object with:
     *      statistic def path:   'core0.rob.stats.ReorderBuffer_utilization_weighted_avg'
     *      target: 16.5
     *
     * \return TRUE if successful, FALSE otherwise
     */
    bool tryAddStatisticDefTrigger_(const std::string & expression) {
        static const std::string STAT_DEF_TAG = "stat_def.";

        if (expression.find(STAT_DEF_TAG) == 0) {
            std::string sub_expression(expression.substr(STAT_DEF_TAG.size()));

            std::string comparison_str;
            std::pair<std::string, std::string> operands;
            if(!ExpressionTrigger::splitAroundDelimiter_(sub_expression, operands, comparison_str)) {
                std::ostringstream oss;
                oss << "Unable to parse the following statistic expression: '"
                    << sub_expression << "'. \n"
                    << "StatisticDef expressions should be of the form:\n\t"
                    << "stat_def.<path to StatisticDef> operation target  \n\t(for example, "
                    << "'stat_def.core0.decode.stats.FetchQueue_utilization_count0_probability > 0.13') \n"
                    << "where the operation must be one of the following:\n\t";
                sparta_assert(!supported_comparisons_.empty());
                for (size_t idx = 0; idx < supported_comparisons_.size() - 1; ++idx) {
                    oss << supported_comparisons_[idx] << ", ";
                }
                oss << supported_comparisons_.back() << std::endl;
                throw SpartaException(oss.str());
            }

            //Given a subexpression of the form e.g. "<path to StatisticDef> != 90"
            //parse out the StatisticDef path and its target value
            const std::string stat_def_path = operands.first;

            //Validate the path and get the StatisticDef* from it
            const StatisticDef * stat_def = context_->getChildAs<StatisticDef>(stat_def_path, false);
            if (!stat_def) {
                return false;
            }

            //Parse out the target value, keeping in mind that the target can be
            //a numeric value or a parameter
            size_t npos = 0;
            utils::ValidValue<double> target_value;
            try {
                //First try to resolve the target value as numeric
                target_value = utils::smartLexicalCast<double>(operands.second, npos);
            } catch(...) {
                try {
                    //Try to parse it again as something like '12k' (with units)
                    //  Note that smartLexicalCast parsing of units only works with integral
                    //  base values (12 in the '12k' example here)
                    target_value.clearValid();
                    target_value = utils::smartLexicalCast<uint64_t>(operands.second, npos);
                } catch (...) {
                    //If that still did not work, try to parse the target value as a parameter
                    target_value.clearValid();
                    target_value = parseParameter_<double>(
                        context_, operands.second, expression, false);
                }
            }

            if (!target_value.isValid()) {
                return false;
            }

            SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);

            //Valid StatisticDef trigger
            std::unique_ptr<StatisticDefTrigger> trigger(
                new StatisticDefTrigger(name_, cb, stat_def, target_value));

            //Assign whatever comparison (==, >, etc.) was given in the definition file
            trigger->setComparatorAsString(comparison_str);

            statistic_def_triggers_.emplace_back(trigger.release());
            supports_single_ct_trig_cb_ = false;
            return true;
        }
        return false;
    }

    /*!
     * \brief One of our triggers has just hit. Update class internals and invoke
     * the client's callback based on our policy (&&, ||, no policy).
     */
    void decrement_()
    {
        if (has_fired_) {
            return;
        }

        if (policy_ == Policy::ANY) {
            this->invokeClient_();
            return;
        }

        sparta_assert(waiting_on_ > 0);
        --waiting_on_;
        if (waiting_on_ == 0 && policy_ == Policy::ALL) {
            this->invokeClient_();
        }
    }

    void invokeClient_()
    {
        //Update the "waiting on" variable before invoking the client's
        //callback in case they want to reschedule this trigger for
        //later (keep it alive)
        has_fired_ = true;
        waiting_on_ = 0;

        if (single_ct_trig_callback_.isValid()) {
            auto firing_trigger = this->resolveFiringCounterTriggerForLegacy_();
            sparta_assert(supports_single_ct_trig_cb_);
            sparta_assert(firing_trigger);
            single_ct_trig_callback_.getValue()(firing_trigger);
            //Legacy callbacks may have rescheduled the trigger themselves, so we
            //should not throw later on because "waiting_on_" was out of date
            if (firing_trigger->isActive()) {
                ++waiting_on_;
                has_fired_ = false;
            }
        } else if (string_payload_cb_.isValid()) {
            std::cout << invoke_callback_message_str_;
            string_payload_cb_.getValue().first(string_payload_cb_.getValue().second);
        } else {
            std::cout << invoke_callback_message_str_;
            callback_();
        }

        for (auto & dependent : dependent_triggers_) {
            dependent->notify();
        }

        if (on_triggered_notifier_ != nullptr && !on_triggered_notif_string_.empty()) {
            on_triggered_notifier_->postNotification(on_triggered_notif_string_);
        }
    }

    const CounterTrigger * resolveFiringCounterTriggerForLegacy_() const
    {
        sparta_assert(supports_single_ct_trig_cb_);
        sparta_assert(source_notification_triggers_.empty());
        sparta_assert(source_counter_triggers_.size() == 1);

        auto & trigger = source_counter_triggers_.front();
        sparta_assert(!trigger->isActive());
        return trigger.get();
    }

    static bool splitAroundDelimiter_(
        const std::string & expression,
        std::pair<std::string, std::string> & operands,
        std::string & found_delim)
    {
        std::vector<std::string> operands_vec;
        found_delim.clear();

        for (const auto & comp : supported_comparisons_) {
            operands_vec = ExpressionTrigger::separateByDelimiter_(expression, comp);
            if (operands_vec.size() != 2) {
                continue;
            } else {
                found_delim = comp;
                break;
            }
        }

        if (found_delim.empty()) {
            return false;
        } else {
            operands.first = operands_vec[0];
            operands.second = operands_vec[1];

            //Sanity check
            sparta_assert(std::find(supported_comparisons_.begin(),
                                  supported_comparisons_.end(),
                                  found_delim) != supported_comparisons_.end());
        }
        return true;
    }

    /*!
     * \brief ExpressionTrigger::NotificationTrigger
     * Implements a trigger in terms of a notification source.
     * Listens on a user-provided channel and compares the incoming
     * payloads against a target value. If the comparison passes e.g.
     * "notif.user_channel > 50" then the associated expression trigger
     * will be notified.
     */
    class NotificationTrigger
    {
    public:
        NotificationTrigger(const std::string & channel,
                            const uint64_t target_value,
                            TreeNode * context) :
            target_(target_value),
            context_(context),
            channel_(channel)
        {
            context_->getRoot()->REGISTER_FOR_NOTIFICATION(checkPayload_, uint64_t, channel_);
            registered_ = true;
        }

        ~NotificationTrigger()
        {
            if (registered_) {
                context_->getRoot()->DEREGISTER_FOR_NOTIFICATION(checkPayload_, uint64_t, channel_);
            }
        }

        std::string getNegatedExpression() const
        {
            static auto cases = trigger::getNegatedComparatorMap();
            auto iter = cases.find(comparator_str_);
            sparta_assert(iter != cases.end());

            std::ostringstream oss;
            oss << "notif." << channel_ << " " << iter->second << " " << target_;
            return oss.str();
        }

        void setComparatorAsString(const std::string & comp)
        {
            predicate_ = trigger::createComparator<uint64_t>(comp, target_);
            comparator_str_ = comp;

            //No valid use case for specifying an unrecognized comparison
            if (!predicate_) {
                throw SpartaException("Unrecognized comparison given to a NotificationTrigger: ") << comp;
            }
        }

        void addDependentExpression(ExpressionTrigger * dependent)
        {
            sparta_assert(dependent);
            dependent_triggers_.insert(dependent);
        }

        //Use sets of dependent triggers to guarantee uniqueness -
        //there is no valid use case for notifying the exact same
        //dependent expression more than once that we ('this') have
        //triggered / evaluated to TRUE
        typedef std::set<ExpressionTrigger*> DependentTriggers;

        void stayActive()
        {
            clear_dependent_triggers_on_fire_ = false;
        }

        void suspend()
        {
            suspended_ = true;
        }

        void awaken()
        {
            suspended_ = false;
        }

    private:
        void checkPayload_(const uint64_t & payload)
        {
            if (suspended_) {
                return;
            }

            //If a comparison was never explicitly given, default to ==
            if (predicate_ == nullptr) {
                predicate_ = trigger::createComparator<uint64_t>("==", target_);
                this->checkPayload_(payload);
            }

            //Otherwise, let the comparator let us know if it is time to trigger
            else if(predicate_->eval(payload)) {
                for (auto & expression_trig : dependent_triggers_) {
                    expression_trig->notify();
                }
                if (clear_dependent_triggers_on_fire_) {
                    dependent_triggers_.clear();
                }
            }
        }

        uint64_t target_;
        std::unique_ptr<ComparatorBase<uint64_t>> predicate_;
        std::string comparator_str_;
        DependentTriggers dependent_triggers_;
        bool clear_dependent_triggers_on_fire_ = true;
        bool suspended_ = false;
        TreeNode * context_ = nullptr;
        std::string channel_;
        bool registered_ = false;
    };

    enum Policy {
        ALL,
        ANY
    };

    std::vector<std::unique_ptr<CounterTrigger>> source_counter_triggers_;
    std::vector<std::unique_ptr<NotificationTrigger>> source_notification_triggers_;
    std::vector<std::unique_ptr<StatisticDefTrigger>> statistic_def_triggers_;
    std::vector<std::unique_ptr<SingleTrigger>> source_subclass_triggers_;
    std::vector<std::unique_ptr<ExpressionTrigger>> internal_expression_triggers_;
    NotificationTrigger::DependentTriggers dependent_triggers_;
    utils::ValidValue<ExpressionTriggerInternals> trigger_internals_;

    std::string name_;
    SpartaHandler callback_;

    std::string original_expression_;
    utils::ValidValue<std::string> reference_tag_;
    TreeNode * context_ = nullptr;
    std::shared_ptr<SubContainer> report_container_;
    bool expression_can_be_negated_ = true;
    std::shared_ptr<sparta::trigger::SkippedAnnotatorBase> skipped_annotator_;

    std::string on_triggered_notif_string_;
    std::shared_ptr<sparta::NotificationSource<std::string>> on_triggered_notifier_;

    /*!
     * \brief Two or more expression triggers can have exactly identical expression
     * strings, but different groups of dependent triggers / clients. This method
     * will prevent identical status updates from being printed in these scenarios.
     */
    void populateInvokeCallbackMessageStr_();

    std::string invoke_callback_message_str_;
    typedef std::set<std::string> MessageStrings;

    utils::ValidValue<SingleCounterTrigCallback> single_ct_trig_callback_;
    bool supports_single_ct_trig_cb_ = true;

    sparta::utils::ValidValue<
        std::pair<StringPayloadTrigCallback, std::string>
    > string_payload_cb_;

    Policy policy_ = Policy::ALL;
    size_t waiting_on_ = 0;
    bool has_fired_ = false;
    friend class ExpiringExpressionTrigger;

    //... ==, <, >= ...
    static std::vector<std::string> supported_comparisons_;

    //Allow expression triggers to depend on each other from a unique key "entity.event"
    //  For example -
    //
    //          subreport:
    //            trigger:
    //                tag:   "t0"
    //                start: "core0.rob.stats.total_number_retired >= 1000"
    //                stop:  "simulation_stopped"
    //            core0:
    //              include: simple_stats.yaml
    //
    //          subreport:
    //            trigger:
    //                tag:   "t1"
    //                start: "notif.interesting_statistic <= 42"
    //                stop:  "core1.rob.stats.total_number_retired >= 9999"
    //            core1:
    //              include: simple_stats.yaml
    //
    //          subreport:
    //            trigger:
    //              start: "t0.start && t1.start"
    //              stop:  "t0.stop  || t1.stop "
    //            core*:
    //              include: simple_stats.yaml
    typedef std::map<std::string, ExpressionTrigger*> ReferenceTriggers;
};

/*!
 * \brief ExpressionTrigger subclass specific to TimeTrigger expression parsing
 *      \example
 *          ExpressionTrigger * trigger = new ExpressionTimeTrigger("1500 ns", ...)
 *              // supported units include:      picoseconds  (ps)
 *                                               nanoseconds  (ns)
 *                                               microseconds (us)
 *
 *              // if no units are supplied, the default is nanoseconds
 */
class ExpressionTimeTrigger : public ExpressionTrigger
{
public:
    ExpressionTimeTrigger(const std::string & name,
                          const SpartaHandler & callback,
                          const std::string & expression,
                          TreeNode * context) :
        ExpressionTrigger(name, callback, expression),
        context_(context)
    {
        const bool valid = this->tryAddTimeTrigger_(
            ExpressionTrigger::getOriginalExpression());

        if (!valid) {
            throw SpartaException("The following trigger expression could not be parsed: '") << expression << "'";
        }
    }

    /*!
     * \brief Return the trigger's expression string
     */
    virtual std::string toString() const override final
    {
        std::ostringstream expression;
        expression << (static_cast<double>(target_value_.getValue()) / 1000) << ","
                   << "type=nanoseconds,counter=NS";
        return expression.str();
    }

    ~ExpressionTimeTrigger() {}

private:
    /*!
     * \brief Given an expression such as "1500 ns", parse this into a
     * TimeTrigger object. The only parameter needed is the simulated
     * time in picoseconds:
     *
     *            10 ms ->  10 x 10^9 picoseconds
     *           250 us -> 250 x 10^6 picoseconds
     *           175 ns -> 175 x 10^3 picoseconds
     */
    bool tryAddTimeTrigger_(const std::string & expression)
    {
        auto split = ExpressionTrigger::separateByDelimiter_(expression, " ");
        if (split.empty() || split.size() > 2) {
            return false;
        }

        const std::string value_str = split[0];
        double time_value_double = 0;

        if (split.size() == 2) {
            try {
                time_value_double = boost::lexical_cast<double>(value_str);
            } catch (const boost::bad_lexical_cast &) {
                auto vv_target = parseParameter_<double>(
                    context_, value_str, expression, false);
                if (vv_target.isValid()) {
                    time_value_double = vv_target.getValue();
                } else {
                    return false;
                }
            }

            static const std::map<std::string, uint64_t> exponents = {
                {"us", 6},
                {"ns", 3},
                {"ps", 0}
            };

            const std::string & units_str = split[1];
            auto iter = exponents.find(units_str);
            if (iter == exponents.end()) {
                throw SpartaException("Unrecognized units found in what appeared "
                                    "to be a time-based expression:\n\t'")
                      << expression << "'";
            }

            time_value_double *= pow(10, iter->second);

            if (time_value_double == 0) {
                throw SpartaException("You may not specify a target of 0 in time trigger "
                                    "expressions. Found in expression: '") << expression << "'";
            }
        } else {
            try {
                time_value_double = boost::lexical_cast<double>(value_str);
            } catch (boost::bad_lexical_cast &) {
                return false;
            }

            if (time_value_double == 0) {
                throw SpartaException("You may not specify a target of 0 in time trigger expressions.");
            }

            time_value_double *= 1000;
        }

        uint64_t time_value = static_cast<uint64_t>(time_value_double);
        if (time_value == 0) {
            throw SpartaException(
                "The given expression, '") << expression << "', results in "
                "a zero-picosecond target value. This is incompatible with "
                "SPARTA time triggers, which require a minimum of 1 picosecond.";
        }

        //Valid time trigger
        ExpressionTrigger::addTimeTrigger_(time_value, context_->getClock());
        target_value_ = time_value;

        return true;
    }

    virtual void fillInTriggerInternals_(
        ExpressionTriggerInternals & internals) const override {
        internals.num_time_triggers_ = 1;
    }

    utils::ValidValue<uint64_t> target_value_;
    TreeNode * context_ = nullptr;
};

/*!
 * \brief While the base ExpressionTrigger class does support counter
 * triggers in general, this subclass may be used with some additional
 * trigger properties that are harder to parse out in the one base class.
 *
 *      ExpressionTrigger * trigger = new ExpressionCounterTrigger(
 *          "MyTriggerName",
 *          callback,
 *          "core0.rob.stats.total_number_retired >= 1000",
 *          absolute_offset,
 *          simulation | context);
 *
 *      or
 *
 *      trigger = new ExpressionCounterTrigger(
 *          "MyTriggerName",
 *          callback,
 *          "core0.rob.stats.total_number_retired >= 1500 align",
 *          absolute_offset,
 *          simulation | context);
 *
 *                                                        ^^^^^
 *      or
 *
 *      trigger = new ExpressionCounterTrigger(
 *          "MyTriggerName",
 *          callback,
 *          "core0.rob.stats.total_number_retired >= 1500 noalign",
 *          absolute_offset,
 *          simulation | context);
 *                                                        ^^^^^^^
 */
class ExpressionCounterTrigger : public ExpressionTrigger
{
public:
    ExpressionCounterTrigger(const std::string & name,
                             const SpartaHandler & callback,
                             const std::string & expression,
                             const bool apply_absolute_offset,
                             app::Simulation * sim);

    ExpressionCounterTrigger(const std::string & name,
                             const SpartaHandler & callback,
                             const std::string & expression,
                             const bool apply_absolute_offset,
                             TreeNode * context);

    /*!
     * \brief Return the trigger's expression string
     */
    virtual std::string toString() const override final;

    /*!
     * \brief Return the target value that was first parsed from the
     * provided expression string (target values advance into the future
     * when triggers get rescheduled)
     */
    uint64_t getOriginalTargetValue() const
    {
        return trigger_point_.getValue();
    }

    ~ExpressionCounterTrigger();

private:
    bool tryParseCounterTrigger_(const std::string & expression);

    bool tryParseContextCounterTrigger_(const std::string & expression);

    virtual void fillInTriggerInternals_(
        ExpressionTriggerInternals & internals) const override;

    const bool apply_offset_;
    utils::ValidValue<uint64_t> target_value_;
    utils::ValidValue<uint64_t> trigger_point_;
    const CounterBase * ctr_ = nullptr;
    bool align_ = true;
    app::Simulation * sim_ = nullptr;
    TreeNode * context_ = nullptr;
    utils::ValidValue<std::string> stringized_;
};

/*!
 * \brief The class accepts expression strings in the form of:
 *         "specific.clock.name 1250"
 *              -> trigger callback at every 1250 cycles on clock named "specific.clock.name"
 *              -> this will look for a clock with this name from the:
 *                    ** simulation's root clock, or
 *                    ** context clock
 *                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *                   Depending on the constructor that was called
 *
 *         "1500"
 *              -> trigger callback at every 1500 cycles on the:
 *                   -> simulation's root clock
 *                   -> context clock
 *
 *                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *                   Depending on the constructor that was called
 */
class ExpressionCycleTrigger : public ExpressionTrigger
{
public:
    ExpressionCycleTrigger(const std::string & name,
                           const SpartaHandler & callback,
                           const std::string & expression,
                           app::Simulation * sim);

    ExpressionCycleTrigger(const std::string & name,
                           const SpartaHandler & callback,
                           const std::string & expression,
                           TreeNode * context);

    /*!
     * \brief Return the trigger's expression string
     */
    virtual std::string toString() const override final;

    ~ExpressionCycleTrigger();

private:
    bool tryAddCycleTrigger_(
        const std::string & expression);

    bool createCycleTrigger_(
        const std::string & clock_name,
        const uint64_t target_value);

    virtual void fillInTriggerInternals_(
        ExpressionTriggerInternals & internals) const override;

    utils::ValidValue<uint64_t> target_value_;
    std::string clock_name_;
    app::Simulation * sim_ = nullptr;
    TreeNode * context_ = nullptr;
};

/*!
 * \brief Given a single expression for a trigger's enabled state,
 * call the user's "on enabled callback" and "on disabled callback"
 * at the appropriate times. For example:
 *
 *   void MyClass::whenEnabled_() {
 *       std::cout << "Toggle trigger just got enabled!" << std::endl;
 *   }
 *
 *   void MyClass::whenDisabled_() {
 *       std::cout << "Toggle trigger just got disabled!" << std::endl;
 *   }
 *
 *   void MyClass::init() {
 *       auto rising_edge_callback = CREATE_SPARTA_HANDLER(MyClass, whenEnabled_);
 *       auto falling_edge_callback = CREATE_SPARTA_HANDLER(MyClass, whenDisabled_);
 *
 *       ExpressionToggleTrigger trigger(
 *           "MyToggleTrigger",
 *           "notif.stats_profiler == 1",
 *           rising_edge_callback,
 *           falling_edge_callback,
 *           ...);
 *    }
 *
 * This will result in calls to 'whenEnabled_()' whenever the 'stats_profiler'
 * notification value is equal to 1 (or more specifically, when it goes from anything
 * NOT EQUAL to 1... to 1)
 *
 * And will result in calls to 'whenDisabled_()' whenever the same notification value
 * is NOT EQUAL to 1 (or more specifically, when it goes from EQUAL TO 1... to anything
 * not equal to 1)
 *
 * These are not single-fire callbacks. The listeners for the rising edge and falling
 * edge user callbacks will be kept alive for the entire simulation.
 *
 * The 'enabled expression' has limitations:
 *     1. You may not specify both && and || in the same expression
 *     2. You may only use notification-based operands in the expression
 *
 *     trigger:
 *       whenever: notif.channelA <= 785 && notif.channelB != 404
 *       update-count: ...
 *
 *     trigger:
 *       whenever: notif.channelZ >= 99
 *       update-cycles: ...
 *
 *   These are valid expressions for toggle triggers. The following is not:
 *
 *     trigger:
 *       whenever: core0.rob.stats.total_number_retired >= 1000
 *       update-time: ...
 *
 *   Since it attempts to use a counter-based operand in the 'whenever' expression, or
 *
 *     trigger:
 *       whenever: t0.start || notif.channelQ <= 123
 *       update-count: ...
 *
 *   Since it attempts to use a tagged trigger (t0.start)
 */
class ExpressionToggleTrigger
{
public:
    ExpressionToggleTrigger(
        const std::string & name,
        const std::string & enabled_expression,
        const SpartaHandler & on_enabled_callback,
        const SpartaHandler & on_disabled_callback,
        TreeNode * context,
        const app::SimulationConfiguration * cfg);

    ~ExpressionToggleTrigger();

    const std::string & toString() const;

private:
    void risingEdge_();
    void fallingEdge_();

    std::string name_;
    SpartaHandler on_enabled_callback_;
    SpartaHandler on_disabled_callback_;
    TreeNode * context_ = nullptr;

    std::string current_expression_;
    std::string original_expression_;
    std::string pending_expression_;
    std::unique_ptr<ExpressionTrigger> rising_edge_trigger_;
    std::unique_ptr<ExpressionTrigger> falling_edge_trigger_;
    bool display_trigger_messages_ = false;

    enum class LastTriggeredAction {
        RisingEdge,
        FallingEdge
    };
    utils::ValidValue<LastTriggeredAction> last_action_;
};

inline bool operator==(const ExpressionTrigger::ExpressionTriggerInternals & internals1,
                       const ExpressionTrigger::ExpressionTriggerInternals & internals2)
{
    return internals1.num_counter_triggers_ == internals2.num_counter_triggers_ &&
           internals1.num_cycle_triggers_   == internals2.num_cycle_triggers_   &&
           internals1.num_time_triggers_    == internals2.num_time_triggers_    &&
           internals1.num_notif_triggers_   == internals2.num_notif_triggers_;
}

inline bool operator!=(const ExpressionTrigger::ExpressionTriggerInternals & internals1,
                       const ExpressionTrigger::ExpressionTriggerInternals & internals2)
{
    return !(internals1 == internals2);
}

} // namespace trigger
} // namespace sparta
