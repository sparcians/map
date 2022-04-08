// <Report> -*- C++ -*-

/*!
 * \file Report.hpp
 * \brief Part of the metrics and statistics system.
 * Contains a Report class which refers to a number of StatisticInstance
 * instances of other Reports to present a set of associated simuation metrics
 */

#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cmath>

#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "simdb_fwd.hpp"

namespace sparta
{
    class SubContainer;

    namespace report {
        namespace format {
            class ReportHeader;
        }
    }

    class StatInstRowIterator;
    namespace db {
        class DatabaseContextCounter;
    }

    /*!
     * \brief Collection of optionally-named StatisticInstances and other
     * (sub)reports.
     *
     * Adding items to a report should be as easy as possible. Report should
     * accept items of all possible types and indirections
     *
     * Reports do not share items. Items can be copied from reports, but no
     * report will depend on any other report's items
     *
     * Contents can be identified and retrieved by name (key)
     */
    class Report
    {
    public:

        /*!
         * \brief Function pointer for deciding whether to make a subreport
         * during recursive addition of statistics/counters from a subtree
         * \param n Context node for which a subreport could be created
         * \param sr_name [out] Name of subreport, if one is being created
         * \param child_report [out] Should a child report or a sibling be
         * created (if one is being created)
         * \param report_depth Current report hierarchy depth
         * \return true if a subreport should be created. sr_name and
         * child_report must also be set if true. False if nothing should be
         * done
         */
        typedef std::function<bool(const TreeNode* n,
                                   std::string& sr_name,
                                   bool& child_report,
                                   uint32_t report_depth)> subreport_decision_fxn_t;

        /*!
         * \brief Function pointer for deciding whether to include a node during
         * recursive addition of statistics/counters from a subtree
         */
        //typedef bool (*inclusion_decision_fxn_t)(const TreeNode* n);
        typedef std::function<bool(const TreeNode*)> inclusion_decision_fxn_t;

        /*!
         * \brief Cleanly format a number. If it is an integer, print as an
         * integer. If it has a decimal portion, print as a floating point
         * number. If nan, print as nan (regardless of sign). If inf, use C++
         * std::ostream to stringize it. Note that this prevents scientific
         * notation for integers only
         * \param val Value to display
         * \param float_scinot_allowed Display scientific notiation for floats
         * (non integral values) when stringstream deems it necessary. If false,
         * scientific notation will not be used
         * \param decimal_places Number of decimal places to use if the output
         * is a float. If < 0, uses default state of stringstream for decimal
         * place count
         */
        static std::string formatNumber(double val,
                                        bool float_scinot_allowed=true,
                                        int32_t decimal_places=-1)
        {
            std::stringstream o;
            double integral;
            double fractional = std::modf(val, &integral);
            if(std::isnan(val)){
                return "nan";
            }else if(std::isinf(val)){
                o << val; // Use built-in conversion (e.g. +inf, -inf)
            }else if(fractional == 0){
                if(val < 0) {
                    // Print as a straight integer, no decimals
                    o << (int64_t)integral;
                }
                else {
                    o << (uint64_t) integral;
                }
            }else{
                if(decimal_places >= 0){
                    o << std::setprecision(static_cast<uint32_t>(decimal_places));
                }
                if(float_scinot_allowed){
                    o << val;
                }else{
                    std::ios_base::fmtflags old = o.flags(); // Store original format
                    o << std::fixed << val;
                    o.setf(old);
                }
            }
            return o.str();
        }

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Default constructor with no name or context
         */
        Report() :
            Report("", nullptr, nullptr)
        {
            // Delegated constructor
        }

        /*!
         * \brief Basic constructor
         * \param name Name of report
         */
        explicit Report(const std::string& name) :
            Report(name, nullptr, nullptr)
        {
            // Delegated constructor
        }

        /*!
         * \brief Basic constructor with context node
         * \param name Name of report
         * \param context Node from which a relative search will be performed
         * when new items areadded to this report using a a node location string
         * Often, this is a RootTreeNode.
         */
        Report(const std::string& name, TreeNode* context) :
            Report(name, context, nullptr)
        { }

        /*!
         * \brief Basic constructor with context node
         * \param name Name of report
         * \param scheduler The scheduler associated with this report
         *
         * This contructor can be used with Reports that just provide
         * SI outputs for verification.
         */
        Report(const std::string& name, const Scheduler * scheduler) :
            Report(name, nullptr, scheduler)
        { }

        /*!
         * \brief Basic constructor with context node and scheduler
         * \param name Name of report
         * \param context Node from which a relative search will be performed
         * when new items areadded to this report using a a node location string
         * Often, this is a RootTreeNode.
         */
        Report(const std::string& name, TreeNode* context, const Scheduler* scheduler) :
            scheduler_(scheduler),
            name_(name),
            author_(""),
            context_(context),
            parent_(nullptr),
            start_tick_(0),
            end_tick_(Scheduler::INDEFINITE)
        {
            if(!scheduler_ && context){
                setContext(context);
            }
        }

        /*!
         * \brief Copy constructor. This a deep copy
         * Note that the parent link is lost and parent of new report
         * nullptr. Parent reset through setParent_
         */
        Report(const Report& rhp) :
            scheduler_(rhp.scheduler_),
            name_(rhp.name_),
            author_(rhp.author_),
            style_(rhp.style_),
            context_(rhp.context_),
            parent_(rhp.parent_),
            subreps_(rhp.subreps_),
            start_tick_(rhp.start_tick_),
            end_tick_(rhp.end_tick_),
            info_string_(rhp.info_string_),
            sub_statistics_(rhp.sub_statistics_),
            si_row_iterator_(rhp.si_row_iterator_),
            report_node_id_(rhp.report_node_id_),
            si_node_ids_(rhp.si_node_ids_)
        {
            // Update parent pointers of all subreports
            for(auto& sr : subreps_){
                sr.setParent_(this);
            }

            // Copy StatisticInstances
            for(const statistics::stat_pair_t& sp : rhp.stats_){
                add(sp.second, sp.first);
            }
        }

        /*!
         * \brief Move Constructor
         * Note that the parent link is copied
         */
        Report(Report&& rhp) :
            scheduler_(rhp.scheduler_),
            name_(std::move(rhp.name_)),
            author_(std::move(rhp.author_)),
            style_(std::move(rhp.style_)),
            context_(std::move(rhp.context_)),
            parent_(rhp.parent_),
            subreps_(std::move(rhp.subreps_)),
            stats_(std::move(rhp.stats_)),
            start_tick_(rhp.start_tick_),
            end_tick_(rhp.end_tick_),
            info_string_(std::move(rhp.info_string_)),
            sub_statistics_(std::move(rhp.sub_statistics_)),
            si_row_iterator_(std::move(rhp.si_row_iterator_)),
            report_node_id_(rhp.report_node_id_),
            si_node_ids_(std::move(rhp.si_node_ids_))
        {
            rhp.report_node_id_ = 0;
            // Update parent pointers of all subreports
            for(auto& sr : subreps_){
                sr.setParent_(this);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Assignment operator
         * \note Parent link for this report remains the same
         */
        Report& operator=(const Report& rhp) {
            scheduler_ = rhp.scheduler_;
            name_ = rhp.name_;
            author_ = rhp.author_;
            style_ = rhp.style_;
            context_ = rhp.context_;
            subreps_ = rhp.subreps_;

            // Update parent pointers of all subreports
            for(auto& sr : subreps_){
                sr.setParent_(this);
            }

            stats_.clear(); // Clear local stats

            // Copy StatisticInstances
            for(const statistics::stat_pair_t& sp : rhp.stats_){
                add(sp.second, sp.first);
            }
            start_tick_ = rhp.start_tick_;
            end_tick_ = rhp.end_tick_;
            info_string_ = rhp.info_string_;
            sub_statistics_ = rhp.sub_statistics_;
            si_row_iterator_ = rhp.si_row_iterator_;
            report_node_id_ = rhp.report_node_id_;
            si_node_ids_ = rhp.si_node_ids_;
            return *this;
        }

        //! \name Content Population
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Sets the context node for any method in this Report that
         *        performs a lookup by node path.
         * \note Typically, this should be a sparta::RootTreeNode, but it can be
         *       anything - even nullptr.
         * \note If a context is being set, that context *must* include a scheduler
         * \note The start_tick will be set using the context provided
         * \see getContext
         */
        void setContext(TreeNode* n)
        {
            context_ = n;
            scheduler_ = context_->getScheduler();
            sparta_assert(scheduler_ != nullptr);
            start_tick_ = scheduler_->getElapsedTicks();
        }

        /*!
         * \brief Gets the current context of this node, if any
         * \see setContext
         */
        TreeNode* getContext() const {
            return context_;
        }

        /*!
         * \brief Helper class which allows chained function calls for adding
         * items to a report.
         *
         * Example
         * \code
         * Report r("my_report", &root);
         * r.add("core0.stats.s1") \
         *      (root.getChild("core0.stats.s2")) \
         *      ("core0.stats.s3")
         *      ;
         * \endcode
         */
        class StatAdder {

            Report& r_; //!< Report on which this helper class operates

        public:

            //! \brief No default construction
            StatAdder() = delete;

            //! \brief Not copy-constructable
            StatAdder(const StatAdder&) = delete;

            /*!
             * \brief Move constructor
             */
            StatAdder(StatAdder&& rhp) :
                r_(rhp.r_)
            { }

            /*!
             * \brief Basic constructor
             * \param r Report which this helper class will operate
             */
            explicit StatAdder(Report& r) :
                r_(r)
            { }

            StatAdder& operator()(const StatisticInstance& si,
                                  const std::string& name="") {
                r_.add(si, name);
                return *this;
            }
            StatAdder& operator()(StatisticInstance&& si,
                                  const std::string& name="") {
                r_.add(si, name);
                return *this;
            }
            StatAdder& operator()(StatisticDef* sd,
                                  const std::string& name="") {
                r_.add(sd, name);
                return *this;
            }
            StatAdder& operator()(CounterBase* ctr,
                                  const std::string& name="") {
                r_.add(ctr, name);
                return *this;
            }
            StatAdder& operator()(TreeNode* n,
                                  const std::string& name="") {
                r_.add(n, name);
                return *this;
            }
            StatAdder& operator()(const std::string& expression) {
                r_.add(expression);
                return *this;
            }
        };

        /*!
         * \brief Adds a copy of an existing statistic instance to this Report
         * \param si const Reference to existing StatisticInstance to copy
         * \param name Name to give this instance within this Report.
         * If "" (default) does not give this instance a name.
         * \return StatAdder so that multiple calls can be chained following
         * an add.
         * \throw SpartaException if the given name is not "" and is already used
         * by another item immediately in this report (not the name of a
         * subreport or item in a subreport)
         */
        StatAdder add(const StatisticInstance& si, const std::string& name="");

        /*!
         * \brief Moves an existing statistic instance into this Report
         * \param si rvalue reference to existing StatisticInstance to move
         * \param name Name to give this instance within this Report.
         * If "" (default) does not give this instance a name.
         * \return StatAdder so that multiple calls can be chained following
         * an add.
         * \throw SpartaException if the given name is not "" and is already used
         * by another item immediately in this report (not the name of a
         * subreport or item in a subreport)
         */
        StatAdder add(StatisticInstance&& si, const std::string& name="");

        /*!
         * \brief Add a StatisticDef to the report
         * \param sd Pointer to the StatisticDef
         * \param name Name of this stat
         */
        StatAdder add(StatisticDef* sd, const std::string& name="");

        /*!
         * \brief Add a Counter type to the report
         * \param ctr The counter to add
         * \param name The name of the counter
         */
        StatAdder add(CounterBase* ctr, const std::string& name="");

        /*!
         * \brief Add a TreeNode type that represents a counter/stat derivative
         * \param n The TreeNode to add
         * \param name The name of the item in the report
         */
        StatAdder add(TreeNode* n, const std::string& name="");

        /*!
         * \brief Add sub statisitc
         * \param n The sub statistic to add
         * \param name The name of the stat for the report
         */
        StatAdder addSubStats(StatisticDef * n, const std::string & name_prefix);

        /*!
         * \brief Add a single Expression parsed at the current context for this
         * Report. If the expression resolves to a single TreeNode, adds an
         * entry for that TreeNode. This matches the way Report leaves are
         * handled
         * \param expression Expression to parse
         * \note to Add multiple nodes using a wildcard expression,
         * TreeNode::findChildren must be used with the add function accepting a
         * vector of TreeNodes
         * \throw SpartaException if this Report currently has no context node
         * (see getContext). Throws if the expression could not be evaluated
         */
        StatAdder add(const std::string& expression, const std::string& name="");

        /*!
         * \brief Adds any number of TreeNode items to this Report. Type of node
         * is determined dynamically.
         * \param nv Vector of TreeNodes which are either Counters or
         * StatisticDefs.
         * \throw SpartaException if any item in \a nv is not a Counter or
         * StatisticDef.
         * \return StatAdder helper object for performing chained function calls
         * to add items
         * \warning If this method throws, only some items specified by the
         * pattern. There is no rollback of partial completion
         */
        StatAdder add(const std::vector<TreeNode*>& nv);

        /*!
         * \brief By default, statistics reset their internal offsets whenever
         * a report update is captured. However, some report formats support
         * statistics that never reset their internal offset value on report
         * update, and instead always substract the statistic value that was
         * present at the time of report start.
         */
        void accumulateStats() const;

        /*!
         * \brief Tell this report if ContextCounter stats should be auto-
         * expanded or not (disabled by default).
         */
        void enableContextCounterStatsAutoExpansion() {
            auto_expand_context_counter_stats_ = true;
        }

        /*!
         * \brief Returns enabled/disabled state of ContextCounter stats
         * auto-expansion.
         */
        bool isContextCounterStatsAutoExpansionEnabled() const {
            return auto_expand_context_counter_stats_;
        }

        /*!
         * \brief Consume a YAML file at the given path and add its content
         * \param file_path relative (to cwd) or absolute path to the file
         * \param verbose Show verbose output during file parsing
         * \throw SpartaException if file does not exist, is a malformed Report
         * definition, refers to TreeNode locations that do not exist, or
         * this Report has no context
         * \pre Report must have a context set.
         * \see setContext
         * \see getContext
         */
        void addFile(const std::string& file_path, bool verbose=false);

        /*!
         * \brief Consume a string as if it were a yaml file at the given path
         * and add its content
         * \param content string containing a YAML deport definition
         * \param verbose Show verbose output during file parsing
         * \throw SpartaException if file does not exist, is a malformed Report
         * definition, refers to TreeNode locations that do not exist, or
         * this Report has no context
         * \pre Report must have a context set.
         * \see setContext
         * \see getContext
         */
        void addDefinitionString(const std::string& content, bool verbose=false);

        /*!
         * \brief Adds a subtree of counters and/or statistics to the report
         * \param n Subtree Node
         * \param make_sr_fxn Function which decides when a subreport
         * should be added. If not nullpt, places the name of the subreport in
         * the sr_name argument and returns true if a subreport should be
         * created at the given node \a n.
         * \param branch_inc_fxn Function which decides whether to recurs into
         * the children of a branch node (any node having children). If nullptr,
         * all branhc nodes are considered.
         * \param leaf_inc_fx. Function which decides whether to consider a
         * "leaf" node (counter, stat, etc [regarless of whether these nodes
         * actually have children]). If nullptr, all "leaf" nodes are considered
         * \param add_counters Should Counters be added to the report
         * \param add_stats Should StatisticDefs be added to the report
         * \param max_recurs_depth Maximum recursion depth. If -1, has no
         * effect. If 0, only immediate stats/counters in node \a n are
         * included. Otherwise, at most \a max_recurs_depth levels of ancestor
         * are searched for appropriate counters/stats to add to this report
         */
        void addSubtree(const TreeNode* n,
                        subreport_decision_fxn_t make_sr_fxn,
                        inclusion_decision_fxn_t branch_inc_fxn,
                        inclusion_decision_fxn_t leaf_inc_fxn,
                        bool add_counters,
                        bool add_stats,
                        int32_t max_recurs_depth);

        /*!
         * \brief Autopopulates a hierarchical report using addSubtree and
         * generated internal callbacks.
         * \param n Node at which to start the population. This node is added to
         * the report if it is valid as a statistic instance, but it cannot
         * create a subreport
         * \param attribute_expr Expression of the attributes to accept. If "",
         * does not perform filtering based on node attributes
         * \param captures Vector of captures from regex-like pattern mathing
         * captures earlier in the report (or command-line)
         * \param max_recurs_depth Maximum recursion depth. At node = n,
         * recursion depth is considered 0,
         * \param max_report_depth Maximum report depth At entry of this
         * function, report depth is considered 0.
         * \param max_report_depth Maximum report depth
         */
        void autoPopulate(const TreeNode* n,
                          const std::string& attribute_expr,
                          const std::vector<std::string>& captures,
                          int32_t max_recurs_depth=-1,
                          int32_t max_report_depth=-1);

        /*!
         * \brief Consume a YAML file at the given path just like addFile,
         * but with an initial set of replacements for any escape sequences
         * found within the Report definition file.
         * \note This is mainly used by simulator infratructure when
         * instantiating a number of reports reports based on wildcard locations
         * \param replacements Vector of replacements which can be indexed by
         * "%1" for replacements[0], "%2" for replacements[1], (etc...) within
         * the report name or stat names specific to this report (not locations)
         * \see addFile
         */
        void addFileWithReplacements(const std::string& file_path,
                                     const std::vector<std::string>& replacements,
                                     bool verbose=false);

        /*!
         * \brief Consume a YAML string at the given path just like
         * addDefinitionString, but with an initial set of replacements for any
         * escape sequences found within the Report definition file.
         * \note This is mainly used by simulator infratructure when
         * instantiating a number of reports reports based on wildcard locations
         * \param replacements Vector of replacements which can be indexed by
         * "%1" for replacements[0], "%2" for replacements[1], (etc...) within
         * the report name or stat names specific to this report (not locations)
         * \see addDefinitionString
         */
        void addDefinitionStringWithReplacements(const std::string& content,
                                                 const std::vector<std::string>& replacements,
                                                 bool verbose=false);

        /*!
         * \brief deep-copies content of another report into this one
         */
        void copyFromReport(const Report& r) {
            for(const Report& sr : r.subreps_){
                subreps_.push_back(sr); // Copies
                subreps_.back().setParent_(this);
            }
            for(const statistics::stat_pair_t& sp : r.stats_){
                add(sp.second, sp.first);
            }
        }

        /*!
         * \brief Adds a blank report to the subreports list
         * \post Subreport will have this report as its parent
         * \return The newly created empty subreport.
         */
        Report& addSubreport(const std::string& name);

        /*!
         * \brief Adds a new subreport deep-copied from an existing report
         * \param r Source report form which all content will be deep copied.
         * No content is shared
         * \return The newly created subreport whose content will be deep-copied
         * from the parameter \a r
         */
        Report& addSubreport(const Report& r);

        /*!
         * \brief Returns a reference to a subreport at a given index
         * \see getNumSubreports
         * \throw exception if no subreport is found at the given index
         */
        Report& getSubreport(size_t idx) {
            auto itr = subreps_.begin();
            for(uint32_t x = 0; x < idx; ++x){
                itr++;
            }
            return *itr;
        }

        /*!
         * \brief Gets a subreport by name.
         * \throw SpartaException if subreport with \a name is not found
         */
        Report& getSubreport(const std::string& name) {
            for(uint32_t i = 0; i < getNumSubreports(); ++i){
                if(getSubreport(i).getName() == name){
                    return getSubreport(i);
                }
            }
            throw SpartaException("Failed to get SubReport \"") << name << "\" from " << getName();
        }

        /*!
         * \brief Does this report have a subreport with a given name
         * \param name Name to searchubreports for
         * \return true if this report has a subreport with name \a name
         */
        bool hasSubreport(const std::string& name) {
            for(uint32_t i = 0; i < getNumSubreports(); ++i){
                if(getSubreport(i).getName() == name){
                    return true;
                }
            }
            return false;
        }

        /*!
         * \brief Does this report have a subreport at the given address
         * \param name Subreport reference to search for.
         * \return true if this report has subreport instance \a r
         */
        bool hasSubreport(const Report& r) {
            for(uint32_t i = 0; i < getNumSubreports(); ++i){
                if(&getSubreport(i) == &r){
                    return true;
                }
            }
            return false;
        }

        /*!
         * \brief Removes a subreport by instance address
         * \post Changes size of subreports vector if subeport having name is found
         * \warning Do not call this from within a function that iterates
         * subreports. Any iterators to the internal subreports vector will be
         * invalidated. If reports must be removed within a loop, use an integer
         * for loop from 0 to getNumSubreports().
         * \return Number of subreports removed - if any
         */
        uint32_t removeSubreport(const Report& r) {
            for(auto itr = subreps_.begin(); itr != subreps_.end(); ++itr){
                if(&(*itr) == &r){
                    subreps_.erase(itr);
                    return 1;
                }
            }
            return 0;
        }

        /*!
         * \brief Removes subreports having the given name
         * \post Changes size of subreports vector if subeport having name is found
         * \warning Do not call this from within a function that iterates
         * subreports. Any iterators to the internal subreports vector will be
         * invalidated. If reports must be removed within a loop, use an integer
         * for loop from 0 to getNumSubreports().
         * \return Number of subreports removed
         */
        uint32_t removeSubreport(const std::string& name) {
            uint32_t num_removed = 0;
            for(auto itr = subreps_.begin(); itr != subreps_.end();){
                if(itr->getName() == name){
                    itr = subreps_.erase(itr);
                    ++num_removed;
                }else{
                    ++itr;
                }
            }
            return num_removed;
        }

        /*!
         * \brief Reports can consume definition YAML entries specifying start
         * and stop behavior, and thus should own those trigger objects.
         */
        void handleParsedTrigger(
            const std::unordered_map<std::string, std::string> & kv_pairs,
            TreeNode * context);

        /*!
         * \brief Let objects know if this report has any triggered behavior
         * for any purpose (this will recurse into all subreports from this
         * report node)
         */
        bool hasTriggeredBehavior() const;

        /*!
         * \brief Query whether this report can be considered ready for statistics
         * printouts (triggered behavior under the hood can render the report "dormant"
         * during warmup periods, cool down periods, etc.)
         *
         * Keep in mind that just because a report responds TRUE one time does
         * not mean that it is always active for stats printouts to file.
         */
        bool isActive() const;

        /*!
         * \brief Returns a reference to a contained StatisticInstance at a
         * given index
         * \param idx Index of statistic to retrieve. Must be in range
         * [0,getNumStatistics)
         * \note index order is order of addition to this report and is
         * constant unless resorted
         * \see getNumStatistics
         * \throw SpartaException if idx is out of bounds
         */
        StatisticInstance& getStatistic(size_t idx) {
            return stats_.at(idx).second;
        }

        /*!
         * \brief Returns a reference to a contained StatisticInstance with a
         * given name for this report
         * \param name Name associated with a statistic within this report. This
         * does not refer to any sparta::TreeNode
         * \note Unnamed statistics cannot be retrieved through this interface
         * \throw SpartaException if no statistic exists with the given name
         * \see hasStatistic
         */
        StatisticInstance& getStatistic(const std::string& name) {
            auto name_itr = stat_names_.find(name);
            if(name_itr == stat_names_.end()){
                throw SpartaException("Could not find statistic named \"") << name
                    << "\" in report \"" << getName() << "\"";
            }
            // Get the statistic from the vector pair
            auto itr = std::find_if(stats_.begin(), stats_.end(),
                                    [name] (const auto & p) -> bool {
                                        return name == p.first;
                                    });
            sparta_assert(itr != stats_.end());
            return (*itr).second;
        }

        /*!
         * \brief Return true if this report has the given stat name
         * \param name The name of the stat to look for
         * \return true has stat, false otherwise
         */
        bool hasStatistic(const std::string& name) const {
            auto itr = stat_names_.find(name);
            return (itr != stat_names_.end());
        }

        /*!
         * \brief Gets the setof subreports contained in this report
         */
        const std::list<Report>& getSubreports() const {
            return subreps_;
        }

        /*!
         * \brief Gets the number of subreports immediately contained in this
         * Report
         */
        uint32_t getNumSubreports() const {
            return subreps_.size();
        }

        /*!
         * \brief Gets the maximum subreport depth from this Report.
         * \return 0 if this Report contains no subreports, 1 if it contains
         * subreports which contain no subreports of their own, 2 if these
         * subreports have at most 1 level of subreports below them.
         */
        uint32_t getSubreportDepth() const {
            uint32_t depth = 0;

            if(subreps_.size() > 0){
                depth = 1;
            }
            for(const Report& sr : subreps_){
                depth = std::max(depth, 1 + sr.getSubreportDepth());
            }

            return depth;
        }

        /*!
         * \brief Gets the set of statistic instances immediately contained in
         * this Report
         */
        const std::vector<statistics::stat_pair_t>& getStatistics() const {
            return stats_;
        }

        /*!
         * \brief Gets the number of statistics immediately owned by this report
         * (excludes subreports)
         */
        uint32_t getNumStatistics() const {
            return stats_.size();
        }

        /*!
         * \brief Gets the number of statistics having names which are
         * immediately owned by this report (excludes subreports)
         */
        uint32_t getNumNamedStatistics() const {
            return stat_names_.size();
        }

        /*!
         * \brief Gets the number of statistics having no names which are
         * immediately owned by this report (excludes subreports)
         */
        uint32_t getNumAnonymousStatistics() const {
            return getNumStatistics() - getNumNamedStatistics();
        }

        /*!
         * \brief Gets the total number of statistics in this report and all
         * subreports (recursively)
         */
        uint32_t getRecursiveNumStatistics() const {
            uint32_t num_stats = stats_.size();

            for(const Report& sr : subreps_){
                num_stats += sr.getRecursiveNumStatistics();
            }

            return num_stats;
        }

        /*!
         * \brief Mapping from statistic definitions to their substatistic instances
         * (supports using ContextCounters together with report triggers)
         */
        typedef std::unordered_map<const StatisticDef*,
            std::vector<const StatisticInstance*>> SubStaticticInstances;

        /*!
         * \brief Get this report's mapping from statistic definitions to substatistic
         * instances, if any
         */
        const SubStaticticInstances & getSubStatistics() const {
            return sub_statistics_;
        }

        /*!
         * \brief Mapping from StatisticInstance's to their sub-StatisticInstance's
         * (supports ContextCounter pseudo-recreation from SimDB records after
         * simulation, where we do not actually have StatisticDef's, or even
         * TreeNode's of any kind).
         *
         * To illustrate what this data structure represents, say that we had
         * the following hierarchy in the original SPARTA simulation:
         *
         *   Report
         *     SI                       (wraps a ContextCounter)
         *       internal_counters_[0]  (wraps a CounterBase)
         *       internal_counters_[1]  (wraps a CounterBase)
         *
         * The equivalent hierarchy when recreating the same report *after*
         * a simulation looks like this:
         *
         *   Report
         *     SI                       (is the root node of a DatabaseContextCounter)
         *       SI                     (is the first sub-statistic under it)
         *       SI                     (is the second sub-statistic under it)
         */
        typedef
            // ...the root node of a DatabaseContextCounter
            std::unordered_map<const StatisticInstance*,
                std::pair<
                    // ...the DatabaseContextCounter itself
                    std::shared_ptr<db::DatabaseContextCounter>,
                    // ...the list of sub-statistics under it
                    std::vector<const StatisticInstance*>>
            > DBSubStatisticInstances;

        /*!
         * \brief Get a SimDB-recreated Report's mapping from SI's to sub-statistic
         * instances, if any
         */
        const DBSubStatisticInstances & getDBSubStatistics() const {
            if (report_node_id_ == 0) {
                sparta_assert(db_sub_statistics_.empty());
            }
            return db_sub_statistics_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Computation Window
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Start the window for this instance. Cleares any cached result
         * values
         * \note This is implicitly called at construction
         */
        void start() {
            if (!legacy_start_trigger_) {
                std::cout << "     [trigger] Now starting report '" << this->getName()
                          << "' at tick " << scheduler_->getCurrentTick()
                          << std::endl;
            }

            start_tick_ = scheduler_->getElapsedTicks();
            end_tick_ = Scheduler::INDEFINITE;

            // Start all contents
            for(auto& s : stats_){
                s.second.start();
            }
            for(auto& r : subreps_){
                r.start();
            }
        }

        /*!
         * \brief Ends the window for this instance. Computes and caches the
         * result of the statistic.
         * \note Re-ending (two calls to end at different times withiout a start
         * call between them) IS supported
         */
        void end(){
            if (!legacy_stop_trigger_) {
                std::cout << "     [trigger] Now stopping report '" << this->getName()
                          << "' at tick " << scheduler_->getCurrentTick()
                          << std::endl;
            }

            end_tick_ = scheduler_->getElapsedTicks();

            // End all contents
            // Start all contents
            for(auto& s : stats_){
                s.second.end();
            }
            for(auto& r : subreps_){
                r.end();
            }
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

        /*!
         * \brief Has this report ended?
         * \return true if end_tick_ != Scheduler::INDEFINITE
         */
        bool isEnded() const {
            return end_tick_ != Scheduler::INDEFINITE;
        }

        /*!
         * \brief Supply information string (for headers)
         */
        void setInfoString(const std::string& info) {
             info_string_ = info;
        }

        /*!
         * \brief Obtain information string (for headers)
         */
        const std::string& getInfoString() const {
             return info_string_;
        }

        /*!
         * \brief Set and overwrite header content
         */
        report::format::ReportHeader & getHeader() const;
        bool hasHeader() const;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Meta-Data
        //! @{
        ////////////////////////////////////////////////////////////////////////


        /*!
         * \brief Returns the parent of this report in the report hierarchy.
         * If not nullptr, the report returned is guaranteed to contain this
         * report in its subreports list
         * \see hasSubreport
         */
        Report* getParent() const {
            return parent_;
        }

        /*!
         * \brief Get the root-level Report object this object
         * lives under. This is different from getParent(), as
         * getRoot() walks all the way to the top of the report
         * tree. This method never returns null. If this object
         * has no parent, getRoot() returns 'this'.
         */
        Report* getRoot() {
            Report * parent = this;
            Report * root = parent;
            while (parent != nullptr) {
                root = parent;
                parent = parent->parent_;
            }
            sparta_assert(root);
            return root;
        }

        /*!
         * \brief Sets the name of the report
         * \param name Name to assign
         * \note The report name can be changed at any time.
         */
        void setName(const std::string& name) {
            name_ = name;
        }

        /*!
         * \brief Gets the current name of this report
         */
        const std::string& getName() const {
            return name_;
        }

        const Scheduler* getScheduler() const {
            return scheduler_;
        }

        /*!
         * \brief Sets the author of the report
         * \param author Author to assign
         * \note The report author can be changed at any time.
         */
        void setAuthor(const std::string& author) {
            author_ = author;
        }

        /*!
         * \brief Gets the current author of this report
         */
        const std::string& getAuthor() const {
            return author_;
        }

        /*!
         * \brief Sets a particular style attribute on this report
         * \param style Style attribute to set
         * \param value Valut to associate with the \a style attribute
         */
        void setStyle(const std::string& style, const std::string & value) {
            style_[style] = value;
        }

        /*!
         * \brief Does this report have a particular style
         * \param style Style attribute for which to search
         */
        bool hasStyle(const std::string& style) const {
            return style_.find(style) != style_.end();
        }

        /*!
         * \brief Gets a particular style attribute from this report (or
         * inherited from its ancestors). If it can not be found (via hasStyle
         * and a recursive parent search), returns \a def
         * \param style Style attribute to retrieve
         * \param def Default value to return if \a style attribute is not found
         */
        const std::string getStyle(const std::string& style,
                                    const std::string& def="") const {
            if(hasStyle(style)){
                return style_.at(style);
            }else if(parent_){
                return parent_->getStyle(style, def);
            }
            return def;
        }

        /*!
         * \brief Return a map of all styles for this report
         */
        const std::map<std::string, std::string> & getAllStyles() const {
            return style_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Representation (lossy)
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Renders this Report to a string containing
         * computation window, source, and current value.
         */
        std::string stringize() const {
            std::stringstream ss;
            ss << "Report: " << getName();

            // Range
            ss << "[" << start_tick_ << ",";
            if(end_tick_ == Scheduler::INDEFINITE){
                ss << scheduler_->getElapsedTicks();
            }else{
                ss << end_tick_;
            }
            ss << "]";

            // Content
            return ss.str();
        }

        /*!
         * \brief Dumps the content of this report to an ostream in a trivial
         * text based format for easy screen reading
         */
        void dump(std::ostream& o, uint32_t depth=0) const {
            const std::string INDENT_STR = "  ";
            std::stringstream indent;
            for(uint32_t d=0; d<depth; ++d){
                indent << INDENT_STR;
            }

            o << indent.str() << "Report: \"" << getName() << "\" [" << start_tick_ << ",";
            if(end_tick_ == Scheduler::INDEFINITE){
                o << scheduler_->getElapsedTicks();
            }else{
                o << end_tick_;
            }
            o << "]\n";

            indent << INDENT_STR;

            for(const statistics::stat_pair_t& si : stats_){
                o << indent.str();
                if(si.first != ""){
                    // Print "custom_name = value"
                    o << si.first;
                }else{
                    // Print "stat_location = value"
                    o << si.second.getLocation();
                }
                o << " = ";

                double val = si.second.getValue();
                o << formatNumber(val);

                // Could print the expression after the value
                //o << "  # " << si.second.getExpressionString();

                o << std::endl;
            }

            for(const Report& sr : subreps_){
                sr.dump(o, depth+1);
            }
        }

        /*!
         * \brief This utility can be used to generate a formatted
         * report from a root-level ReportNodeHierarchy record in
         * the provided database (ObjectManager).
         *
         * The given report_hier_node_id must have ParentNodeID=0
         * in the ReportNodeHierarchy table, or this method will
         * throw an exception.
         */
        static bool createFormattedReportFromDatabase(
            const simdb::ObjectManager & obj_mgr,
            const simdb::DatabaseID report_hier_node_id,
            const std::string & filename,
            const std::string & format,
            const Scheduler * scheduler);

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Adds a new field to the stats_ list. Catches and rethrows
         * SpartaExceptions after appending what would have been the name of the
         * stat being added
         * \param name Name of the field to add. This can be a
         * StatisticInstance&, a TreeNode*, a CounterBase*, or StatisticDef*
         * \param si_arg Construction argument for a sparta::StatisticInstance to
         * be added as a stat.
         * function takes ownership
         */
        template <typename T>
        void addField_(const std::string& name, T si_arg) {
            try{
                stats_.emplace_back(name, StatisticInstance(si_arg));
                stats_.back().second.setContext(scheduler_);
            }catch(SpartaException& ex){
                ex << " StatisticInstance would have been named \"" << name << "\"";
                throw;
            }
        }

        /*!
         * \brief If the given parent statistic instance has any pending substatistic
         * info (TreeNode* and statistic name), create those substatistics now and add
         * them to this report
         */
        void addSubStatistics_(const StatisticInstance * parent_stat) {
            const auto & sub_statistics_info = parent_stat->getSubStatistics();
            for (const auto & sub_stat_info : sub_statistics_info) {
                const TreeNode * stat_node = sub_stat_info.getNode();
                const std::string & stat_name = sub_stat_info.getName();

                //Add the substatistic to this report
                add(stat_node, stat_name);

                //Update mapping from statistic definition to substatistic instance
                const StatisticDef * stat_def = parent_stat->getStatisticDef();
                if (stat_def != nullptr) {
                    sub_statistics_[stat_def].emplace_back(&stats_.back().second);
                }
            }
        }

        /*!
         * \brief Sets the parent link of this report.
         * \param parent Report to assign as this reports parent. Can be nullptr
         * \pre If not nullptr, \a parent must already contain this report as a
         * subreport which will be verified through hasSubreport(const Report&)
         * \note Overrides previous parent
         * \see getParent
         * \see hasSubreport
         */
        void setParent_(Report* parent) {
            if(parent){
                sparta_assert(parent->hasSubreport(*this),
                                  "Attempted to setParent as \"" << parent->getName()
                                  << "\" on subreport \"" << getName() << "\" while this report "
                                  "was not listed as a subreport of that parent. This report must "
                                  "be a subreport of parent before setParent_ can be called");
            }
            parent_ = parent;
        }

        /*!
         * \brief Implements addSubtree. Most parameters are identical
         * \param recurs_depth Current recursion depth (from addSubtree call)
         * \param report_depth Current report depth (from addSubtree call)
         * \param stat_prefix String prefix prepended to all stats. This is
         * appended-to whenever a branch node is encountered that doesn't cause
         * a new subreport for be created.
         */
        void recursAddSubtree_(const TreeNode* n,
                               subreport_decision_fxn_t make_sr_fxn,
                               inclusion_decision_fxn_t branch_inc_fxn,
                               inclusion_decision_fxn_t leaf_inc_fxn,
                               bool add_counters,
                               bool add_stats,
                               int32_t max_recurs_depth,
                               uint32_t recurs_depth,
                               uint32_t report_depth,
                               const std::string& stat_prefix);

        /*!
         * \brief Callbacks for diagnostic / trigger status printout on report start and stop
         */
        void legacyDelayedStart_(const trigger::CounterTrigger * trigger);
        void legacyDelayedEnd_(const trigger::CounterTrigger * trigger);

        /*!
         * \brief Reconstruct a Report node from a database record ID in
         * the provided SimDB. Throws if the given report hierarchy node
         * ID is not found in this database.
         *
         * This private constructor is not meant to be invoked directly
         * from the outside world. This would typically be called from
         * from SimDB-related static sparta::Report methods.
         */
        Report(const simdb::DatabaseID report_hier_node_id,
               const simdb::ObjectManager & obj_mgr,
               const Scheduler * scheduler);

        /*!
         * \brief When we recreate a sparta::Report object from SimDB
         * records, we need to put a few things in place which help
         * us *directly* get our SI values from the database blob,
         * since we are not even running an actual simulation. For
         * the most part, SimDB-created SI's do not have any internals
         * such as CounterBase/ParameterBase/StatisticDef pointers.
         * They get their SI values from "StatInstValueLookup" objects,
         * which are tied to "StatInstRowIterator" objects. The SI's
         * own the value lookup objects, while the reports/subreports
         * own the row iterator objects. Both of these objects work
         * together like this:
         *
         *    1.  Advance the row iterator to the next row of SI values.
         *
         *    2a. Ask the value lookup for your specific SI value.
         *        It knows your SI index, so it knows the element
         *        offset into the SI double vector, and it gives
         *        you the value.
         *
         *    2b. The value lookup objects are all bound to the row
         *        iterator's vector<double>, which is itself bound
         *        to an ObjectQuery against one of the SimDB tables...
         *
         *    3.  Call StatInstRowIterator::getNext() to advance the
         *        row iterator one more row in the database. This
         *        calls ObjectQuery::getNext(), which memcpy's the
         *        next SI blob into the row iterator's vector<double>,
         *        decompressing the blob if needed.
         *
         *    4.  All StatInstValueLookup objects that were bound to this
         *        row iterator object will be "updated automatically", since
         *        they are just using indirection to point somewhere else
         *        which actually has the current values.
         */
        bool prepareForSIDatabaseIteration_(
            const simdb::ObjectManager & obj_mgr);

        /*!
         * \brief Starting at 'this' report node, recursively get
         * all mappings from Report/SI database node ID to the
         * Report or SI that lives at each node.
         */
        void recursGetReportAndSINodeDatabaseIDs_(
            std::unordered_map<simdb::DatabaseID, Report*> & report_nodes_by_id,
            std::unordered_map<simdb::DatabaseID, const StatisticInstance*> & si_nodes_by_id);

        /*!
         * \brief Starting at 'this' report node, find the first
         * StatInstRowIterator member variable we encounter while
         * traversing in a depth-first fashion.
         */
        std::shared_ptr<sparta::StatInstRowIterator>
            recursFindTopmostSIRowIteratorPlaceholder_();

        /*!
         * \brief Set/reset/unset the StatInstRowIterator that is
         * given to us. Passing in a null row iterator is the same
         * thing as resetting this report's row iterator; it will
         * not reject a null iterator object.
         */
        void recursSetSIRowIterator_(
            std::shared_ptr<sparta::StatInstRowIterator> & si_row_iterator);

        /*!
         * \brief Schedler associated with this report (for time-elapsed information)
         */
        const Scheduler * scheduler_;

        /*!
         * \brief Name of this report
         */
        std::string name_;

        /*!
         * \brief Author of this report
         */
        std::string author_;

        /*!
         * \brief Styles associated with this report
         */
        std::map<std::string, std::string> style_;

        /*!
         * \brief Context of node-name seaches in this Report.
         * Can be nullptr, which will disallow adding by node name/pattern.
         * Can be changed at any time.
         */
        TreeNode* context_;

        /*!
         * \brief Link to parent report
         */
        Report* parent_ = nullptr;

        /*!
         * \brief Vector of subreports in specific order of addition (unless
         * resorted)
         */
        std::list<Report> subreps_;

        /*!
         * \brief Individual report start and stop behavior is controlled by
         * expressions given in definition YAML files.
         */
        std::unique_ptr<trigger::ExpressionTrigger> report_start_trigger_;
        std::unique_ptr<trigger::ExpressionTrigger> report_stop_trigger_;
        std::shared_ptr<sparta::SubContainer> report_container_;

        bool legacy_start_trigger_ = true;
        bool legacy_stop_trigger_ = true;

        /*!
         * \brief Vector of contained stats (excluding subreports) and their
         * associated names (for this report) in  order of addition (unless
         * resorted)
         * \note Anything removed from this list needs to be removed from
         * stat_names_ as well.
         */
        std::vector<statistics::stat_pair_t> stats_;

        /*!
         * \brief Map of string identifiers to statistics in the stats_ vector
         */
        std::set<std::string> stat_names_;

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
         * \brief Optional information (for headers)
         */
        std::string info_string_;

        /*!
         * \brief Optional header content
         */
        mutable std::shared_ptr<report::format::ReportHeader> header_;

        /*!
         * \brief Mapping from statistic definitions to their substatistic instances,
         * if any
         */
        SubStaticticInstances sub_statistics_;

        /*!
         * \brief Flag for enabling auto-expansion of ContextCounter stats (off by default)
         */
        bool auto_expand_context_counter_stats_ = false;

        //! \name SimDB-related variables
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Mapping from DB-recreated StatisticInstance's to their
         * sub-statistic instances, if any.
         */
        DBSubStatisticInstances db_sub_statistics_;

        /*!
         * \brief This row iterator object is a wrapper around an
         * ObjectQuery. It is used to get report/SI data values
         * out of a SimDB, and into a formatted report (json_detail,
         * html, text, etc.)
         */
        std::shared_ptr<sparta::StatInstRowIterator> si_row_iterator_;

        /*!
         * \brief Cached database ID. Equals 0 for all Report objects
         * created during simulation. Will be non-zero for Report objects
         * recreated after a simulation from SimDB record(s).
         */
        simdb::DatabaseID report_node_id_ = 0;

        /*!
         * \brief Cached database ID(s) of SimDB-recreated StatisticInstance's
         * that belong to this Report object. Will be empty for Report objects
         * created during simulation.
         */
        std::vector<simdb::DatabaseID> si_node_ids_;

        ////////////////////////////////////////////////////////////////////////
        //! @}
    };

    //! \brief Report stream operator
    inline std::ostream& operator<< (std::ostream& out, sparta::Report const & r) {
        r.dump(out);
        return out;
    }

    //! \brief TreeNode stream operator
    inline std::ostream& operator<< (std::ostream& out, sparta::Report const * r) {
        if(nullptr == r){
            out << "null";
        }else{
            r->dump(out);
        }
        return out;
    }

} // namespace sparta
