// <StatisticInstance.hpp> -*- C++ -*-

/*!
 * \file StatisticInstance.hpp
 * \brief Contains a StatisticInstance which refers to a StatisticDef or Counter
 * and some local state to compute a value over a specific sample range
 */

#pragma once

#include <iostream>
#include <sstream>
#include <math.h>
#include <utility>

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
        StatisticInstance() = default;

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
                          std::vector<const TreeNode*>* used);

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
            stat_expr_(expr)
        { }

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
                          std::vector<const TreeNode*>& used);

        //! \brief Copy Constructor
        StatisticInstance(const StatisticInstance& rhp);

        //! \brief Move Constructor
        StatisticInstance(StatisticInstance&& rhp);

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
                          const std::vector<std::pair<std::string, std::string>> & metadata = {});

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
            const std::vector<std::pair<std::string, std::string>> & metadata = {});

        /*!
         * \brief Non-Virtual destructor
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
        StatisticInstance& operator=(const StatisticInstance& rhp);

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
        void start();

        /*!
         * \brief Ends the window for this instance. Computes and caches the
         * result of the statistic.
         * \note Re-ending (two calls to end at different times withiout a start
         * call between them) IS supported
         * \throw SpartaException if node reference is expired (and there is a
         * node reference)
         */
        void end();

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
        double getValue() const;

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
        double getRawLatest() const;

        /*!
         * Does this StatisticInstance support compression (database)?
         */
        bool supportsCompression() const;

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
                              bool resolve_subexprs=true) const;

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
                                        bool resolve_subexprs=true) const;

        /*!
         * \brief Returns a string that describes the statistic instance
         * If this instance points to a TreeNode, result is that node's
         * description. If it points to a free expression, returns the
         * expression.
         * \param show_stat_node_expressions If true, also shows expressions for
         * nodes which are StatisticDefs
         */
        std::string getDesc(bool show_stat_node_expressions) const;

        /*!
         * \brief Renders this StatisticInstance to a string containing
         * computation window, source, and current value.
         * \param o ostream to which this stat instance is rendered
         * \oaram show_range Should range information for this instance be
         * written to \a o?
         */
        void dump(std::ostream& o, bool show_range=false) const;

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
        std::string getLocation() const;

        /*!
         * \brief Gets the statistic value semantic associated with this
         * statistic instance.
         *
         * For counters and expressions, returns StatisticDef::VS_INVALID.
         * For expired node references, returns StatisticDef::VS_INVALID
         */
        StatisticDef::ValueSemantic getValueSemantic() const;

        /*!
         * \brief Gets the visibility associated with this
         * statistic instance.
         */
        InstrumentationNode::visibility_t getVisibility() const;

        /*!
         * \brief Gets the Class associated with this
         * statistic instance.
         */
        InstrumentationNode::class_t getClass() const;

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
        void getClocks(std::vector<const Clock*>& clocks) const;

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
        double computeValue_() const;

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
        const StatisticDef* sdef_ = nullptr;

        /*!
         * \brief Counter reference from which this statistic instance will
         * compute its value (nullptr if there is no Counter to reference)
         */
        const CounterBase* ctr_ = nullptr;

        /*!
         * \brief Parameter reference from which this statistic instance will
         * compute its value (nullptr if there is no Parameter to reference)
         */
        const ParameterBase* par_ = nullptr;

        /*!
         * \brief Expression containing the representation of sdef_.
         * If this StatisticInstance refers to a StatisticDef, this
         * will contain the instantiated expression from that stat
         * def. This is not used for Counters. If this
         * StatisticInstance is constructed only with a anonymous
         * Expression, then this will be a copy of that expression
         */
        sparta::statistics::expression::Expression stat_expr_;

        /*!
         * \brief Tick on which this statistic started (exclusive)
         */
        Scheduler::Tick start_tick_{0};

        /*!
         * \brief Tick on which this statistic ended (inclusive)
         *
         * Is Scheduler::INDEFINITE; if not yet ended
         */
        Scheduler::Tick end_tick_{Scheduler::INDEFINITE};

        /*!
         * \brief Cached Scheduler object
         */
        mutable const Scheduler * scheduler_ = nullptr;

        /*!
         * \brief Get the Scheduler associated with this StatisticInstance
         */
        const Scheduler * getScheduler_() const;

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
        mutable InitialStatValue initial_{NAN};

        /*!
         * \brief Result value (truncated during output if required)
         */
        double result_{NAN};

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
