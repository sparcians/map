// <StatisticInstance.hpp> -*- C++ -*-

/*!
 * \file StatisticInstance.cpp
 * \brief Contains a StatisticInstance which refers to a StatisticDef or Counter
 * and some local state to compute a value over a specific sample range
 */

#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/report/db/StatInstValueLookup.hpp"
#include "sparta/report/db/StatInstRowIterator.hpp"

namespace sparta
{
    StatisticInstance::StatisticInstance(const StatisticDef* sd,
                                         const CounterBase* ctr,
                                         const ParameterBase* par,
                                         const TreeNode* n,
                                         std::vector<const TreeNode*>* used)
    {
        const StatisticDef* stat_def;
        if(!sd){
            stat_def = dynamic_cast<const StatisticDef*>(n);

            sdef_ = stat_def;
        }else{
            sdef_ = stat_def = sd;
        }
        const CounterBase* counter;
        if(!ctr){
            counter = dynamic_cast<const CounterBase*>(n);
            ctr_ = counter;
        }else{
            ctr_ = counter = ctr;
        }
        const ParameterBase* param;
        if(!par){
            param = dynamic_cast<const ParameterBase*>(n);
            par_ = param;
        }else{
            par_ = param = par;
        }

        // Find the non-null argument
        const TreeNode* node = n;
        if(!node){
            node = sd;
            if(!node){
                node = ctr;
                if(!node){
                    node = par;
                    sparta_assert(node,
                                  "StatisticInstance was constructed with all null arguments. "
                                  "This is not allowed");
                }
            }
        }
        sparta_assert(int(nullptr != sdef_) + int(nullptr != ctr_) + int(nullptr != par_) == 1,
                      "Can only instantiate a StatisticInstance with either a StatisticDef, "
                      "a Counter, or a Parameter of any numeric type. Got Node: \"" << node->getLocation()
                      << "\". This node is not a stat, counter, or parameter.");

        // Get the Scheduler as context
        if(node->getClock()) {
            scheduler_ = n->getClock()->getScheduler();
        }

        if(sdef_){
            node_ref_ = stat_def->getWeakPtr();

            std::vector<const TreeNode*>* local_used_ptr;
            std::vector<const TreeNode*> temp_used;
            if(!used){
                local_used_ptr = &temp_used;
            }else{
                local_used_ptr = used;
            }
            stat_expr_ = sdef_->realizeExpression(*local_used_ptr);
            if(stat_expr_.hasContent() == false){
                throw SpartaException("Cannot construct StatisticInstance based on node ")
                    << stat_def->getLocation() << " because its expression: "
                    << stat_def->getExpression() << " parsed to an empty expression";
            }
            const auto & sub_stats_info = sdef_->getSubStatistics();
            for (auto & sub_stat_creation_info : sub_stats_info) {
                addSubStatistic_(sub_stat_creation_info);
            }
        }else if(ctr_){
            node_ref_ = counter->getWeakPtr();
        }else if(par_){
            node_ref_ = param->getWeakPtr();
        }else{
            // Should not have been able to call constructor without 1 or
            // the 3 args being non-null
            throw SpartaException("Cannot instantiate a StatisticInstance without a statistic "
                                  "definition or counter pointer");
        }

        start();

        sparta_assert(false == node_ref_.expired());
    }

    StatisticInstance::StatisticInstance(std::shared_ptr<StatInstCalculator> & calculator,
                                         std::vector<const TreeNode*>& used) :
        StatisticInstance(nullptr, nullptr, nullptr, calculator->getNode(), &used)
    {
        // Creating SI's using this constructor essentially means that you
        // want to perform your own StatisticDef calculation, the math/logic
        // of which is too complicated or cumbersome to express in a single
        // std::string. Counter and Parameter SI's are simple enough that
        // SPARTA will not let you try to override their SI value calculation.
        // StatisticDef's and their subclasses are the exception.
        sparta_assert(sdef_);
        sparta_assert(ctr_ == nullptr);
        sparta_assert(par_ == nullptr);
        user_calculated_si_value_ = calculator;
    }

    //! \brief Copy Constructor
    StatisticInstance::StatisticInstance(const StatisticInstance& rhp) :
        node_ref_(rhp.node_ref_),
        sdef_(rhp.sdef_),
        ctr_(rhp.ctr_),
        par_(rhp.par_),
        stat_expr_(rhp.stat_expr_),
        start_tick_(rhp.start_tick_),
        end_tick_(rhp.end_tick_),
        scheduler_(rhp.scheduler_),
        initial_(rhp.initial_),
        result_(rhp.result_),
        sub_statistics_(rhp.sub_statistics_),
        user_calculated_si_value_(rhp.user_calculated_si_value_),
        direct_lookup_si_value_(rhp.direct_lookup_si_value_),
        provided_metadata_(rhp.provided_metadata_)
    {
        if (rhp.provided_location_.isValid()) {
            provided_location_ = rhp.provided_location_.getValue();
        }
        if (rhp.provided_description_.isValid()) {
            provided_description_ = rhp.provided_description_.getValue();
        }
        if (rhp.provided_expr_string_.isValid()) {
            provided_expr_string_ = rhp.provided_expr_string_.getValue();
        }
        if (rhp.provided_value_semantic_.isValid()) {
            provided_value_semantic_ = rhp.provided_value_semantic_.getValue();
        }
        if (rhp.provided_visibility_.isValid()) {
            provided_visibility_ = rhp.provided_visibility_.getValue();
        }
        if (rhp.provided_class_.isValid()) {
            provided_class_ = rhp.provided_class_.getValue();
        }
    }

    //! \brief Move Constructor
    StatisticInstance::StatisticInstance(StatisticInstance&& rhp) :
        node_ref_(std::move(rhp.node_ref_)),
        sdef_(rhp.sdef_),
        ctr_(rhp.ctr_),
        par_(rhp.par_),
        stat_expr_(std::move(rhp.stat_expr_)),
        start_tick_(rhp.start_tick_),
        end_tick_(rhp.end_tick_),
        scheduler_(rhp.scheduler_),
        initial_(rhp.initial_),
        result_(rhp.result_),
        sub_statistics_(std::move(rhp.sub_statistics_)),
        user_calculated_si_value_(std::move(rhp.user_calculated_si_value_)),
        direct_lookup_si_value_(std::move(rhp.direct_lookup_si_value_)),
        provided_metadata_(std::move(rhp.provided_metadata_))
    {
        rhp.sdef_ = nullptr;
        rhp.ctr_ = nullptr;
        rhp.par_ = nullptr;
        rhp.result_ = NAN;

        if (rhp.provided_location_.isValid()) {
            provided_location_ = rhp.provided_location_.getValue();
        }
        if (rhp.provided_description_.isValid()) {
            provided_description_ = rhp.provided_description_.getValue();
        }
        if (rhp.provided_expr_string_.isValid()) {
            provided_expr_string_ = rhp.provided_expr_string_.getValue();
        }
        if (rhp.provided_value_semantic_.isValid()) {
            provided_value_semantic_ = rhp.provided_value_semantic_.getValue();
        }
        if (rhp.provided_visibility_.isValid()) {
            provided_visibility_ = rhp.provided_visibility_.getValue();
        }
        if (rhp.provided_class_.isValid()) {
            provided_class_ = rhp.provided_class_.getValue();
        }

        rhp.provided_location_.clearValid();
        rhp.provided_description_.clearValid();
        rhp.provided_expr_string_.clearValid();
        rhp.provided_value_semantic_.clearValid();
        rhp.provided_visibility_.clearValid();
        rhp.provided_class_.clearValid();
    }

    StatisticInstance::StatisticInstance(const std::string & location,
                                         const std::string & description,
                                         const std::string & expression_str,
                                         const StatisticDef::ValueSemantic value_semantic,
                                         const InstrumentationNode::visibility_t visibility,
                                         const InstrumentationNode::class_t cls,
                                         const std::vector<std::pair<std::string, std::string>> & metadata)
    {
        provided_location_ = location;
        provided_description_ = description;
        provided_expr_string_ = expression_str;
        provided_value_semantic_ = value_semantic;
        provided_visibility_ = visibility;
        provided_class_ = cls;
        provided_metadata_ = metadata;
    }

    StatisticInstance::StatisticInstance(
        const std::string & location,
        const std::string & description,
        const std::shared_ptr<StatInstCalculator> & calculator,
        const InstrumentationNode::visibility_t visibility,
        const InstrumentationNode::class_t cls,
        const std::vector<std::pair<std::string, std::string>> & metadata)
    {
        if (!location.empty()) {
            provided_location_ = location;
        }
        if (!description.empty()) {
            provided_description_ = description;
        }
        user_calculated_si_value_ = calculator;
        provided_visibility_ = visibility;
        provided_class_ = cls;
        provided_metadata_ = metadata;
    }

    StatisticInstance& StatisticInstance::operator=(const StatisticInstance& rhp) {
        node_ref_ = rhp.node_ref_;
        sdef_ = rhp.sdef_;
        ctr_ = rhp.ctr_;
        par_ = rhp.par_;

        stat_expr_ = rhp.stat_expr_;
        start_tick_ = rhp.start_tick_;
        end_tick_ = rhp.end_tick_;
        scheduler_ = rhp.scheduler_;
        initial_ = rhp.initial_;
        result_ = rhp.result_;

        sub_statistics_ = rhp.sub_statistics_;
        user_calculated_si_value_ = rhp.user_calculated_si_value_;
        direct_lookup_si_value_ = rhp.direct_lookup_si_value_;
        provided_metadata_ = rhp.provided_metadata_;

        return *this;
    }

    void StatisticInstance::start() {
        sparta_assert(direct_lookup_si_value_ == nullptr,
                      "You cannot call StatisticInstance::start() for an SI "
                      "that was recreated from a SimDB record");

        start_tick_ = getScheduler_()->getElapsedTicks();
        end_tick_ = Scheduler::INDEFINITE;

        if(user_calculated_si_value_){
            initial_.resetValue(user_calculated_si_value_->getCurrentValue());
            result_ = NAN;
            return;
        }

        if(sdef_ != nullptr){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot start() a StatisticInstance referring to a "
                                      "destructed StatisticDef");
            }
            stat_expr_.start();
            initial_.resetValue(0);
        }else if(ctr_){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot start() a StatisticInstance referring to a "
                                      "destructed Counter");
            }
            initial_.resetValue(ctr_->get());
        }else if(par_){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot start() a StatisticInstance referring to a "
                                      "destructed Parameter");
            }
            initial_.resetValue(par_->getDoubleValue());
        }else{
            stat_expr_.start();
        }

        // Clear result value
        result_ = NAN;
    }

    void StatisticInstance::end(){
        sparta_assert(direct_lookup_si_value_ == nullptr,
                      "You cannot call StatisticInstance::end() for an SI "
                      "that was recreated from a SimDB record");

        end_tick_ = getScheduler_()->getElapsedTicks();

        if(sdef_ != nullptr){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot end() a StatisticInstance referring to a "
                                      "destructed StatisticDef");
            }
            stat_expr_.end();
        }else if(ctr_ != nullptr){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot end() a StatisticInstance referring to a "
                                      "destructed Counter");
            }
            // Do nothing to counter
        }else if(par_ != nullptr){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot end() a StatisticInstance referring to a "
                                      "destructed Parameter");
            }
            // Do nothing to Parameter
        }else{
            stat_expr_.end();
        }

        // Recompute result value
        result_ = computeValue_();
    }

    double StatisticInstance::getValue() const {
        if (direct_lookup_si_value_ != nullptr) {
            return computeValue_();
        }

        if(end_tick_ < start_tick_){
            throw ReversedStatisticRange("Range is reversed. End < start");
        }

        if(start_tick_ > getScheduler_()->getElapsedTicks()){
            throw FutureStatisticRange("Range starts in the future at ") << start_tick_;
        }

        double value;
        if(end_tick_ == Scheduler::INDEFINITE){
            // Compute Value
            value = computeValue_();
        }

        else if(end_tick_ > getScheduler_()->getElapsedTicks()){
            // Rang ends in the future - probable because of a checkpoint
            throw FutureStatisticRange("Range ends in the future at ") << end_tick_;
        }

        else {
            // End tick <= current tick. Use pre-computed value because this
            // window ended in the past
            value = result_;
        }

        //Update any snapshot loggers that are listening for these updates
        for (auto & logger : snapshot_loggers_) {
            logger.takeSnapshot(value);
        }
        return value;
    }

    double StatisticInstance::getRawLatest() const
    {
        if(sdef_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            // Evaluate the expression
            return stat_expr_.evaluate();
        }else if(ctr_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            return ctr_->get();
        }else if(par_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            return par_->getDoubleValue();
        }else{
            return stat_expr_.evaluate();
        }

        return NAN;
    }

    bool StatisticInstance::supportsCompression() const {
        if (user_calculated_si_value_) {
            return false;
        }
        if (sdef_) {
            if (node_ref_.expired()) {
                return false;
            }
            return stat_expr_.supportsCompression();
        } else if (ctr_) {
            if (node_ref_.expired()) {
                return false;
            }
            return ctr_->supportsCompression();
        } else if (par_) {
            if (node_ref_.expired()) {
                return false;
            }
            return par_->supportsCompression();
        }

        return stat_expr_.supportsCompression();
    }

    std::string StatisticInstance::stringize(bool show_range,
                                             bool resolve_subexprs) const {
        std::stringstream ss;
        ss << "<Inst of ";

        // Source
        if(sdef_ || ctr_ || par_){
            if(false == node_ref_.expired()){
                ss << node_ref_.lock()->getLocation();
            }else{
                ss << "<destroyed>";
            }
        }else{
            ss << "expression: " << getExpressionString(show_range,
                                                        resolve_subexprs);
        }

        // Range
        if(show_range){
            ss << " [" << start_tick_ << ",";
            if(end_tick_ == Scheduler::INDEFINITE){
                ss << "now";
            }else{
                ss << end_tick_;
            }
            ss << "]";
        }

        // Value
        //! \note Could produce nan, -nan, -inf, +inf, or inf depending on glibc
        ss << " = " << getValue() << ">";
        return ss.str();
    }

    std::string StatisticInstance::getExpressionString(bool show_range,
                                                       bool resolve_subexprs) const {
        if(provided_expr_string_.isValid()) {
            return provided_expr_string_.getValue();
        }
        if(sdef_){
            if(node_ref_.expired() == false){
                // Print the fully rendered expression string instead of the
                // string used to construct the StatisticDef node
                return stat_expr_.stringize(show_range, resolve_subexprs);
                //return sdef_->getExpression(resolve_subexprs);
            }else{
                return "<expired StatisticDef reference>";
            }
        }else if(ctr_){
            if(node_ref_.expired() == false){
                return ctr_->getLocation();
            }else{
                return "<expired Counter reference>";
            }
        }else if(par_){
            if(node_ref_.expired() == false){
                return par_->getLocation();
            }else{
                return "<expired Parameter reference>";
            }
        }else{
            return stat_expr_.stringize(show_range, resolve_subexprs);
        }
    }

    std::string StatisticInstance::getDesc(bool show_stat_node_expressions) const {
        if(provided_description_.isValid()) {
            return provided_description_.getValue();
        }
        if(sdef_){
            if(node_ref_.expired() == false){
                std::string result = sdef_->getDesc();
                if(show_stat_node_expressions){
                    result += " ";
                    result += stat_expr_.stringize(false, // show_range
                                                   true); // result_subexprs;
                }
                return result;
            }else{
                return "<expired StatisticDef reference>";
            }
        }else if(ctr_){
            if(node_ref_.expired() == false){
                return ctr_->getDesc();
            }else{
                return "<expired Counter reference>";
            }
        }else if(par_){
            if(node_ref_.expired() == false){
                return par_->getDesc();
            }else{
                return "<expired Parameter reference>";
            }
        }

        std::string result = "Free Expression: ";
        result += stat_expr_.stringize(false, // show_range
                                       true); // result_subexprs
        return result;
    }

    void StatisticInstance::dump(std::ostream& o, bool show_range) const {
        // Source
        if(false == node_ref_.expired()){
            o << node_ref_.lock()->getLocation() << " # "
              << getExpressionString();
        }else{
            o << "<destroyed>";
        }

        // Range
        if(show_range){
            o << " [" << start_tick_ << ",";
            if(end_tick_ == Scheduler::INDEFINITE){
                o << "now";
            }else{
                o << end_tick_;
            }
            o << "]";
        }

        // Value
        o << " = " << getValue();
    }

    std::string StatisticInstance::getLocation() const {
        if(provided_location_.isValid()) {
            return provided_location_.getValue();
        }
        if(sdef_){
            if(node_ref_.expired() == false){
                return node_ref_.lock()->getLocation();
            }else{
                return "<expired>";
            }
        }else if(ctr_){
            if(node_ref_.expired() == false){
                return node_ref_.lock()->getLocation();
            }else{
                return "<expired>";
            }
        }else if(par_){
            if(node_ref_.expired() == false){
                return node_ref_.lock()->getLocation();
            }else{
                return "<expired>";
            }
        }else{
            return "<expression>";
        }
    }

    StatisticDef::ValueSemantic StatisticInstance::getValueSemantic() const {
        if(provided_value_semantic_.isValid()) {
            return provided_value_semantic_.getValue();
        }
        if(sdef_){
            if(node_ref_.expired() == false){
                return sdef_->getValueSemantic();
            }else{
                return StatisticDef::VS_INVALID;
            }
        }else if(ctr_){
            return StatisticDef::VS_INVALID;
        }else if(par_){
            return StatisticDef::VS_INVALID;
        }else{
            return StatisticDef::VS_INVALID;
        }
    }

    InstrumentationNode::visibility_t StatisticInstance::getVisibility() const {
        if(provided_visibility_.isValid()) {
            return provided_visibility_.getValue();
        }
        if(node_ref_.expired()) {
            return InstrumentationNode::VIS_NORMAL;
        }
        if(sdef_){
            return sdef_->getVisibility();
        }else if(ctr_){
            return ctr_->getVisibility();
        }else if(par_){
            return InstrumentationNode::VIS_NORMAL; // Use normal for parameters for now
        }

        return InstrumentationNode::VIS_NORMAL;
    }

    InstrumentationNode::class_t StatisticInstance::getClass() const {
        if(provided_class_.isValid()) {
            return provided_class_.getValue();
        }
        if(node_ref_.expired()) {
            return InstrumentationNode::DEFAULT_CLASS;
        }
        if(sdef_){
            return sdef_->getClass();
        }else if(ctr_){
            return ctr_->getClass();
        }else if(par_){
            return InstrumentationNode::DEFAULT_CLASS; // Use normal for parameters for now
        }

        return InstrumentationNode::DEFAULT_CLASS;
    }

    void StatisticInstance::getClocks(std::vector<const Clock*>& clocks) const {
        if(sdef_){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot getClocks() on a StatisticInstance refering to "
                                      "an expired TreeNode reference");
            }

            stat_expr_.getClocks(clocks);
        }else if(ctr_){
            if(node_ref_.expired() == true){
                throw SpartaException("Cannot getClocks() on a Counter refering to "
                                      "an expired TreeNode reference");
            }

            const Clock* clk = node_ref_.lock()->getClock();
            if(clk != nullptr){
                clocks.push_back(clk);
            }
        }else{
            stat_expr_.getClocks(clocks);
        }
    }

    void StatisticInstance::setSIValueDirectLookupPlaceholder(
        const std::shared_ptr<sparta::StatInstValueLookup> & direct_lookup)
    {
        direct_lookup_si_value_ = direct_lookup;
    }

    void StatisticInstance::realizeSIValueDirectLookup(
        const StatInstRowIterator & si_row_iterator)
    {
        if (direct_lookup_si_value_ != nullptr) {
            auto realized_lookup = direct_lookup_si_value_->
                realizePlaceholder(si_row_iterator.getRowAccessor());

            sparta_assert(realized_lookup != nullptr);
            direct_lookup_si_value_.reset(realized_lookup);
        }
    }

    bool StatisticInstance::isSIValueDirectLookupValid() const
    {
        if (direct_lookup_si_value_ == nullptr) {
            return false;
        }

        //The following function call throws if this direct
        //lookup object is a placeholders::StatInstValueLookup
        //which has not yet been realized.
        try {
            return direct_lookup_si_value_->isIndexValidForCurrentRow();
        } catch (...) {
        }

        return false;
    }

    double StatisticInstance::computeValue_() const {
        if(user_calculated_si_value_){
            return user_calculated_si_value_->getCurrentValue() - getInitial();
        }
        if(direct_lookup_si_value_){
            return getCurrentValueFromDirectLookup_();
        }
        if(sdef_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            // Evaluate the expression
            return stat_expr_.evaluate();
        }else if(ctr_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            if(ctr_->getBehavior() == CounterBase::COUNT_LATEST){
                return ctr_->get();
            }else{
                // Compute the delta
                return ctr_->get() - getInitial();
            }
        }else if(par_){
            if(node_ref_.expired() == true){
                return NAN;
            }
            return par_->getDoubleValue();
        }else{
            return stat_expr_.evaluate();
        }
    }

    const Scheduler * StatisticInstance::getScheduler_() const {
        if (scheduler_) {
            return scheduler_;
        }

        sparta_assert(false == node_ref_.expired(),
                      "This node has expired and taken the Scheduler with it");

        const Clock * clk = nullptr;
        if (sdef_) {
            clk = sdef_->getClock();
        } else if (ctr_) {
            clk = ctr_->getClock();
        } else if (par_) {
            clk = par_->getClock();
        }
        if (clk) {
            scheduler_ = clk->getScheduler();
        }

        // Should always be able to fall back on singleton scheduler
        sparta_assert(nullptr != scheduler_);
        return scheduler_;
    }

    double StatisticInstance::getCurrentValueFromDirectLookup_() const
    {
        if (direct_lookup_si_value_ == nullptr) {
            throw SpartaException("StatisticInstance asked for its SI ")
                << "value from a null direct-lookup object";
        }

        sparta_assert(getInitial() == 0,
                      "Unexpectedly encountered a StatisticInstance that "
                      "was created from a SimDB record, but whose SI offset "
                      "value (SI::getInitial()) was not zero. This is a bug.");

        //The following function call throws if this direct
        //lookup object is a placeholders::StatInstValueLookup
        //which has not yet been realized.
        try {
            return direct_lookup_si_value_->getCurrentValue();
        } catch (...) {
        }

        return NAN;
    }

}
