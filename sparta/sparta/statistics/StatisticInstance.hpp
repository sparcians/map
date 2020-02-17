// <StatisticInstance> -*- C++ -*-

/*!
 * \file StatisticInstance.hpp
 * \brief Contains a StatisticInstance which refers to a StatisticDef or Counter
 * and some local state to compute a value over a specific sample range
 */

#ifndef __STATISTIC_INSTANCE_H__
#define __STATISTIC_INSTANCE_H__

#include <iostream>
#include <sstream>
#include <math.h>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/Expression.hpp"
#include "sparta/statistics/StatInstCalculator.hpp"
#include "sparta/statistics/dispatch/StatisticSnapshot.hpp"

namespace sparta
{
    //Forward declarations related to SimDB
    class StatInstValueLookup;
    class StatInstRowIterator;

    /*!
     * \brief Exception indicating that the range of a StatisticInstance
     * was reversed when it was accessed (probably caused by checkpointing)
     */
    class ReversedStatisticRange : public SpartaException
    {
    public:

        /*!
         * \brief Construct with a default string.
         * \note Stream insertion operator can be used to add more text to the
         * reason.
         */
        ReversedStatisticRange(const std::string & reason) :
            SpartaException(reason)
        { }

        /*!
         * \brief Destructor
         */
        virtual ~ReversedStatisticRange() noexcept {}
    };

    /*!
     * \brief Exception indicating that the range of a StatisticInstance
     * starts or ends in the future (probably caused by to checkpointing)
     */
    class FutureStatisticRange : public SpartaException
    {
    public:

        /*!
         * \brief Construct with a default string.
         * \note Stream insertion operator can be used to add more text to the
         * reason.
         */
        FutureStatisticRange(const std::string & reason) :
            SpartaException(reason)
        { }

        /*!
         * \brief Destructor
         */
        virtual ~FutureStatisticRange() noexcept {}
    };

    /*!
     * \brief Instance of either a StatisticDef or CounterBase or
     * an Expression. Has a sample window (simulator ticks) over which it will
     * compute the value of the contained expression/counter for that range
     *
     * A StatisticInstance refers to a StatisticDef, CounterBase and uses said
     * definition to determine inputs and evalute it's statistic function.
     *
     * The overhead of distinguishing between StatisticDef and CounterBase is
     * done here, since it is external to simulation and introduces no overhead
     * unless this StatisticInstance is being evaluated
     *
     * Internally, a StatisticInstance will store spapshot values of counters
     * such that it can compute deltas for evaluating the statistic over
     * the desired sample range.
     */
    class StatisticInstance final
    {
        /*!
         * \brief Private Default constructor
         */
        StatisticInstance() :
            sdef_(nullptr),
            ctr_(nullptr),
            par_(nullptr),
            start_tick_(0),
            end_tick_(Scheduler::INDEFINITE),
            initial_(NAN),
            result_(NAN),
            sub_statistics_()
        {
        }

        /*!
         * \brief Private constructor. Exactly one of the pointers contained
         * must be specified
         * \post Starts computation window
         * \param sd StatisticDefinition on which this instance is computed
         * \param ctr CounterBase through which a value is computed.
         * \param param ParameterBase through which  value is computed
         * \param used TreeNodes already in an expression containing this
         * instance
         */
        StatisticInstance(const StatisticDef* sd,
                          const CounterBase* ctr,
                          const ParameterBase* par,
                          const TreeNode* n,
                          std::vector<const TreeNode*>* used) :
            StatisticInstance()
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

    public:

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Construction with a predefined expression
         * \param expr Expression reference to move
         * \note You *must* set the context (setContext) after this
         *       call.  The Expression might or might not know the context for the Scheduler
         */
        StatisticInstance(statistics::expression::Expression&& expr) :
            StatisticInstance()
        {
            stat_expr_ = expr;
        }

        /*!
         * \brief Construct with a StatisticDef or Counter as a TreeNode*
         * \param node Must be an interface to a StatisticDef or a Counter
         */
        StatisticInstance(const TreeNode* node) :
            StatisticInstance(nullptr, nullptr, nullptr, node, nullptr)
        { }

        /*!
         * \brief Construct with a StatisticDef or Counter as a TreeNode*
         * \param node Must be an interface to a StatisticDef or a Counter
         */
        StatisticInstance(const TreeNode* node, std::vector<const TreeNode*>& used) :
            StatisticInstance(nullptr, nullptr, nullptr, node, &used)
        { }

        /*!
         * \brief Construct with a StatInstCalculator function (wrapper around
         * a SpartaHandler)
         * \param calculator Must be a StatInstCalculator with a non-null
         * tree node attached to it (its 'getNode()' method will be evaluated
         * in this constructor)
         */
        StatisticInstance(std::shared_ptr<StatInstCalculator> & calculator,
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
        StatisticInstance(const StatisticInstance& rhp) :
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
        StatisticInstance(StatisticInstance&& rhp) :
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

        /*!
         * \brief Construct a StatisticInstance with its metadata
         * values set directly, as opposed to this SI asking its
         * underlying counter/parameter/etc. for these values.
         */
        StatisticInstance(const std::string & location,
                          const std::string & description,
                          const std::string & expression_str,
                          const StatisticDef::ValueSemantic value_semantic,
                          const InstrumentationNode::visibility_t visibility,
                          const InstrumentationNode::class_t cls,
                          const std::vector<std::pair<std::string, std::string>> & metadata = {}) :
            StatisticInstance()
        {
            provided_location_ = location;
            provided_description_ = description;
            provided_expr_string_ = expression_str;
            provided_value_semantic_ = value_semantic;
            provided_visibility_ = visibility;
            provided_class_ = cls;
            provided_metadata_ = metadata;
        }

        /*!
         * \brief Construct a StatisticInstance with its location and
         * description set directly, along with a StatInstCalculator
         * which can retrieve the SI value on demand from another
         * source (such as a database file).
         */
        StatisticInstance(
            const std::string & location,
            const std::string & description,
            const std::shared_ptr<StatInstCalculator> & calculator,
            const InstrumentationNode::visibility_t visibility = InstrumentationNode::DEFAULT_VISIBILITY,
            const InstrumentationNode::class_t cls = InstrumentationNode::DEFAULT_CLASS,
            const std::vector<std::pair<std::string, std::string>> & metadata = {}) :
            StatisticInstance()
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

        /*!
         * \brief Virtual destructor
         */
        ~StatisticInstance() {;}

        /*!
         * \brief Get this statistic instance's list of pending substatistic information
         * (TreeNode* and stat name), if any
         */
        const std::vector<StatisticDef::PendingSubStatCreationInfo> & getSubStatistics() const {
            return sub_statistics_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \brief Assignment Operator
        StatisticInstance& operator=(const StatisticInstance& rhp) {
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

        //! \name Computation Window
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Start the computation window for this instance.
         * \note This is implicitly called at construction
         * \post Clears cached result values
         * \post Resets any initial values
         * \throw SpartaException if node reference is expired (and there is a
         * node reference)
         */
        void start() {
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

        /*!
         * \brief Ends the window for this instance. Computes and caches the
         * result of the statistic.
         * \note Re-ending (two calls to end at different times withiout a start
         * call between them) IS supported
         * \throw SpartaException if node reference is expired (and there is a
         * node reference)
         */
        void end(){
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

        /*!
         * \brief Returns the time at which this computation window was started.
         * If started multiple times, returns the most recent start tick.
         */
        Scheduler::Tick getStart() const {
            return start_tick_;
        }

        /*!
         * \brief Returns the time at which ths computation window was ended.
         * \brief If ended once or multiple times, returns the most recent
         * ending tick. If never ended, returns Scheduler::INDEFINITE
         */
        Scheduler::Tick getEnd() const {
            return end_tick_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Value
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Tell this statistic to continually accumulate statistic
         * values, always subtracting out the statistic value that was present
         * when the 'start()' method was first called.
         */
        void accumulateStatistic() const {
            initial_.setIsCumulative(true);
            std::vector<const StatisticInstance*> stats_in_expr;
            stat_expr_.getStats(stats_in_expr);
            for (const auto & stat : stats_in_expr) {
                stat->accumulateStatistic();
            }
        }

        /*!
         * \brief During SimDB->report generation, there is
         * a notion of "placeholder" objects which get set
         * on StatisticInstance/Report objects temporarily.
         * These placeholders can be cloned into "realized"
         * versions of themselves later on.
         *
         * This method lets SimDB-recreated Report objects
         * set placeholders this SI will soon use to get
         * SI data values directly from a SimDB blob (not
         * from an actual simulation).
         */
        void setSIValueDirectLookupPlaceholder(
            const std::shared_ptr<sparta::StatInstValueLookup> & direct_lookup);

        /*!
         * \brief Our StatInstValueLookup *placeholder* object
         * needs to bind itself to a StatInstRowIterator object,
         * since these two classes go hand in hand. Now that we're
         * being given the row iterator, we can use it to "realize"
         * our "SI direct value lookup" object now.
         */
        void realizeSIValueDirectLookup(
            const StatInstRowIterator & si_row_iterator);

        /*!
         * \brief If this SI is using a StatInstValueLookup object
         * to get its SI values, ask if this direct-lookup object
         * can be used to get the current SI value.
         */
        bool isSIValueDirectLookupValid() const;

        /*!
         * \brief Returns the value computed for this statistic instance at the
         * current time
         * \return Computed value (current if instance has not been ended and
         * cached if previously ended)
         * \throw ReversedStatisticRange if end tick is less than start tick
         * \throw FutureStatisticRange StatisticRange if the end tick is finite (not
         * Scheduler::INDEFINITE) and it is greater than the current scheduler
         * tick (Scheduler::getCurrentTick)
         */
        double getValue() const {
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

        /*!
         * \brief Returns the initial value of this instance at start_tick_
         */
        double getInitial() const {
            return initial_.getValue();
        }

        /*!
         * \brief Returns the Raw latest value of the this instance for whatever
         * statistic or counter it contains. This could fiffer from getValue()
         * since it disregards the computation window
         */
        double getRawLatest() const {
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

        bool supportsCompression() const {
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

        /*!
         * \brief Renders this StatisticInstance to a string containing
         * computation window, source, and current value.
         * \note This does evaluate the expression
         * \param show_range Should the range be shown in any subexpression
         * nodes.
         * \param resolve_subexprs Should any referenced statistic defs be
         * expanded to their full expressions so that this becomes an expression
         * containing only counters.
         */
        std::string stringize(bool show_range=true,
                              bool resolve_subexprs=true) const {
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

        /*!
         * \brief Returns a string containing the expression that this statistic
         * will evaluate.
         * \note This could be a simple counter identifier or a full arithmetic
         * expression
         * \param show_range Should the range be shown in any subexpression
         * nodes.
         * \param resolve_subexprs Should any referenced statistic defs be
         * expanded to their full expressions so that this becomes an expression
         * containing only counters.
         */
        std::string getExpressionString(bool show_range=true,
                                        bool resolve_subexprs=true) const {
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

        /*!
         * \brief Returns a string that describes the statistic instance
         * If this instance points to a TreeNode, result is that node's
         * description. If it points to a free expression, returns the
         * expression.
         * \param show_stat_node_expressions If true, also shows expressions for
         * nodes which are StatisticDefs
         */
        std::string getDesc(bool show_stat_node_expressions) const {
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

        /*!
         * \brief Renders this StatisticInstance to a string containing
         * computation window, source, and current value.
         * \param o ostream to which this stat instance is rendered
         * \oaram show_range Should range information for this instance be
         * written to \a o?
         */
        void dump(std::ostream& o, bool show_range=false) const {
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

        /*!
         * \brief Allow this statistic instance to emit statistic value
         * snapshots for observation purposes. These loggers are given
         * the current SI value with each call to getValue()
         */
        void addSnapshotLogger(statistics::StatisticSnapshot & snapshot) const
        {
            snapshot_loggers_.emplace_back(snapshot);
        }

        /*!
         * \brief Remove any SI value loggers we may have been given.
         */
        void disableSnapshotLogging() const
        {
            snapshot_loggers_.clear();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Source Data
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Get the location associated with this statistic instance
         *
         * For counters, returns the counter node's location. For statistic
         * defs, returns the stat def node's location. For expression, returns
         * "<expression>". If any referenced node is expired, returns
         * "<expired>".
         */
        std::string getLocation() const {
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

        /*!
         * \brief Gets the statistic value semantic associated with this
         * statistic instance.
         *
         * For counters and expressions, returns StatisticDef::VS_INVALID.
         * For expired node references, returns StatisticDef::VS_INVALID
         */
        StatisticDef::ValueSemantic getValueSemantic() const {
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

        /*!
         * \brief Gets the visibility associated with this
         * statistic instance.
         */
        InstrumentationNode::visibility_t getVisibility() const {
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

        /*!
         * \brief Gets the Class associated with this
         * statistic instance.
         */
        InstrumentationNode::class_t getClass() const {
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

        /*!
         * \brief Give the reporting infrastructure access to all metadata
         * that has been set. The database report writers need this metadata,
         * and others may need it as well.
         */
        const std::vector<std::pair<std::string, std::string>> & getMetadata() const {
            return provided_metadata_;
        }

        /*!
         * \brief Returns the StatisticDef used to compute this statistic
         */
        const StatisticDef* getStatisticDef() {
            return sdef_;
        }
        const StatisticDef* getStatisticDef() const {
            return sdef_;
        }

        /*!
         * \Returns the counter used to compute this statistic
         */
        const CounterBase* getCounter() {
            return ctr_;
        }
        const CounterBase* getCounter() const {
            return ctr_;
        }

        /*!
         * \Returns the parameter used to compute this statistic
         */
        const ParameterBase* getParameter() {
            return par_;
        }
        const ParameterBase* getParameter() const {
            return par_;
        }

        /*!
         * \brief Gets all clocks associated with this Statistic instance (if
         * any) whether it points to a StatisticDef, a Counter or an anonymous
         * Expression.
         * \param clocks Vector of clocks to which all found clocsk will be
         * appended. This vector will not be cleared
         * \throw SpartaException if this StatisticInstance refers to an expired
         * TreeNode.
         */
        void getClocks(std::vector<const Clock*>& clocks) const {
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

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /**
         * \brief Set the context of this StatisticInstance (sets the scheduler) based on a TreeNode
         * \param context The context of this StatisticInstance based on a TreeNode
         */
        void setContext(const TreeNode * context) {
            sparta_assert(nullptr != context->getClock());
            scheduler_ = context->getClock()->getScheduler();
            sparta_assert(nullptr != scheduler_);
        }

        /**
         * \brief Set the Scheduler context of this StatisticInstance
         * \param scheduler The Scheduler this SI should use
         */
        void setContext(const Scheduler * scheduler) {
            scheduler_ = scheduler;
            sparta_assert(nullptr != scheduler_);
        }

    private:

        /*!
         * \brief Computes the value for this statistic.
         * \return Computed value over computation window. If any dependant
         * counters or StatisticDefs have expired, returns NAN.
         * \pre Referenced StatisticDef or Counter must not have been destroyed
         */
        double computeValue_() const {
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

        /*!
         * \brief Ask the StatInstValueLookup object for our current
         * SI value. Throws an exception if the direct-value object
         * is not being used.
         *
         * This does not apply to normal, in-simulation SI's. This
         * supports post-simulation SimDB workflows only. This
         * method is not implemented inline in this header so that
         * SimDB headers aren't included in downstream builds that
         * don't care about it.
         */
        double getCurrentValueFromDirectLookup_() const;

        /*!
         * \brief Append one pending substatistic for future creation (and addition to the
         * appropriate report)
         */
        void addSubStatistic_(const StatisticDef::PendingSubStatCreationInfo & creation_info) const {
            sub_statistics_.emplace_back(creation_info);
        }

        /*!
         * \brief Pointer to TreeNode from which this instance can be computed.
         * Tracked to ensure that this instance does not attempt to access it
         * once expired. This exists
         */
        TreeNode::ConstWeakPtr node_ref_;

        /*!
         * \brief Statistic definition from which this statistic instance
         * compute its value. (nullptr if there is no StatisticDef to
         * reference)
         */
        const StatisticDef* sdef_;

        /*!
         * \brief Counter reference from which this statistic instance will
         * compute its value (nullptr if there is no Counter to reference)
         */
        const CounterBase* ctr_;

        /*!
         * \brief Parameter reference from which this statistic instance will
         * compute its value (nullptr if there is no Parameter to reference)
         */
        const ParameterBase* par_;

        /*!
         * \brief Expression containing the representation of sdef_.
         * If this StatisticInstance refers to a StatisticDef, this will contain
         * the instantiate expression from that stat def. This is not used for
         * Counters. If this StatisticInstance is
         * constructed only with a anonymous Expression, then this will be a
         * copy of that expression
         */
        sparta::statistics::expression::Expression stat_expr_;

        /*!
         * \brief Tick on which this statistic started (exclusive)
         */
        Scheduler::Tick start_tick_;

        /*!
         * \brief Tick on which this statistic ended (inclusive)
         *
         * Is Scheduler::INDEFINITE; if not yet ended
         */
        Scheduler::Tick end_tick_;

        /*!
         * \brief Cached Scheduler object
         */
        mutable const Scheduler * scheduler_ = nullptr;

        /*!
         * \brief Get the Scheduler associated with this StatisticInstance
         */
        const Scheduler * getScheduler_() const {
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

        /*!
         * \brief Helper class which wraps an initial value together
         * with an "is cumulative statistic or not" flag. If this
         * statistic is in accumulating mode, then all but the first
         * call to 'resetValue()' will be ignored.
         */
        class InitialStatValue {
        public:
            InitialStatValue(const double value) :
                is_cumulative_(false),
                initial_(value)
            {
                if (std::isnan(initial_.getValue())) {
                    initial_.clearValid();
                }
            }

            InitialStatValue(const InitialStatValue & rhp) :
                is_cumulative_(rhp.is_cumulative_),
                initial_(rhp.initial_)
            {}

            InitialStatValue & operator=(const InitialStatValue & rhp) {
                is_cumulative_ = rhp.is_cumulative_;
                initial_ = rhp.initial_;
                return *this;
            }

            void setIsCumulative(const bool is_cumulative) {
                is_cumulative_ = is_cumulative;
            }

            double getValue() const {
                return initial_.isValid() ? initial_.getValue() : 0;
            }

            void resetValue(const double initial) {
                if (is_cumulative_) {
                    if (!initial_.isValid()) {
                        initial_ = initial;
                    }
                    return;
                }
                initial_ = initial;
            }

        private:
            bool is_cumulative_ = false;
            utils::ValidValue<double> initial_;
        };

        /*!
         * \brief Initial value at start_tick_
         */
        mutable InitialStatValue initial_;

        /*!
         * \brief Result value (truncated during output if required)
         */
        double result_;

        /*!
         * \brief Snapshot objects who have requested access to statistics values
         */
        mutable std::vector<statistics::StatisticSnapshot> snapshot_loggers_;

        /*!
         * \brief Pending substatistic information (TreeNode* and stat name)
         */
        mutable std::vector<StatisticDef::PendingSubStatCreationInfo> sub_statistics_;

        /*!
         * \brief User-provided callback which generates the stat value
         */
        std::shared_ptr<StatInstCalculator> user_calculated_si_value_;

        /*!
         * \brief SimDB-recreated StatisticInstance's do not get their
         * SI values from CounterBase/ParameterBase/StatisticDef objects
         * like live-simulation SI's do. Those SI's which are created
         * from SimDB records are bound to their SI value blobs using
         * StatInstValueLookup objects. These are lightweight wrappers
         * around a shared vector<double>, who know their individual
         * element index/offsets into that vector.
         */
        std::shared_ptr<sparta::StatInstValueLookup> direct_lookup_si_value_;

        /*!
         * \brief Typically, SI's will defer to their underlying counter/
         * parameter/StatDef for properties like location and description.
         * But some SI's may not have these internal pieces (counters and
         * such) because they are being created outside of a simulation,
         * and outside of a device tree.
         *
         * These member variables are prefixed with "provided_" to mean
         * that they were *provided* these values directly during SI
         * construction.
         */
        utils::ValidValue<std::string> provided_location_;
        utils::ValidValue<std::string> provided_description_;
        utils::ValidValue<std::string> provided_expr_string_;
        utils::ValidValue<StatisticDef::ValueSemantic> provided_value_semantic_;
        utils::ValidValue<InstrumentationNode::visibility_t> provided_visibility_;
        utils::ValidValue<InstrumentationNode::class_t> provided_class_;
        std::vector<std::pair<std::string, std::string>> provided_metadata_;

    }; // class StatisticInstance

    //! \brief StatisticInstance stream operator
    inline std::ostream& operator<< (std::ostream& out,
                                     sparta::StatisticInstance const & si) {
        out << si.stringize();
        return out;
    }

    //! \brief TreeNode stream operator
    inline std::ostream& operator<< (std::ostream& out,
                                     sparta::StatisticInstance const * si) {
        if(nullptr == si){
            out << "null";
        }else{
            out << si->stringize();
        }
        return out;
    }

} // namespace sparta

// __STATISTIC_INSTANCE_H__
#endif
