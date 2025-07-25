// <ExpressionTrigger> -*- C++ -*-

#include "sparta/trigger/ExpressionTrigger.hpp"

#include "sparta/trigger/SkippedAnnotators.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"
#include "sparta/report/SubContainer.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

namespace sparta {
namespace trigger {

ExpressionTrigger::~ExpressionTrigger()
{
}

const ExpressionTrigger::ExpressionTriggerInternals & ExpressionTrigger::getInternals()
{
    if (trigger_internals_.isValid()) {
        return trigger_internals_;
    }

    ExpressionTrigger::ExpressionTriggerInternals internals;
    fillInTriggerInternals_(internals);

    if (internals == ExpressionTrigger::ExpressionTriggerInternals()) {
        internals.num_counter_triggers_ = source_counter_triggers_.size();
        internals.num_notif_triggers_ = source_notification_triggers_.size();
    }

    auto aggregate_internals = [&internals]
        (const ExpressionTrigger::ExpressionTriggerInternals & other)
    {
        internals.num_counter_triggers_ += other.num_counter_triggers_;
        internals.num_cycle_triggers_   += other.num_cycle_triggers_;
        internals.num_time_triggers_    += other.num_time_triggers_;
        internals.num_notif_triggers_   += other.num_notif_triggers_;
    };

    for (auto & internal_exp_trigger : internal_expression_triggers_) {
        aggregate_internals(internal_exp_trigger->getInternals());
    }

    trigger_internals_ = internals;
    return trigger_internals_;
}

void ExpressionTrigger::populateInvokeCallbackMessageStr_()
{
    sparta_assert(!name_.empty());
    sparta_assert(!original_expression_.empty());

    std::ostringstream oss;
    oss << "  [expression] The following expression for event '" << name_
        << "' has evaluated to TRUE:\n\t\t\t*** "
        << original_expression_ << " ***" << std::endl;

    const std::string message = oss.str();

    if (report_container_ != nullptr) {
        SubContainer & sc = *report_container_;
        MessageStrings & msgs = sc.getContentByName<MessageStrings>("messages");

        if (msgs.find(message) == msgs.end()) {
            invoke_callback_message_str_ = message;
            msgs.insert(message);
        }
    }
}

bool ExpressionTrigger::tryAddReferencedTrigger_(const std::string & expression)
{
    if (report_container_ != nullptr) {
        SubContainer & sc = *report_container_;
        if (sc.hasContentNamed("references")) {
            auto & refs = sc.getContentByName<ReferenceTriggers>("references");
            auto found = refs.find(expression);
            if (found != refs.end()) {
                //Attach ourselves to other expression triggers without their knowledge!
                ExpressionTrigger * referenced_trigger = found->second;
                referenced_trigger->dependent_triggers_.insert(this);

                supports_single_ct_trig_cb_ = false;
                expression_can_be_negated_ = false;
                return true;
            }
        }
    }
    return false;
}

void ExpressionTrigger::setReferenceEvent(const std::string & tag,
                                          const std::string & event)
{
    if (!tag.empty() && !event.empty()) {
        sparta_assert(event != "update", "Unsupported - periodic triggers "
                    "cannot be used in other expression triggers");
        reference_tag_ = tag;
        if (event != "internal") {
             reference_tag_.getValue() += std::string(".") + event;
        }

        if (report_container_ != nullptr) {
            SubContainer & sc = *report_container_;
            if (!sc.hasContentNamed("references")) {
                ReferenceTriggers init;
                init[reference_tag_] = this;
                sc.setContentByName("references", init);
            } else {
                auto & refs = sc.getContentByName<ReferenceTriggers>("references");
                refs[reference_tag_] = this;
            }
        }
    }
}

/*!
 * \brief Implementation of ExpressionTrigger::addTimeTrigger_
 */
void ExpressionTrigger::addTimeTrigger_(const uint64_t target_value,
                                        const Clock * clk)
{
    sparta_assert(!this->hasFired());
    SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);

    this->addTrigger_(std::unique_ptr<TimeTrigger>(
        new TimeTrigger(name_, cb, target_value, clk)));

    supports_single_ct_trig_cb_ = false;
    if (waiting_on_ == 1) {
        skipped_annotator_.reset(new trigger::UpdateTimeSkippedAnnotator(clk));
    } else {
        skipped_annotator_.reset();
    }
}

/*!
 * \brief Implementation of ExpressionTrigger::addCounterTrigger_
 */
void ExpressionTrigger::addCounterTrigger_(const CounterBase * ctr, const uint64_t target_value)
{
    sparta_assert(!this->hasFired());
    SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);

    source_counter_triggers_.emplace_back(
        new CounterTrigger(name_, cb, ctr, target_value));

    ++waiting_on_;
    supports_single_ct_trig_cb_ &= (source_counter_triggers_.size() == 1);
    if (waiting_on_ == 1) {
        skipped_annotator_.reset(new trigger::UpdateCountSkippedAnnotator(ctr));
    } else {
        skipped_annotator_.reset();
    }
}

/*!
 * \brief Implementation of ExpressionTrigger::addCycleTrigger_
 */
void ExpressionTrigger::addCycleTrigger_(const sparta::Clock * clk, const uint64_t target_value)
{
    sparta_assert(!this->hasFired());
    SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);

    std::unique_ptr<CycleTrigger> trigger(new CycleTrigger(name_, cb, clk));
    trigger->setRelative(clk, target_value);
    this->addTrigger_(std::move(trigger));

    supports_single_ct_trig_cb_ = false;
    if (waiting_on_ == 1) {
        skipped_annotator_.reset(new trigger::UpdateCyclesSkippedAnnotator(clk));
    } else {
        skipped_annotator_.reset();
    }
}

void ExpressionTrigger::addContextCounterTrigger_(
    const StatisticDef * stat_def,
    const uint64_t target_value,
    const std::string & calc_func_name)
{
    SpartaHandler cb = CREATE_SPARTA_HANDLER(ExpressionTrigger, notify);
    std::unique_ptr<ContextCounterTrigger> trigger(
        new ContextCounterTrigger(name_, cb, stat_def, target_value, calc_func_name));

    //ContextCounter triggers, just like regular CounterTrigger's,
    //use a ">=" comparison operator since their values are always
    //monotonically increasing.
    trigger->setComparatorAsString(">=");

    //Save the trigger for later
    supports_single_ct_trig_cb_ = false;
    source_counter_triggers_.emplace_back(trigger.release());
    ++waiting_on_;
}

/*!
 * \brief While the base ExpressionTrigger class does support counter
 * triggers in general, this subclass may be used with some additional
 * trigger properties that are harder to parse out in the one base class.
 */
ExpressionCounterTrigger::ExpressionCounterTrigger(const std::string & name,
                                                   const SpartaHandler & callback,
                                                   const std::string & expression,
                                                   const bool apply_absolute_offset,
                                                   app::Simulation * sim) :
    ExpressionTrigger(name, callback, expression),
    apply_offset_(apply_absolute_offset),
    sim_(sim)
{
    std::string pruned_expression = expression;
    this->pruneExpression_(pruned_expression);
    if (!this->tryParseCounterTrigger_(pruned_expression) &&
        !this->tryParseContextCounterTrigger_(pruned_expression)) {
        throw SpartaException("The following trigger expression could not be parsed: '") << expression << "'";
    }
}

ExpressionCounterTrigger::ExpressionCounterTrigger(const std::string & name,
                                                   const SpartaHandler & callback,
                                                   const std::string & expression,
                                                   const bool apply_absolute_offset,
                                                   TreeNode * context) :
    ExpressionTrigger(name, callback, expression),
    apply_offset_(apply_absolute_offset),
    context_(context)
{
    std::string pruned_expression = expression;
    this->pruneExpression_(pruned_expression);
    if (!this->tryParseCounterTrigger_(pruned_expression) &&
        !this->tryParseContextCounterTrigger_(pruned_expression)) {
        throw SpartaException("The following trigger expression could not be parsed: '") << expression << "'";
    }
}

ExpressionCounterTrigger::~ExpressionCounterTrigger()
{
}

bool ExpressionCounterTrigger::tryParseContextCounterTrigger_(
    const std::string & full_expression)
{
    //Given an expression of the form e.g.
    //
    //   "core0.dispatch.stats.weighted_count_insts_per_unit 200"
    //   "core0.dispatch.stats.weighted_count_insts_per_unit.weightedAvg_ 200"
    //   "core0.dispatch.stats.weighted_count_insts_per_unit.agg 200 noalign"
    //   etc.
    //
    //parse out the ContextCounter path, the (optional) user-specified
    //aggregation function name, its target value (trigger point), and
    //its alignment setting ('align' by default if not given)

    static const std::string STAT_DEF_KEYWORD = "stat_def.";
    if (full_expression.find(STAT_DEF_KEYWORD) != 0) {
        return false;
    }
    const std::string expression =
        full_expression.substr(STAT_DEF_KEYWORD.size());

    auto split = this->separateByDelimiter_(expression, " ");
    if (split.size() < 2) {
        return false;
    }

    //Local utility for separating a ContextCounter path from its
    //user-specified aggregation function name. This does not validate
    //the resulting split path / function name - it's just a split utility.
    auto separate_stat_def_path_from_agg_fcn_name =
        [](const std::string & path_plus_func) -> std::pair<std::string, std::string>
    {
        std::vector<std::string> vsplit;
        boost::split(vsplit, path_plus_func, boost::is_any_of("."));
        if (vsplit.size() < 2) {
            return std::make_pair(path_plus_func, "");
        }

        const std::string stripped_fcn_name = vsplit.back();
        vsplit.pop_back();

        std::ostringstream oss;
        for (size_t idx = 0; idx < vsplit.size() - 1; ++idx) {
            oss << vsplit[idx] << ".";
        }
        oss << vsplit.back();

        const std::string stripped_path = oss.str();
        return std::make_pair(stripped_path, stripped_fcn_name);
    };

    //In order to create the ContextCounterTrigger, we have
    //to be able to parse the expression given to us into
    //a few parts:
    //
    //The actual StatisticDef* from the path, e.g.
    //   'top.core0.dispatch...'
    //
    //The aggregation function name. This defaults to the
    //"agg" function, which SPARTA provides for all these
    //triggers, but could be overridden by users' yaml
    //to call their own aggregation function, e.g.
    //
    //   'top.core0.dispatch.my.foo.ctx.ctr.average_',
    //                                      ^^^^^^^^
    //                                  (their C++ method
    //                                   name which is
    //                                   registered with
    //                                   the trigger /
    //                                   parser /
    //                                   report engine)
    //
    //The "trigger context" which is just the TreeNode*
    //that corresponds to this reports "pattern" field
    //in its yaml:
    //
    //    pattern:  _global         <-- getRoot()->getSearchScope()
    //    trigger:
    //      update-count:  top...
    // ..
    //    pattern:  top             <-- getRoot()
    //    trigger:
    //      update-count:  core0...
    const StatisticDef * stat_def = nullptr;
    std::string calc_func_name = "agg";
    const TreeNode * trigger_context = nullptr;

    //Path and optional aggregate function name
    const std::string counter_path = split[0];
    if (sim_ != nullptr) {
        stat_def = sim_->getRoot()->getSearchScope()->
            getChildAs<const StatisticDef*>(counter_path, false);

        if (stat_def == nullptr) {
            //If we didn't find the stat, it could have been given to
            //us as something like:
            //
            //  core0.dispatch.stats.weighted_count_insts_per_unit.avg
            //
            //Where ".avg" is the custom aggregate function name, and
            //is not part of the actual StatisticDef path.
            const std::pair<std::string, std::string> psplit =
                separate_stat_def_path_from_agg_fcn_name(counter_path);

            //Let's try to find the StatisticDef* again
            stat_def = sim_->getRoot()->getSearchScope()->
                getChildAs<const StatisticDef*>(psplit.first, false);

            //Store the custom aggregate function name if this is
            //a real StatisticDef (ContextCounter)
            if (stat_def) {
                calc_func_name = psplit.second;
            }
        }

        if (stat_def) {
            trigger_context = sim_->getRoot()->getSearchScope();
        }
    }

    if (stat_def == nullptr && context_ != nullptr) {
        stat_def = context_->getChildAs<const StatisticDef*>(counter_path, false);

        if (stat_def == nullptr) {
            //Same as above - if this path is not a real StatisticDef,
            //try splitting up the path string we got from YAML and
            //see if it was given to us as:
            //  "<ContextCounter path>.<aggregate function name>"
            const std::pair<std::string, std::string> psplit =
                separate_stat_def_path_from_agg_fcn_name(counter_path);

            stat_def = context_->getChildAs<const StatisticDef*>(psplit.first, false);
            if (stat_def) {
                calc_func_name = psplit.second;
            }
        }

        if (stat_def) {
            trigger_context = context_;
        }
    }

    //If we still haven't resolved the StatisticDef*, this is not
    //a valid ContextCounter trigger expression for 'update-count'.
    if (!stat_def) {
        return false;
    }

    utils::ValidValue<uint64_t> trigger_point;
    utils::ValidValue<uint64_t> target_value;
    utils::ValidValue<std::string> location;

    //Target
    try {
        size_t npos = 0;
        trigger_point = utils::smartLexicalCast<uint64_t>(split[1], npos);
        if (npos != std::string::npos) {
            return false;
        }
    } catch(...) {
        auto vv_target = parseParameter_<uint64_t>(
            context_, split[1], expression, false);
        if (vv_target.isValid()) {
            trigger_point = vv_target.getValue();
        } else {
            return false;
        }
    }

    if (trigger_point.getValue() == 0) {
        throw SpartaException(
            "You may not specify a counter delta "
            "of 0. Found in expression: '") << expression << "'";
    }

    //Align
    bool align = true;
    if (split.size() == 3) {
        if (split[2] == "align") {
            align = true;
        } else if (split[2] == "noalign") {
            align = false;
        } else {
            throw SpartaException(
                "A trigger expression was encountered "
                "with an unknown option: '") << split[2]
                << "' (the full trigger expression was '"
                << expression << "')";
        }
    }

    //Expression counter triggers may be created on the fly,
    //when the absolute offset is not easily known by the
    //outside world. Unless we were explicitly told not to
    //add an offset to the initial trigger point (when this
    //update trigger is supposed to hit first), then set the
    //absolute offset to be our ContextCounter's current value.
    //Go through the aggregation function to find this offset
    //value.
    utils::ValidValue<uint64_t> absolute_offset;
    if (apply_offset_) {
        //Look for a user-supplied custom aggregation function
        //that has already been registered. This would have been
        //done using the REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN
        //macro.
        sparta_assert(trigger_context != nullptr);

        //If there is no user-supplied aggregation function,
        //we can still try to get the default aggregate value
        //just by adding up the internal counters' current
        //values.
        utils::ValidValue<uint64_t> raw_sum;
        for (const auto & sub_stat : stat_def->getSubStatistics()) {
            //Get the sub-statistic as a CounterBase. If any of
            //the sub-statistics are *not* CounterBase objects,
            //then this is not a ContextCounter.
            auto tn = dynamic_cast<const CounterBase*>(sub_stat.getNode());
            if (tn == nullptr) {
                raw_sum.clearValid();
                break;
            }

            //We got another CounterBase* sub-statistic. Add their
            //value to the raw sum.
            if (!raw_sum.isValid()) {
                raw_sum = tn->get();
            } else {
                raw_sum += tn->get();
            }
        }

        //If all sub-statistics were CounterBase*, we would have a valid
        //raw sum value at this point.
        if (raw_sum.isValid()) {
            absolute_offset = raw_sum.getValue();
        }
    }

    //If we were asked to apply an offset to the trigger point, but
    //the absolute offset value could not be figured out, then this
    //was not a valid ContextCounter trigger expression.
    if (!absolute_offset.isValid()) {
        return false;
    }

    if (align) {
        target_value = ((absolute_offset / trigger_point) + 1) * trigger_point;
    } else {
        target_value = (absolute_offset + trigger_point);
    }

    //Valid ContextCounter update trigger. Tell the base class
    //to make the trigger and let it drive report updates.
    ExpressionTrigger::addContextCounterTrigger_(
        stat_def, target_value, calc_func_name);

    //Store the toString() metadata for later. This is done here so that we
    //don't have to store the StatisticDef* as a member for all 'update-count'
    //triggers, when it only applies to ContextCounter triggers.
    std::ostringstream stringized;
    stringized << trigger_point.getValue()
               << ",after=" << target_value.getValue()
               << ",type=" << (align ? "aligned_icount" : "icount")
               << ",counter=" << stat_def->getContextLocation();
    stringized_ = stringized.str();

    return true;
}

bool ExpressionCounterTrigger::tryParseCounterTrigger_(const std::string & expression)
{
    //Given an expression of the form e.g.
    //
    //          "core0.rob.stats.total_number_retired 2500"
    //          "core0.rob.stats.total_number_retired 2500 align"
    //          "core0.rob.stats.total_number_retired 2500 noalign"
    //
    //parse out the counter path, its target value (trigger point), and its
    //alignment setting ('align' by default if not given)

    auto split = this->separateByDelimiter_(expression, " ");
    if (split.size() < 2) {
        return false;
    }

    //Path
    const std::string counter_path = split[0];
    if (sim_ != nullptr) {
        ctr_ = sim_->getRoot()->getSearchScope()->getChildAs<const CounterBase*>(counter_path, false);
    }
    if (ctr_ == nullptr && context_ != nullptr) {
        ctr_ = context_->getChildAs<const CounterBase*>(counter_path, false);
    }
    if (!ctr_) {
        return false;
    }

    //Target
    uint64_t trigger_point;
    try {
        size_t npos = 0;
        trigger_point = utils::smartLexicalCast<uint64_t>(split[1], npos);
        if (npos != std::string::npos) {
            return false;
        }
    } catch(...) {
        auto vv_target = parseParameter_<uint64_t>(
            context_, split[1], expression, false);
        if (vv_target.isValid()) {
            trigger_point = vv_target.getValue();
        } else {
            return false;
        }
    }

    if (trigger_point == 0) {
        throw SpartaException(
            "You may not specify a counter delta "
            "of 0. Found in expression: '") << expression << "'";
    }

    trigger_point_ = trigger_point;

    //Align
    if (split.size() == 3) {
        if (split[2] == "align") {
            //no-op
        } else if (split[2] == "noalign") {
            align_ = false;
        } else {
            throw SpartaException(
                "A trigger expression was encountered "
                "with an unknown option: '") << split[2]
                << "' (the full trigger expression was '"
                << expression << "')";
        }
    }

    //Expression counter triggers may be created on the fly,
    //when the absolute offset is not easily known by the
    //outside world. Unless we are explicitly given a non-zero
    //offset in our constructor, set the absolute offset to be
    //our counter's current count.
    uint64_t absolute_offset = 0;
    if (apply_offset_) {
        absolute_offset = ctr_->get();
    }

    //Valid counter trigger
    target_value_ = 0;
    if (align_) {
        target_value_ = ((absolute_offset / trigger_point_) + 1) * trigger_point_;
    } else {
        target_value_ = (absolute_offset + trigger_point_);
    }

    ExpressionTrigger::addCounterTrigger_(ctr_, target_value_.getValue());
    return true;
}

/*!
 * \brief Return the trigger's expression string
 */
std::string ExpressionCounterTrigger::toString() const
{
    if (stringized_.isValid()) {
        return stringized_.getValue();
    }
    std::ostringstream expression;
    expression << trigger_point_.getValue()
               << ",after=" << target_value_.getValue()
               << ",type=" << (align_ ? "aligned_icount" : "icount")
               << ",counter=" << ctr_->getLocation();
    return expression.str();
}

void ExpressionCounterTrigger::fillInTriggerInternals_(
    ExpressionTrigger::ExpressionTriggerInternals & internals) const
{
    internals.num_counter_triggers_ = 1;
}

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
ExpressionCycleTrigger::ExpressionCycleTrigger(const std::string & name,
                                               const SpartaHandler & callback,
                                               const std::string & expression,
                                               app::Simulation * sim) :
    ExpressionTrigger(name, callback, expression),
    sim_(sim)
{
    std::string pruned_expression = expression;
    this->pruneExpression_(pruned_expression);
    if (!this->tryAddCycleTrigger_(pruned_expression)) {
        throw SpartaException("The following trigger expression could not be parsed: '") << expression << "'";
    }
}

ExpressionCycleTrigger::ExpressionCycleTrigger(const std::string & name,
                                               const SpartaHandler & callback,
                                               const std::string & expression,
                                               TreeNode * context) :
    ExpressionTrigger(name, callback, expression),
    context_(context)
{
    std::string pruned_expression = expression;
    this->pruneExpression_(pruned_expression);
    if (!this->tryAddCycleTrigger_(pruned_expression)) {
        throw SpartaException("The following trigger expression could not be parsed: '") << expression << "'";
    }
}

ExpressionCycleTrigger::~ExpressionCycleTrigger()
{
}

/*!
 * \brief Return the trigger's expression string
 */
std::string ExpressionCycleTrigger::toString() const
{
    std::ostringstream expression;
    expression << target_value_.getValue()
               << ",type=cycles"
               << ",counter=" << clock_name_;
    return expression.str();
}

/*!
 * \brief Parse a trigger expression into a concrete CycleTrigger object.
 * Supported expressions are of the form:
 *
 *           CLOCK CYCLE            e.g. "specific_clock_name 200"
 *                 CYCLE            e.g. "                    750"
 */
bool ExpressionCycleTrigger::tryAddCycleTrigger_(const std::string & expression)
{
    auto split = this->separateByDelimiter_(expression, " ");
    if (split.empty() || split.size() > 2) {
       return false;
    }

    auto cycles_string_2_target_value = [this, &expression](
                                           const std::string & str,
                                           utils::ValidValue<uint64_t> & target_value) -> bool {
        target_value = 0;
        try {
            size_t npos = 0;
            target_value = utils::smartLexicalCast<uint64_t>(str, npos);
            return (npos == std::string::npos);
        } catch (...) {
            target_value = parseParameter_<uint64_t>(context_, str, expression, false);
        }

        return target_value.isValid();
    };

    utils::ValidValue<uint64_t> target_value;

    if (split.size() == 1 && !cycles_string_2_target_value(split[0], target_value)) {
        return false;
    } else if (split.size() == 2) {
        clock_name_ = split[0];
        const std::string & cycles_as_string = split[1];
        if (!cycles_string_2_target_value(cycles_as_string, target_value)) {
            return false;
        }
    }

    if (target_value.isValid() && target_value.getValue() == 0) {
        throw SpartaException("You may not specify a cycle delta "
                            "of 0. Found in expression: '") << expression << "'";
    }

    target_value_ = target_value;
    return this->createCycleTrigger_(clock_name_, target_value_.getValue());
}

bool ExpressionCycleTrigger::createCycleTrigger_(
    const std::string & clock_name,
    const uint64_t target_value)
{
    sparta_assert(sim_ != nullptr || context_ != nullptr, "You may not create "
                "ExpressionCycleTrigger's without specifying at least a Simulation "
                "or a TreeNode to go with it. Without either, no clock can be found.");

    sparta::Clock * clk = nullptr;

    if (sim_ != nullptr) {
        clk = sim_->getRootClock();
    }

    if (clk == nullptr) {
        sparta_assert(context_ != nullptr);
        clk = const_cast<sparta::Clock*>(context_->getClock());
    }

    if (clk == nullptr) {
        return false;
    }

    if (!clock_name.empty()) {
        std::vector<TreeNode*> found;
        clk->findChildren(clock_name, found);
        if (found.size() == 1) {
            clk = dynamic_cast<sparta::Clock*>(found[0]);
        } else {
            return false;
        }
    }

    if (clk == nullptr) {
        return false;
    }

    //Valid cycle trigger
    ExpressionTrigger::addCycleTrigger_(clk, target_value);
    return true;
}

void ExpressionCycleTrigger::fillInTriggerInternals_(
    ExpressionTrigger::ExpressionTriggerInternals & internals) const
{
    internals.num_cycle_triggers_ = 1;
}

/*!
 * \brief ExpressionToggleTrigger implementation.
 *
 * Given a single expression for a trigger's enabled state,
 * call the user's "on enabled callback" and "on disabled callback"
 * at the appropriate times.
 */
ExpressionToggleTrigger::ExpressionToggleTrigger(const std::string & name,
                                                 const std::string & enabled_expression,
                                                 const SpartaHandler & on_enabled_callback,
                                                 const SpartaHandler & on_disabled_callback,
                                                 TreeNode * context,
                                                 const app::SimulationConfiguration * cfg) :
    name_(name),
    on_enabled_callback_(on_enabled_callback),
    on_disabled_callback_(on_disabled_callback),
    context_(context),
    current_expression_(enabled_expression),
    original_expression_(enabled_expression)
{
    std::shared_ptr<SubContainer> trigger_container(new SubContainer);

    rising_edge_trigger_.reset(new ExpressionTrigger(
        "InternalToggleEnable",
        CREATE_SPARTA_HANDLER(ExpressionToggleTrigger, risingEdge_),
        enabled_expression,
        context_,
        trigger_container));

    if (cfg) {
        display_trigger_messages_ = cfg->verbose_report_triggers;
    }
    if (!display_trigger_messages_) {
        rising_edge_trigger_->disableMessages();
    }
    pending_expression_ = rising_edge_trigger_->getNegatedExpression();

    if (pending_expression_.empty()) {
        throw SpartaException(
            "This expression is invalid for use with toggle "
            "triggers: '") << enabled_expression << "'";
    }

    falling_edge_trigger_.reset(new ExpressionTrigger(
        "InternalToggleDisable",
        CREATE_SPARTA_HANDLER(ExpressionToggleTrigger, fallingEdge_),
        pending_expression_,
        context_,
        trigger_container));

    if (!display_trigger_messages_) {
        falling_edge_trigger_->disableMessages();
    }

    rising_edge_trigger_->stayActive();
    falling_edge_trigger_->stayActive();
    falling_edge_trigger_->suspend();
}

ExpressionToggleTrigger::~ExpressionToggleTrigger()
{
}

const std::string & ExpressionToggleTrigger::toString() const
{
    return original_expression_;
}

void ExpressionToggleTrigger::risingEdge_()
{
    sparta_assert(!last_action_.isValid() || last_action_ == LastTriggeredAction::FallingEdge);
    last_action_ = LastTriggeredAction::RisingEdge;
    on_enabled_callback_();
    rising_edge_trigger_->suspend();
    falling_edge_trigger_->awaken();
}

void ExpressionToggleTrigger::fallingEdge_()
{
    sparta_assert(!last_action_.isValid() || last_action_ == LastTriggeredAction::RisingEdge);
    last_action_ = LastTriggeredAction::FallingEdge;
    on_disabled_callback_();
    rising_edge_trigger_->awaken();
    falling_edge_trigger_->suspend();
}

} // namespace trigger
} // namespace sparta
