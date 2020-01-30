// <Report> -*- C++ -*-

/*!
 * \file Report.cpp
 * \brief Part of the metrics and statistics system.
 * Contains a Report class which refers to a number of StatisticInstance
 * instances of other Reports to present a set of associated simuation metrics
 */

#include "sparta/report/Report.hpp"

#include <iostream>
#include <sstream>
#include <math.h>
#include <string>

#include "sparta/parsers/YAMLTreeEventHandler.hpp"
#include "sparta/utils/Printing.hpp"

#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/ContextCounter.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/report/SubContainer.hpp"
#include "sparta/report/format/ReportHeader.hpp"
#include "sparta/report/format/Text.hpp"
#include "sparta/utils/File.hpp"
#include "sparta/tree/filter/Grammar.hpp"
#include "sparta/tree/filter/Parser.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"
#include "sparta/statistics/HistogramFunctionManager.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "simdb/ObjectManager.hpp"
#include "sparta/report/db/StatInstRowIterator.hpp"
#include "sparta/report/db/StatInstValueLookup.hpp"
#include "sparta/report/db/DatabaseContextCounter.hpp"

//SQLite-specific headers
#include "zlib.h"

#include "boost/filesystem.hpp"

namespace sparta
{
    typedef std::pair<YAMLTreeEventHandler::node_uid_t, TreeNode*> parent_node_info_t;
}

namespace std
{
    /*!
     * \brief Hashing function for sparta::parent_node_info_t
     */
    template <> struct hash<sparta::parent_node_info_t>
    {
        size_t operator()(const sparta::parent_node_info_t & x) const
        {
            return hash<unsigned long long>()((unsigned long long)(void*)x.second)
                   + hash<unsigned long long>()((unsigned long long)x.first);
        }
    };
}

namespace sparta
{

/*!
 * \brief Helper for parsing sparta report definition files
 */
class ReportFileParserYAML
{
    /*!
     * \brief Event handler for YAML parser. Operates on a specific report
     */
    class ReportFileEventHandlerYAML : public YAMLTreeEventHandler
    {
        /*!
         * \brief Report to populate
         */
        Report* base_report_;

        /*!
         * \brief Largest context UID so far. New IDs will be created after this
         * to guarnatee uniqueness
         */
        YAMLTreeEventHandler::node_uid_t largest_context_uid_;

        /*!
         * \brief Stack of reports encountered while recursively interpreting a
         * report definition file.
         * \note bottom element (base_report_) Should never be popped off
         *
         * Top of this stack is the current report. Includes the base_report_
         */
        std::stack<std::vector<Report*>> report_stack_;

        /*!
         * \brief Map of reports encountered while recursively interpreting a
         * report definition file.
         *
         * Used to associate the uid field in YAMLTreeEventHandler::NavNode
         * objects so that multiple reports can be tracked as the tree context
         * expands during recursion.
         */
        std::unordered_map<node_uid_t, Report*> report_map_;

        /*!
         * \brief Map for tracking parent-child report relationships.
         */
        std::unordered_map<parent_node_info_t, node_uid_t> next_uid_map_;

        /*!
         * \brief Is this event handler currently in a report content parsing
         * state. If not in this state, leaves (stats/counters) should not be
         * allowed
         */
        std::stack<bool> in_content_stack_;

        // Did we find an 'ignore' block?
        bool in_ignore_ = false;

        // Did we find an 'optional' block?
        bool in_optional_ = false;

        /*!
         * \brief Current set of autopopulate options. Must be empty unless in
         * an "autopopulate:" map
         */
        std::map<std::string, std::string> current_autopop_block_;

        /*!
         * \brief Current set of style options. If not null, parser is in a
         * style block (KEY_STYLE) within a content section. If null, not in a
         * style block. When exiting a style map, these items in this map are
         * transfered to the report at the top of the parser's stack
         */
        std::unique_ptr<std::map<std::string, std::string>> style_block_;

        /*!
         * \brief Current trigger for the report or subreport being populated.
         * If the report is to be triggered, the trigger definitions should be found
         * at the root level of the report/subreport ONLY:
         *
         *      subreport:
         *          name: Core 0 stats
         *          trigger:
         *              start: "core0.rob.stats.total_number_retired >= 1500"
         * >> !             trigger:
         * >> !                 stop: "core0.rob.stats.total_number_retired >= 40"
         *          core0:
         *            include: stats.yaml
         *      subreport:
         *          name: Core 1 stats
         *          trigger:
         *              start: "core1.rob.stats.total_number_retired >= 2500"
         *          core1:
         *              include: stats.yaml
         *
         * Will throw an exception. Nested triggers are not supported.
         */
        std::unique_ptr<std::unordered_map<std::string, std::string>> trigger_defn_;

        //! \brief Keyword for a new top-level report
        static constexpr char KEY_REPORT[] = "report";

        //! \brief Keyword for a new subreport within another [sub]report
        static constexpr char KEY_SUBREPORT[] = "subreport";

        //! \brief Keyword for content of a report (stats)
        static constexpr char KEY_CONTENT[] = "content";

        //! \brief Keyword for name of a report (outside of content)
        static constexpr char KEY_NAME[] = "name";

        //! \brief Keyword for author of a report (outside of content)
        static constexpr char KEY_AUTHOR[] = "author";

        //! \brief Keyword to automatically populate a report from a subtree
        static constexpr char KEY_AUTOPOPULATE[] = "autopopulate";

        //! \brief Attribute filter key (within autopopulate)
        static constexpr char KEY_AUTOPOPULATE_ATTRIBUTES[] = "attributes";

        //! \brief Maximum recursion depth key (within autopopulate)
        static constexpr char KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH[] = "max_recursion_depth";

        //! \brief Maximum subtree depth key (within autopopulate)
        static constexpr char KEY_AUTOPOPULATE_MAX_REPORT_DEPTH[] = "max_report_depth";

        //! \brief Style block key
        static constexpr char KEY_STYLE[] = "style";

        //! \brief Trigger block key
        static constexpr char KEY_TRIGGER[] = "trigger";

        //! \brief Report Ignore Keyword
        static constexpr char KEY_REPORT_IGNORE[] = "report_ignore";

        //! \brief Report optional keyword
        static constexpr char KEY_REPORT_OPTIONAL[] = "optional";

    public:

        /*!
         * \brief Constructor
         * \param r Report to start consuming
         * \param filename Name of YAML file to consume
         * \param contexts vector of TreeNodes from which this parser navigates
         * when it encounters a new TreeNode relative path.
         * \param in_content Is the owner of this parser within the content section
         * of some report definition file. This is typically false except when
         * "#include"-ing a yaml file from within a content section of another
         */
        ReportFileEventHandlerYAML(Report* r,
                                   const std::string& filename,
                                   NavVector contexts,
                                   bool in_content=false,
                                   bool verbose=false) :
            YAMLTreeEventHandler(filename,
                                 contexts,
                                 verbose,
                                 nullptr),
            base_report_(r),
            largest_context_uid_(0)
        {
            if(nullptr == base_report_){
                throw SpartaException("Cannot parse a yaml report definition file without a non-null "
                                    "base report pointer");
            }

            in_content_stack_.push(in_content);

            report_stack_.push({base_report_}); // Always at bottom of stack

            // Direct all context IDs to the base report no matter what they are
            for(auto& ctxt : contexts) {
                report_map_[ctxt->uid] = base_report_;
                largest_context_uid_ = std::max(ctxt->uid, largest_context_uid_);
            }
        }

    protected:


        // Called per context scope node
        void handleLeafScalar_(TreeNode* n,
                               const std::string& value,
                               const std::string& assoc_key,
                               const std::vector<std::string>& captures,
                               node_uid_t uid) override {
            // Note: this is invoked once per context node
            sparta_assert(n);
            bool in_content = in_content_stack_.top();
            //Report* const r = report_stack_.top();
            Report* const r = report_map_.at(uid);

            if(style_block_ != nullptr){
                // Handle style. This is also done in handleLeafScalarUnknownKey_
                verbose() << indent_() << "Got style \"" << assoc_key << " = \"" << value << "\""
                          << std::endl;
                (*style_block_)[assoc_key] = value;
            }else if(current_autopop_block_.size() > 0){
                if(assoc_key == KEY_AUTOPOPULATE_ATTRIBUTES){
                    current_autopop_block_[KEY_AUTOPOPULATE_ATTRIBUTES] = value;
                }else if(assoc_key == KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH){
                    current_autopop_block_[KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH] = value;
                }else if(assoc_key == KEY_AUTOPOPULATE_MAX_REPORT_DEPTH){
                    current_autopop_block_[KEY_AUTOPOPULATE_MAX_REPORT_DEPTH] = value;
                }else{
                    // Unknown key for within an autopop section
                    std::stringstream ss;
                    ss << "Encountered key within an autopopulation block that was not handled: \""
                       << assoc_key << "\". Value = \"" << value << "\"" << std::endl;
                    addError_(ss.str());
                }
            }else if(assoc_key == KEY_NAME){
                if(r->getName() != ""){
                    verbose() << indent_() << "Warning: A current report being renamed from \""
                        << r->getName() << "\" to \"" << value << "\"" << " because a name key was "
                        "found when the report already had a name. This probably happened "
                        "because a \"name:\" was specified twice within the report or a "
                        "file was added to a report which already had a name" << std::endl;
                }
                std::string full_name = value;
                if(replaceByIndex(full_name, n, captures)){
                    r->setName(full_name);
                }
                verbose() << indent_() << "  Updated name of report: " << r << std::endl;
            }else if(assoc_key == KEY_AUTHOR){
                if(r->getAuthor() != ""){
                    verbose() << indent_() << "Warning: Report being re-authored from \""
                        << r->getAuthor() << "\" to \"" << value << "\"" << " because an author "
                        "key was found when the report already had an author" << std::endl;
                }
                r->setAuthor(value);
                verbose() << indent_() << "  Updated author of report: " << r << std::endl;
            }else if(assoc_key == KEY_AUTOPOPULATE){
                r->autoPopulate(n, value, captures, -1, -1);
            }else{
                verbose() << indent_() << "Got leaf scalar at " << *n << " with value = \"" << value
                          << "\" and key \"" << assoc_key << "\" in report " << *r << std::endl;

                if(in_content){
                    std::string full_name = value;
                    if(getSubstituteForStatName(full_name, n, captures)){
                        r->add(n, full_name);
                        const bool expand_cc_stats = r->isContextCounterStatsAutoExpansionEnabled();
                        if(expand_cc_stats) {
                            auto sd = dynamic_cast<StatisticDef*>(n);
                            if (sd && sd->getSubStatistics().size() > 1) {
                                r->addSubStats(sd, full_name);
                            }
                        }
                    }else{
                        // Errors were added via addError_. They will be displayed later
                    }
                }else{
                    // Cannot add stats/counters outside of a content section
                    std::stringstream ss;
                    ss << "Encountered a leaf scalar \"" << assoc_key << "\" that was not within a "
                       << "content section";
                    addError_(ss.str());
                }
            }
        }

        bool handleLeafScalarUnknownKey_(TreeNode* n,
                                         const std::string& value,
                                         const std::string& assoc_key,
                                         const NavNode& scope) override {
            sparta_assert(n);
            bool in_content = in_content_stack_.top();
            //Report* const r = report_stack_.top();

            if(in_content){
                if(current_autopop_block_.size() > 0){
                    // Inapproriate context for unknown key for within an autopop section
                    std::stringstream ss;
                    ss << "Encountered unknown node key within an autopopulation block: \""
                       << assoc_key << "\". Value = \"" << value << "\". This key should have been "
                       "handled in handleLeafScalar_ instead" << std::endl;
                    addError_(ss.str());
                }else{
                    // Attempt to convert assoc_key to an expression
                    try{
                        // Attempt to see if there is a hist_def keyword in this path
                        const std::size_t first_of = assoc_key.find_first_of('.');
                        if(first_of != std::string::npos){
                            const auto last_pos = assoc_key.rfind('.');
                            const auto sec_last_pos = assoc_key.rfind('.', last_pos - 1);
                            const bool is_npos = sec_last_pos == std::string::npos;
                            const auto prefix = assoc_key.substr(is_npos ? 0 : sec_last_pos + 1,
                                                                 is_npos ? last_pos :
                                                                 last_pos - sec_last_pos - 1);
                            if(prefix == sparta::FunctionManager::get().getToken()){
                                std::string path_in_report(assoc_key);
                                path_in_report.erase(is_npos ? 0 : sec_last_pos + 1,
                                                     prefix.size() + 1);

                                std::string fcn_key {};

                                // Attempt to strip out the function name from the full path.
                                const std::size_t last_of = path_in_report.find_last_of('.');

                                // Case when there is no part of the location left in the string.
                                // Cases like hist_def.fcn_name : fcn_name_detail
                                if(last_of == std::string::npos){
                                    fcn_key = path_in_report;
                                    path_in_report = "";
                                }
                                // Case when there is some part of the location left in the string.
                                // Cases like hist_def.core0.histogram_tn.fcn_name : fcn_name_detail
                                else{
                                    const std::size_t chars_to_remove = path_in_report.size() - last_of;

                                    // Store the function name.
                                    fcn_key = path_in_report.substr(last_of + 1, chars_to_remove - 1);
                                    path_in_report.erase(last_of, chars_to_remove);
                                }

                                // Get child node from path string.
                                const auto child_node =
                                    path_in_report.empty() ? n : n->getChild(path_in_report);

                                // Attempt to cast to cycle_histogram node.
                                const auto cycle_histogram_node = dynamic_cast<sparta::CycleHistogramTreeNode*>(child_node);
                                if(cycle_histogram_node){

                                    // Get the registered function pointer from static map.
                                    auto fcn =
                                        sparta::FunctionManager::get().find<CycleHistogramTreeNode>(fcn_key);
                                    using return_type = MetaStruct::return_type_t<decltype(fcn)>;
                                    static_assert(std::is_convertible<return_type, double>::value,
                                    "The return type of the custom method must be convertible to type : double.");

                                    // Bind function pointer with histogram node as parameter.
                                    std::function<return_type (double)> bound_fcn =
                                        std::bind(fcn, cycle_histogram_node);

                                    // Build temporary Expression instance.
                                    statistics::expression::Expression expr(
                                        value, bound_fcn,
                                        statistics::expression::Expression(0.0));

                                    // Build the StatisticInstance responsible for evaluating.
                                    StatisticInstance si(std::move(expr));
                                    si.setContext(n);
                                    std::string full_name = value;
                                    auto& captures = scope.second;
                                    Report* const r = report_map_.at(scope.uid);
                                    if(getSubstituteForStatName(full_name, n, captures)){
                                        r->add(si, full_name);
                                    }
                                }
                                else{
                                    // Attempt to cast to histogram node.
                                    const auto histogram_node = dynamic_cast<sparta::HistogramTreeNode*>(child_node);
                                    if(histogram_node){
                                        // Get the registered function pointer from static map.
                                        auto fcn = sparta::FunctionManager::get().find<HistogramTreeNode>(fcn_key);
                                        using return_type = MetaStruct::return_type_t<decltype(fcn)>;
                                        static_assert(std::is_convertible<return_type, double>::value,
                                        "The return type of the custom method must be convertible to type : double.");

                                        // Bind function pointer with histogram node as parameter.
                                        std::function<return_type (double)> bound_fcn =
                                            std::bind(fcn, histogram_node);

                                        // Build temporary Expression instance.
                                        statistics::expression::Expression expr(
                                            value, bound_fcn,
                                            statistics::expression::Expression(0.0));

                                        // Build the StatisticInstance responsible for evaluating.
                                        StatisticInstance si(std::move(expr));
                                        si.setContext(n);
                                        std::string full_name = value;
                                        auto& captures = scope.second;
                                        Report* const r = report_map_.at(scope.uid);
                                        if(getSubstituteForStatName(full_name, n, captures)){
                                            r->add(si, full_name);
                                        }
                                    }
                                }
                            }
                            else{
                                statistics::expression::Expression expr(assoc_key, n);
                                StatisticInstance si(std::move(expr));
                                si.setContext(n);
                                std::string full_name = value;
                                auto& captures = scope.second;
                                Report* const r = report_map_.at(scope.uid);
                                if(getSubstituteForStatName(full_name, n, captures)){
                                    r->add(si, full_name);
                                }
                            }
                        }
                        else{
                            statistics::expression::Expression expr(assoc_key, n);
                            StatisticInstance si(std::move(expr));
                            si.setContext(n);
                            std::string full_name = value;
                            auto& captures = scope.second;
                            Report* const r = report_map_.at(scope.uid);
                            if(getSubstituteForStatName(full_name, n, captures)){
                                r->add(si, full_name);
                            }
                        }
                    }catch(SpartaException& ex){
                        std::stringstream ss;
                        ss << "Unable to parse expression: \"" << assoc_key << "\" within context: "
                            << n->getLocation() << " in report file \"" <<  getFilename()
                            << "\" for the following reason: " << ex.what();
                        if(in_optional_) {
                            // Possibly in an optional block where the
                            // expressions are simply not in the tree
                            addWarning_(ss.str());
                            return true; // Act as if it were found
                        }
                        else {
                            addError_(ss.str());
                        }
                        return false; // Report as error
                    }
                }

                return true;
            }else{
                if(style_block_ != nullptr){
                    // Handle style. This is also done in handleLeafScalar_
                    verbose() << indent_() << "Got style \"" << assoc_key << " = \"" << value
                              << "\"" << std::endl;
                    (*style_block_)[assoc_key] = value;
                    return true;
                }
                if (trigger_defn_ != nullptr) {
                    verbose() << indent_() << "Got trigger definition -> " << assoc_key << ": '" << value << "'";
                    (*trigger_defn_)[assoc_key] = value;
                    return true;
                }
            }

            // Cannot add stats/counters outside of a content section
            std::stringstream ss;
            ss << "Encountered an unknown leaf sclalar \"" << assoc_key << "\" that was not "
               << "within a content section";
            addError_(ss.str());

            return false;
        }

        void handleLeafSequence_(TreeNode* n,
                                 const std::vector<std::string>& value,
                                 const std::string& assoc_key,
                                 const NavNode& scope) override {
            //Report* const r = report_stack_.top();

            if(assoc_key == KEY_NAME){
                std::stringstream ss;
                ss << "Unexpected key \"name\" with a sequence value. \"name\" is reserved to "
                      "identify the name of a report. Found within scope " << scope;
                addError_(ss.str());
            }else if(assoc_key == KEY_AUTHOR){
                std::stringstream ss;
                ss << "Unexpected key \"author\" with a sequence value. \"author\" is reserved to "
                      "identify the author of a report. Found scope " << scope;
            }else{
                //! \todo Determine what the stat/counter is
                std::stringstream ss;
                ss << "Encountered a leaf sequence at " << *n << " with value = \"" << value
                          << "\" and key \"" << assoc_key << "\". A report definition should not "
                          "contain any leaf sequences";
                addError_(ss.str());
            }
        }

        void handleIncludeDirective_(const std::string& filename,
                                     NavVector& device_trees) override {
            sparta_assert(report_stack_.size() > 0);

            boost::filesystem::path filepath = filename;
            if(false == boost::filesystem::is_regular_file(filepath.native())){
                boost::filesystem::path curfile(getFilename());
                filepath = curfile.parent_path() / filename;
                verbose() << "Note: file \"" << filename << "\" does not exist. Attempting to "
                             "open \"" << filepath.native() << "\" instead" << std::endl;
            }

            ReportFileParserYAML yaml(filepath.native());


            bool in_content = in_content_stack_.top();

            verbose() << indent_() << "Handling include directive at context=" << device_trees
                      << std::endl;

            //Report* const r = report_stack_.top();
            sparta_assert(report_stack_.size() > 0);
            sparta_assert(device_trees.size() > 0,
                              "Somehow reached an include directory in a context with no scope nodes:" << device_trees);

            // Verify that all nodes in the context refer to the same report
            Report* const r = report_map_[device_trees.at(0)->uid];
            for(auto& cx : device_trees){
                if(r != report_map_[cx->uid]){
                    SpartaException ex("");
                    ex << "Encountered include directive in a context where there "
                       "were multiple scope nodes populating different reports. The report "
                       "definition system cannot currently handle this. Ensure inlcude "
                       "directives occur in the report definition where all scopes in the "
                       "current context are within 1 report or subreport file. Context is: [";
                    for(std::shared_ptr<NavNode>& x : device_trees){
                        ex << *x << ",";
                    }
                    ex << "]";
                    throw ex;
                }
            }

            // Proceed because all are garuanteed to have the same report
            yaml.consumeReportFile(r,
                                   device_trees,
                                   in_content,
                                   isVerbose());
        }

        bool isReservedKey_(const std::string& key) const override {
            return (key == KEY_REPORT
                    || key == KEY_SUBREPORT
                    || key == KEY_CONTENT
                    || key == KEY_NAME
                    || key == KEY_AUTHOR
                    || key == KEY_AUTOPOPULATE
                    || key == KEY_AUTOPOPULATE_ATTRIBUTES
                    || key == KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH
                    || key == KEY_AUTOPOPULATE_MAX_REPORT_DEPTH
                    || key == KEY_STYLE
                    || key == KEY_TRIGGER);
        }

        bool isIgnoreKey_(const std::string& key) const override {
            return (key == KEY_REPORT_IGNORE);
        }

        bool traverseSequence_() const override {
            return !in_ignore_;
        }

        bool handleEnterMap_(const std::string& key,
                             NavVector& context) override {
            bool in_content = in_content_stack_.top();
            //Report* const r = report_stack_.top();
            sparta_assert(report_stack_.size() > 0);

            if(key == KEY_REPORT) {
                throw SpartaException("report keyword not supported yet. subreport was probably "
                                    "intended instead");
            }

            if(key == KEY_REPORT_IGNORE) {
                in_ignore_ = true;
                return false;
            }

            // Error
            if(!in_content && (key == KEY_REPORT_OPTIONAL)) {
                std::stringstream ss;
                ss << "Unexpected optional keyword location -- should be in content";
                addError_(ss.str());
                return true;
            }

            if(current_autopop_block_.size() > 0){
                std::stringstream ss;
                ss << "Unexpected map start within autopopulation block at key: \"" << key
                   << "\"";
                addError_(ss.str());
                return true;
            }

            if(style_block_ != nullptr){
                std::stringstream ss;
                ss << "Unexpected map start within style block at key: \"" << key
                   << "\"";
                addError_(ss.str());
                return true;
            }

            // Determine what to do given the current state. Do not return
            // without either posting an error or pushing to the
            // in_content_stack_

            if(in_content){
                in_content_stack_.push(true); // Still in content

                if(in_ignore_) { return false; }

                if(key == KEY_REPORT_OPTIONAL) {
                    in_optional_ = true;
                    return false;
                }

                if(key == KEY_SUBREPORT){
                    // Create a new sub-report
                    verbose() << indent_() << "Creating a new report for context: " << context << std::endl;

                    for(auto& cx : context){
                        if(largest_context_uid_ == MAX_NAV_NODE_UID){
                            throw SpartaException("Ran out of unique node UIDs when parsing a YAML Report ")
                                                << "definition \"" << getFilename() << "\". This is a bug."
                                                "Either the report definition was parsed with some bad "
                                                "UIDs to begin with or runaway subreport creation took "
                                                "place";
                        }
                        ++largest_context_uid_;

                        auto itr = report_map_.find(cx->uid);
                        sparta_assert(itr != report_map_.end(),
                                          "Somehow encountered an internal report map missing an "
                                          "entry or uid" << cx->uid << ". This is a report "
                                          "definition parser bug.");
                        Report* const r = itr->second;
                        Report* subrep = &r->addSubreport("");
                        sparta_assert(report_map_.find(largest_context_uid_) == report_map_.end()); // This should be a new ID
                        report_map_[largest_context_uid_] = subrep;

                        // Cannot immediately update the context because it should still refer to
                        // its prior report in case after leaving this report block other content is
                        // added to the parent report. Therefore, a temporary map must be created
                        // which describes how to supply the next generation of node UIDs in the
                        // overridden getNextNodeID_.
                        //////cx->uid = largest_context_uid_; // Update user data in context to contain UID to this report.
                        next_uid_map_[{cx->uid, cx->first}] = largest_context_uid_;
                        verbose() << indent_() << "Inserting new entry {uid=" << cx->uid << ", node="
                                  << cx->first << "} -> " << largest_context_uid_ << " into next_uid_map_ when creating subreport. map size = "
                                  << next_uid_map_.size() << std::endl;

                    }

                    //report_stack_.push(subrep); // Track this for sanity checking
                    report_stack_.push({}); // Track this for sanity checking

                    in_content_stack_.push(false);
                    return false;
                }else if(key == KEY_AUTOPOPULATE){
                    // Make size of the autopop args block nonzero to
                    // indicate it is open.
                    current_autopop_block_["current"] = "";
                    verbose() << indent_() << " handleEnterMap_ got a key KEY_AUTOPOPULATE"
                              << std::endl;
                    return false;
                }else if(key == KEY_CONTENT){
                    std::stringstream ss;
                    ss << "Unexpected key \"content\" within a \"content\" section";
                    addError_(ss.str());
                }
                return true; // Handle normally
            }else{
                if(key == KEY_SUBREPORT){
                    throw SpartaException("Unexpected key \"") << KEY_SUBREPORT << "\" outside of a "
                        "\"content\" section. Report definition files that are not included by "
                        "other report definitions are implicitly within a report. Any subreports "
                        "must be added within a 'content' section of a report or other subreport";
                }else if(key == KEY_CONTENT){
                    // Entered the content section of the report
                    in_content_stack_.push(true);
                    return false;
                }else if(key == KEY_STYLE){
                    // Entered a style block
                    style_block_.reset(new typename decltype(style_block_)::element_type());
                    in_content_stack_.push(false);
                    return false;
                }else if(key == KEY_TRIGGER){
                    if (trigger_defn_ != nullptr) {
                        throw SpartaException("Encountered a nested trigger while parsing "
                                            "a report definition file");
                    }
                    trigger_defn_.reset(new std::unordered_map<std::string, std::string>());
                    in_content_stack_.push(false);
                    return false;
                }else{

                    //std::stringstream ss;
                    //ss << "Unexpected map start (key = \"" << key << "\") outside of a \"content\" section";
                    //addError_(ss.str());7

                    // Prior implementation
                    in_content_stack_.push(false); // Still outside of content
                    return true; // Handle normally
                }
            }

            in_content_stack_.push(in_content); // Maintain in_content state

            return true;
        }

        bool handleExitMap_(const std::string& key,
                            const NavVector& context) override {
            in_content_stack_.pop(); // Pop of old state
            sparta_assert(!in_content_stack_.empty());
            bool in_content = in_content_stack_.top(); // Use parent's prior in_content state

            verbose() << indent_() << "handleExitMap_ with key = \"" << key << "\" and in_content = "
                      << in_content << " and current_autopop_block_.size() = "
                      << current_autopop_block_.size() << std::endl;

            if(key == KEY_REPORT_IGNORE) {
                in_ignore_ = false;
                return true;
            }

            if(key == KEY_REPORT_OPTIONAL) {
                in_optional_ = false;
                return true;
            }

            if(in_content){
                sparta_assert(style_block_ == nullptr,
                                  "Exited map while being inside a style block. Autopopulation "
                                  "blocks should have 1 level of key-value pairs only");

                in_content_stack_.push(true); // Still in content
                if(key == KEY_SUBREPORT){
                    // Close off reports in current context
                    verbose() << indent_() << "Exiting construction of subreports for context: "
                              << context << std::endl;

                    for(auto& cx : context){
                        // Nothing really needs to happen. But for sanity checking, remove the
                        // entries from the map to guarantee they won't be executed later
                        Report* const r = report_map_.at(cx->uid); // Get the report for this context
                        report_map_.erase(cx->uid);
                        verbose() << indent_() << "  (Ended subreport \"" << r->getName() << "\")"
                                  << std::endl;
                    }

                    // Ensure base report is always present
                    if(report_stack_.size() == 0) {
                        throw SpartaException("Exited more report blocks than were entered, report "
                                            "stack became empty while parsing \"")
                                            << getFilename() << "\"";
                    }

                    // Close off this report and remove from the stack
                    //Report* const r = report_stack_.top();
                    report_stack_.pop(); // Track for sanity checking

                    return false;
                }else if(key == KEY_CONTENT){
                    return false;
                }else if(key == KEY_AUTOPOPULATE){
                    sparta_assert(current_autopop_block_.size() != 0,
                                      "Exited map keyed as an autopopulation block. Somehow, the "
                                      "parser had no autopopulation block being tracked");

                    std::string attr_filter = "";
                    if(current_autopop_block_.count(KEY_AUTOPOPULATE_ATTRIBUTES) > 0){
                        attr_filter = current_autopop_block_[KEY_AUTOPOPULATE_ATTRIBUTES];
                    }
                    int32_t max_recursion_depth = -1;
                    if(current_autopop_block_.count(KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH) > 0){
                        size_t end_pos;
                        max_recursion_depth = utils::smartLexicalCast<int32_t>(current_autopop_block_[KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH], end_pos);
                    }
                    int32_t max_report_depth = -1;
                    if(current_autopop_block_.count(KEY_AUTOPOPULATE_MAX_REPORT_DEPTH) > 0){
                        size_t end_pos;
                        max_report_depth = utils::smartLexicalCast<int32_t>(current_autopop_block_[KEY_AUTOPOPULATE_MAX_REPORT_DEPTH], end_pos);
                    }

                    // Invoke autopoulate for each node/capture in the current context
                    //Report* const r = report_stack_.top();
                    for(auto& cx : context){
                        const TreeNode* n = cx->first;
                        const std::vector<std::string>& captures = cx->second;
                        Report* const r = report_map_.at(cx->uid); // Get the report for this context
                        verbose() << indent_() << "Autopopulating " << cx << " for "
                                  << n->getLocation() << " report = " << r->getName() << std::endl;
                        r->autoPopulate(n, attr_filter, captures, max_recursion_depth, max_report_depth);
                    }

                    current_autopop_block_.clear();
                    verbose() << indent_() << "Exiting construction of an autopopulation block"
                              << std::endl;
                    return false;
                }
                return true; // Handle normally
            }else{
                sparta_assert(current_autopop_block_.size() == 0,
                                  "Exited map while being inside an autopopulation block. "
                                  "Autopopulation blocks should have 1 level of key-value pairs "
                                  "only");

                if(key == KEY_STYLE){
                    sparta_assert(style_block_ != nullptr,
                                      "Exited map keyed as a style block. Somehow, the parser had "
                                      "no style block being tracked");

                    // Transfer the styles to the current report
                    sparta_assert(report_stack_.size() > 0);
                    for(auto& cx : context){
                        Report* const r = report_map_.at(cx->uid); // Get the report for this context
                        verbose() << indent_() << "Setting Styles at context for report \""
                                  << r->getName() << "\"" << std::endl;
                        for(auto const & kv : *style_block_){
                            verbose() << indent_() << "  style=" << kv.first << " value="
                                      << kv.second << std::endl;
                            r->setStyle(kv.first, kv.second);
                        }
                    }

                    style_block_.reset(nullptr);
                    return false;
                }else if(key == KEY_SUBREPORT){
                    return false;
                }else if(key == KEY_CONTENT){
                    return false;
                }else if(key == KEY_TRIGGER){
                    if(trigger_defn_ != nullptr){
                        for(auto& cx : context){
                            Report* const r = report_map_.at(cx->uid);
                            verbose() << indent_() << "Setting trigger(s) at context for report \""
                                      << r->getName() << "\"" << std::endl;
                            r->handleParsedTrigger(*trigger_defn_, cx->first);
                        }
                    }
                    trigger_defn_.reset();
                }
            }

            return true; // Handle normally
        }

        /*!
         * \brief Hanle next-node generation in a way that a next generation of
         * nodes is assigned specific new UIDs based on what report was created
         */
        virtual node_uid_t getNextNodeID_(const NavNode* parent,
                                          const TreeNode* node,
                                          const std::vector<std::string> & substitutions) override {
            (void) node;
            (void) substitutions;

            if(!parent){
                return 0;
            }

            auto itr = next_uid_map_.find({parent->uid, parent->first});
            if(itr == next_uid_map_.end()){
                // Inherit form parent, no entry in the map
                verbose() << indent_() << "(getNextNodeID_) parent entry: " << *parent
                          << " not found. in map. Inheriting parent uid " << parent->uid << std::endl;
                verbose() << indent_() << "(getNextNodeID_) next uid map (" << next_uid_map_.size()
                          << " entries):" << std::endl;
                for(auto& e : next_uid_map_){
                    verbose() << indent_() << "  " << e.first << " " << e.second << std::endl;
                }
                return parent->uid;
            }

            // There is an entry, use the UID that it directs us to
            verbose() << indent_() << "(getNextNodeID_) CHILD UID FOUND: " << itr->first << " -> "
                      << itr->second << std::endl;
            return itr->second;
        }

        /*!
         * \brief Replaces "%n" (where n is some integer) and %l instances in
         * full_name with the content of replacements[n+1] or the node \a n's
         * location. Only single-digit integers are currently supported
         * \param full_name Name to perform replacements on (in-place)
         * \param replacements Vector of replacements stings substituted for
         * each %n in full_name. Subsitution chosen is replacements[n+1]. N
         * can be any number of decimal integer digits
         * \return true if a valid name expression was parsed, false if not
         * (e.g. bad "% sequence or index referred to replacement index
         * available in \a replacements
         */
        bool replaceByIndex(std::string& full_name,
                            const TreeNode* n,
                            const std::vector<std::string>& replacements) {

            replaceSubstring(full_name, "%l", n->getLocation());

            size_t pos = 0;
            while(std::string::npos != (pos = full_name.find('%', pos))){
                if(pos == full_name.size()-1){
                    std::stringstream ss;
                    ss << "Encountered stat name \"" << full_name << "\" in this report "
                          "which that ended with a '%' without a following formatting "
                          "character or number";
                    addError_(ss.str());
                    return false;
                }

                // Get the integer following t he '%' char
                std::stringstream fnss;
                fnss << full_name.substr(pos + 1);
                int32_t idx = 0;
                fnss >> idx;
                if(fnss.fail()){
                    std::stringstream ss;
                    ss << "Encountered stat name \"" << full_name << "\" in this report"
                          " which contained a '%' followed by something other than a "
                          "'l' or an integer. '%' is not a valid character in a "
                          "final stat name and must be a substitution";
                    addError_(ss.str());
                    pos++; // Move over the current '%' char
                    return false;
                }else{
                    // Position following index numerals
                    size_t remainder_pos;
                    if(fnss.tellg() < 0){
                        remainder_pos = full_name.size();
                    }else{
                        remainder_pos = pos + 1 + fnss.tellg();
                    }
                    if(idx > 0 && idx > (int64_t)replacements.size()){
                        std::stringstream ss;
                        ss << "Encountered stat name \"" << full_name << "\" in this report"
                              " which contained a '%' followed by " << idx << " which does "
                              "not refer to a wildcard replacement performed in this path. "
                              "Available replacements are (starting with %1): " << replacements;
                        addError_(ss.str());
                        pos++; // Move over the current '%' char
                        return false;
                    }else if(idx < 0 && -idx > (int64_t)replacements.size()){
                        std::stringstream ss;
                        ss << "Encountered stat name \"" << full_name << "\" in this report"
                              " which contained a '%' followed by " << idx << " which does "
                              "not refer to a wildcard replacement performed in this path. "
                              "Available replacements are (starting with %1): " << replacements;
                        addError_(ss.str());
                        pos++; // Move over the current '%' char
                        return false;
                    }else if(idx == 0){
                        // Full location replacement
                        //! \todo Consider switching this to only the
                        //! relative path captured in the configuration file
                        if(nullptr == n){
                            std::stringstream ss;
                            ss << "Encountered stat name \"" << full_name << "\" in this report"
                                  " with a %0 replacement. However, this context does not refer to any "
                                  "specific TreeNode, so the full location cannot be used as a "
                                  "substitution here.";
                            addError_(ss.str());
                            return false;
                        }else{
                            const std::string& loc = n->getLocation();
                            full_name = full_name.substr(0, pos) + loc + \
                            full_name.substr(remainder_pos);
                            pos += loc.size();
                        }
                    }else{
                        // Replace with captured content
                        if(idx < 0){
                          // idx of -1 refers to replacements[replacements.size() - 1]
                          idx = idx + (int64_t)replacements.size();
                        }else{
                          idx -= 1; // idx of 1 refers to replacements[0]
                        }
                        const std::string& str = replacements[idx];
                        full_name = full_name.substr(0, pos) + str + \
                            full_name.substr(remainder_pos);
                        pos += str.size();
                    }
                } // }else{ // if(fnss.fail()){
            } // while(std::string::npos != (pos = full_name.find('%', pos))){

            return true;
        }

        /*!
         * \brief Performs all supported substitutions on the report name or
         * keyword name.
         * \param full_name Name to perform in-place substitutions on
         * \param n Node at which substitution is taking place (for location
         * substitutions)
         * \return Whether the replacements were successful. If false, errors
         * were added via addError_
         */
        bool getSubstituteForStatName(std::string& full_name,
                                      const TreeNode* n,
                                      const std::vector<std::string>& replacements) {
            sparta_assert(n);

            replaceSubstring(full_name, "%l", n->getLocation());

            // Substitution Not yet supported:
            //replaceSubstring(full_name, "%i", idx_str.str());

            // Capture substitutions
            return replaceByIndex(full_name, n, replacements);
        }

    }; // class ReportFileEventHandlerYAML

public:

    /*!
     * \brief Report definition file parser.
     * \param filename Name of file to open. Can be relative or absolute
     */
    ReportFileParserYAML(const std::string& filename) :
        fin_(filename.c_str(), std::ios::in),
        parser_(fin_),
        filename_(filename)
    {
        if(false == fin_.is_open()){
            throw SpartaException("Failed to open YAML Report definition file for read \"")
                << filename << "\"";
        }
    }

    /*!
     * \brief Report definition file parser with istream input
     * \param filename Name of file to open. Can be relative or absolute
     */
    ReportFileParserYAML(std::istream& content) :
        fin_(),
        parser_(content),
        filename_("<istream>")
    {
    }

    virtual ~ReportFileParserYAML() { }

    /*!
     * \brief Reads report content from YAML file.
     * \param r Report to which content read from this report definition file
     * will be added. Must not be nullptr
     * \param device_trees Any nodes in a device tree to use as the roots
     * for resolving node names found in YAML file to device tree nodes.
     * Resolving is done for every tree so there can be multiple results
     * node names at the outer-most level in the YAML file will be
     * resolved to descendants of this <device_tree> node. Must not be nullptr.
     * \param in_content Is the owner of this parser within the content section
     * of some report definition file. This is typically false except when
     * "#include"-ing a yaml file from within a content section of another
     * \param verbose Display verbose output messages to stdout/stderr
     * \throw SpartaException on failure
     * \note Filestream is NOT closed after this call.
     *
     * Any key nodes in the input file which cannot be resolved to at least
     * 1 device tree node will generate an exception.
     *
     * Any leaf value or sequence in the input file will be treated as a
     * parameter value. If they key with which that value is associated
     * in the input file does not resolve to a Parameter node in the
     * device tree, it will generate an exception.
     */
    void consumeReportFile(Report* r,
                           YAMLTreeEventHandler::NavVector device_trees,
                           bool in_content=false,
                           bool verbose=false)
    {
        sparta_assert(r);
        sparta_assert(device_trees.size() > 0);

        if(verbose){
            std::cout << "Reading report definition from \"" << filename_
                      << "\"" << std::endl;
        }

        ReportFileEventHandlerYAML handler(r,
                                           filename_,
                                           device_trees,
                                           in_content,
                                           verbose);
        while(parser_.HandleNextDocument(*((YP::EventHandler*)&handler))) {}

        if(handler.getErrors().size() != 0){
            SpartaException ex("One or more errors detected while consuming the report definition "
                             "file:\n");
            for(const std::string& es : handler.getErrors()){
                ex << es << '\n';
            }
            throw  ex;
        }

        if(verbose){
            if(handler.getWarnings().size() != 0){
                std::cout <<
                    "One or more warnings detected while consuming the report definition file:\n";
                for(const std::string& es : handler.getWarnings()){
                    std::cout << es << '\n';
                }
            }
            std::cout << "Done reading report definition from \"" << filename_ << "\"" << std::endl;
        }
    }

private:

    std::ifstream fin_;    //!< Input file stream. Opened at construction
    YP::Parser parser_;    //!< YP::Parser to which events will be written
    std::string filename_; //!< For recalling errors
}; // class ReportFileParserYAML

constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_REPORT[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_SUBREPORT[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_CONTENT[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_NAME[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_AUTHOR[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_AUTOPOPULATE[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_AUTOPOPULATE_ATTRIBUTES[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_AUTOPOPULATE_MAX_RECURSION_DEPTH[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_AUTOPOPULATE_MAX_REPORT_DEPTH[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_STYLE[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_TRIGGER[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_REPORT_IGNORE[];
constexpr char ReportFileParserYAML::ReportFileEventHandlerYAML::KEY_REPORT_OPTIONAL[];

void Report::addFile(const std::string& file_path, bool verbose)
{
    const std::vector<std::string> replacements;
    addFileWithReplacements(file_path, replacements, verbose);
}

void Report::addDefinitionString(const std::string& content, bool verbose)
{
    const std::vector<std::string> replacements;
    addDefinitionStringWithReplacements(content, replacements, verbose);
}

void Report::addFileWithReplacements(const std::string& file_path,
                                     const std::vector<std::string>& replacements,
                                     bool verbose)
{
    TreeNode* context = getContext();
    if(!context){
        throw SpartaException("Cannot add a report definition file \"") << file_path
            << "\" to a Report when that report does not have a context node. One must be set with "
               "Report::setContext";
    }

    ReportFileParserYAML yaml(file_path);
    std::shared_ptr<YAMLTreeEventHandler::NavNode> scope(new YAMLTreeEventHandler::NavNode({nullptr, context, replacements, 0}));
    yaml.consumeReportFile(this,
                           {scope},
                           false, // Not in a context block at start
                           verbose);
}

void Report::addDefinitionStringWithReplacements(const std::string& content,
                                                 const std::vector<std::string>& replacements,
                                                 bool verbose)
{
    TreeNode* context = getContext();
    if(!context){
        throw SpartaException("Cannot add a report definition string \"") << content
            << "\" to a Report when that report does not have a context node. One must be set with "
               "Report::setContext";
    }

    std::stringstream ss(content);
    ReportFileParserYAML yaml(ss);
    std::shared_ptr<YAMLTreeEventHandler::NavNode> scope(new YAMLTreeEventHandler::NavNode({nullptr, context, replacements, 0}));
    yaml.consumeReportFile(this,
                           {scope},
                           false, // Not in a context block at start
                           verbose);
}

Report& Report::addSubreport(const std::string& name) {
    subreps_.emplace_back(name, context_, scheduler_);
    subreps_.back().setParent_(this);
    if (report_container_ == nullptr) {
        report_container_.reset(new sparta::SubContainer);
    }
    subreps_.back().report_container_ = report_container_;
    return subreps_.back();
}

Report& Report::addSubreport(const Report& r) {
    subreps_.emplace_back(r);
    subreps_.back().setParent_(this);
    if (report_container_ == nullptr) {
        report_container_.reset(new sparta::SubContainer);
    }
    subreps_.back().report_container_ = report_container_;
    return subreps_.back();
}

void Report::addSubtree(const TreeNode* n,
                        subreport_decision_fxn_t make_sr_fxn,
                        inclusion_decision_fxn_t branch_inc_fxn,
                        inclusion_decision_fxn_t leaf_inc_fxn,
                        bool add_counters,
                        bool add_stats,
                        int32_t max_recurs_depth)
{
    recursAddSubtree_(n,
                      make_sr_fxn, branch_inc_fxn, leaf_inc_fxn,
                      add_counters, add_stats,
                      max_recurs_depth,
                      0, // recurs_depth
                      0, // report_depth
                      ""); // stat_prefix
}

void Report::recursAddSubtree_(const TreeNode* n,
                               subreport_decision_fxn_t make_sr_fxn,
                               inclusion_decision_fxn_t branch_inc_fxn,
                               inclusion_decision_fxn_t leaf_inc_fxn,
                               bool add_counters,
                               bool add_stats,
                               int32_t max_recurs_depth,
                               uint32_t recurs_depth,
                               uint32_t report_depth,
                               const std::string& stat_prefix)
{
    sparta_assert(n != nullptr);

    const bool is_ctr = dynamic_cast<const CounterBase*>(n) != nullptr;
    const bool is_stat = dynamic_cast<const StatisticDef*>(n) != nullptr;

    std::string child_stat_prefix = stat_prefix;

    //! \todo Add ability include parameters. This will likely need to be an
    //! additional input option
    if((add_counters && is_ctr) || (add_stats && is_stat)){
        if(leaf_inc_fxn == nullptr || leaf_inc_fxn(n)){
            //std::cerr << "\n\nAdding " << n->getLocation() << " to " << getName() << std::endl
            //          << "Report: " << getName() << std::endl;
            //for(auto& s : stats_){
            //    std::cerr << "  Stat " << s.second->stringize() << " " << s.second->getLocation() << std::endl;
            //}
            add(n, child_stat_prefix + n->getName());
        }
    }

    // Add subreport, it at all, after handling this node's content.
    // First node in recursive addSubtree call should not get its own report
    Report* r = this;
    if(recurs_depth > 0){
        std::string subreport_name;
        bool make_child_sr;
        if(make_sr_fxn != nullptr
           && make_sr_fxn(n, subreport_name, make_child_sr, report_depth)){
            // Must track r but not recurs here in order to prevent recursion on
            // subreport creation
            if(make_child_sr || parent_ == nullptr){
                report_depth++;
                r = &addSubreport(subreport_name);
            }else{
                // Sibling subreport
                r = &parent_->addSubreport(subreport_name);
            }

            // Made a new sureport. Clear this prefix since all children will be local to the subreport
            child_stat_prefix = "";
        }else{
            // Did not make a new subreport here.
            r = this;
            if(child_stat_prefix.size() == 0 && dynamic_cast<const StatisticSet*>(n) != nullptr){
                // This is a statisticSet and the stat prefix is empty. Do not append this name
                // ('stats') because it would be ugly. If there were a prefix, the name 'stats'
                // would be needed for correctness
            }else{
                //  Add the name of the node to the prefix
                child_stat_prefix += n->getName() + ".";
            }
        }
    }

    // If recursion depth max is reached, do not go into subtree.
    // Note that this is done after getting all local stats/counters.
    // The plus one is added to recurs depth becuase the depth stops recursion
    // at branch nodes, but we can still look at the leaves of those branch
    // nodes, which is done above.
    if(max_recurs_depth >= 0 && recurs_depth >= (uint32_t)max_recurs_depth + 1){
        //std::cerr << " Recurs depth stoppped at " << n->getLocation() << std::endl;
        return;
    }

    // Recurs into this branch node. Note that this is done regardless
    // of whether this was added as a counter or a stat
    if(branch_inc_fxn == nullptr || branch_inc_fxn(n)){
        for(TreeNode* child : TreeNodePrivateAttorney::getAllChildren(n)){

            // Recursively add children from the sparta tree
            r->recursAddSubtree_(child,
                                 make_sr_fxn, branch_inc_fxn, leaf_inc_fxn,
                                 add_counters, add_stats,
                                 max_recurs_depth,
                                 recurs_depth + 1,
                                 report_depth + 1,
                                 child_stat_prefix);

        }

        // Remove the subreport if it had no stats
        if((r != this) && (r->getRecursiveNumStatistics() == 0) && (r->getParent() != nullptr)){
            sparta_assert(r->getParent()->removeSubreport(*r) == 1);
        }
    }
}

void Report::autoPopulate(const TreeNode* n,
                          const std::string& attribute_expr,
                          const std::vector<std::string>& captures,
                          int32_t max_recurs_depth,
                          int32_t max_report_depth)
{
    (void) captures;

    tree::filter::Parser tfp;
    tree::filter::Expression ex;
    if(attribute_expr == ""){
        ex = tree::filter::Expression(true);
    }else{
        ex = tfp.parse(attribute_expr);
    }

    auto make_sr_fxn = [=](const TreeNode* tn,
                           std::string& rep_name,
                           bool& make_child_sr,
                           uint32_t report_depth) -> bool {

        // Do not make a new subreport if max report depth is reached
        if(max_report_depth >= 0 && (report_depth >= (uint32_t)max_report_depth + 1)){
            make_child_sr = false;
        }else{
            make_child_sr = true;
        }

        // Note: Cannot currently test for DynamicResourceTreeNode without
        // knowing its template types. DynamicResourceTreeNode will need to
        // have a base class that is not TreeNode which can be used here.
        if(dynamic_cast<const ResourceTreeNode*>(tn) != nullptr
           || dynamic_cast<const RootTreeNode*>(tn) != nullptr
           || tn->hasChild(StatisticSet::NODE_NAME)){
            rep_name = tn->getLocation(); // Use location as report name
            return true;
        }
        return false;
    };
    auto filt_leaf_fxn = [=](const TreeNode* n) -> bool {
        return ex.valid(n);
    };
    addSubtree(n,
               make_sr_fxn,
               nullptr, // Do not filter branches
               filt_leaf_fxn,
               true,
               true,
               max_recurs_depth);
}

/*!
 * \brief Let objects know if this report has any triggered behavior
 * for any purpose (this will recurse into all subreports from this
 * report node)
 */
bool Report::hasTriggeredBehavior() const
{
    if (report_start_trigger_ != nullptr || report_stop_trigger_ != nullptr) {
        return true;
    }

    //Assume no triggered behavior until found otherwise
    bool is_triggered = false;

    for (const Report & r : this->getSubreports()) {
        is_triggered |= r.hasTriggeredBehavior();
        if (is_triggered) {
            return true;
        }
    }

    return false;
}

/*!
 * \brief Query whether this report can be considered ready for statistics
 * printouts (triggered behavior under the hood can render the report "dormant"
 * during warmup periods, cool down periods, etc.)
 *
 * Keep in mind that just because a report responds TRUE one time does
 * not mean that it is always active for stats printouts to file.
 */
bool Report::isActive() const
{
    //Assume active until found otherwise
    bool report_node_is_active = true;
    if (report_start_trigger_ != nullptr) {
        report_node_is_active = report_start_trigger_->hasFired();
        if (!report_node_is_active) {
            return false;
        }
    }

    if (report_stop_trigger_ != nullptr) {
        report_node_is_active &= !report_stop_trigger_->hasFired();
        if (!report_node_is_active) {
            return false;
        }
    }

    for (const auto & r : this->getSubreports()) {
        report_node_is_active &= r.isActive();
        if (!report_node_is_active) {
            return false;
        }
    }

    return true;
}

/*!
 * \brief Reports can consume definition YAML entries specifying start
 * and stop behavior, and thus should own those trigger objects.
 */
void Report::handleParsedTrigger(
    const std::unordered_map<std::string, std::string> & kv_pairs,
    TreeNode * context)
{
    sparta_assert(context);
    sparta_assert(!kv_pairs.empty());

    auto ref_tag = kv_pairs.find("tag");

    //Simple expressions like "core0.rob.stats.total_number_retired >= 100"
    //need to be handled the exact same way as always (just one CounterTrigger
    //as if we had owned it all along) - switch callbacks if possible
    trigger::ExpressionTrigger::SingleCounterTrigCallback legacy_start_cb = std::bind(
        &Report::legacyDelayedStart_, this, std::placeholders::_1);

    trigger::ExpressionTrigger::SingleCounterTrigCallback legacy_stop_cb = std::bind(
        &Report::legacyDelayedEnd_, this, std::placeholders::_1);

    //Set up the start trigger
    auto start = kv_pairs.find("start");
    if (start != kv_pairs.end()) {
        const std::string expression = start->second;
        SpartaHandler cb = CREATE_SPARTA_HANDLER(Report, start);

        report_start_trigger_.reset(new trigger::ExpressionTrigger(
            "ReportSetup", cb, expression, context, report_container_));

        if (ref_tag != kv_pairs.end()) {
            const std::string tag = ref_tag->second;
            report_start_trigger_->setReferenceEvent(tag, "start");
        }

        legacy_start_trigger_ = report_start_trigger_->
            switchToSingleCounterTriggerCallbackIfAble(legacy_start_cb);
    }

    //Set up the stop trigger
    auto stop = kv_pairs.find("stop");
    if (stop != kv_pairs.end()) {
        const std::string expression = stop->second;
        SpartaHandler cb = CREATE_SPARTA_HANDLER(Report, end);

        report_stop_trigger_.reset(new trigger::ExpressionTrigger(
            "ReportTeardown", cb, expression, context, report_container_));

        if (ref_tag != kv_pairs.end()) {
            const std::string tag = ref_tag->second;
            report_stop_trigger_->setReferenceEvent(tag, "stop");
        }

        legacy_stop_trigger_ = report_stop_trigger_->
            switchToSingleCounterTriggerCallbackIfAble(legacy_stop_cb);
    }
}

/*!
 * \brief Callbacks for diagnostic / trigger status printout on report start and stop
 */
void Report::legacyDelayedStart_(const trigger::CounterTrigger * trigger)
{
    sparta_assert(legacy_start_trigger_);

    auto ctr = trigger->getCounter();
    auto clk = trigger->getClock();
    std::cout << "     [trigger] Now starting report '" << this->getName()
              << "' after warmup delay of " << trigger->getTriggerPoint()
              << " on counter: " << ctr << ". Occurred at tick "
              << scheduler_->getCurrentTick() << " and cycle "
              << clk->currentCycle() << " on clock " << clk << std::endl;

    this->start();
}

void Report::legacyDelayedEnd_(const trigger::CounterTrigger * trigger)
{
    sparta_assert(legacy_stop_trigger_);

    auto ctr = trigger->getCounter();
    auto clk = trigger->getClock();
    std::cout << "     [trigger] Now stopping report '" << this->getName()
              << "' after specified terminate of " << trigger->getTriggerPoint()
              << " on counter: " << ctr << ". Occurred at tick "
              << scheduler_->getCurrentTick() << " and cycle "
              << clk->currentCycle() << " on clock " << clk << std::endl;

    this->end();
}

report::format::ReportHeader & Report::getHeader() const
{
    if (header_ == nullptr) {
        header_.reset(new report::format::ReportHeader);
    }
    return *header_;
}

bool Report::hasHeader() const
{
    return (header_ != nullptr);
}

/*!
 * \brief Reconstruct a StatisticInstance from a record found
 * in the given database.
 */
std::unique_ptr<StatisticInstance> createSIFromSimDB(
    const simdb::DatabaseID report_hier_node_id,
    const simdb::ObjectManager & obj_mgr)
{
    std::vector<std::pair<std::string, std::string>> metadata;
    std::string location, description, expr_string;
    uint32_t value_semantic, visibility;
    int cls;

    obj_mgr.safeTransaction([&]() {
        //Start out by using the incoming node ID to query
        //the SIMetadata table for basic SI properties like
        //location, description, visibility, etc.
        std::unique_ptr<simdb::ObjectQuery> si_meta_query(
            new simdb::ObjectQuery(obj_mgr, "SIMetadata"));

        si_meta_query->addConstraints(
            "ReportNodeID", simdb::constraints::equal, report_hier_node_id);

        si_meta_query->writeResultIterationsTo(
            "Location", &location,
            "Desc", &description,
            "ExprString", &expr_string,
            "ValueSemantic", &value_semantic,
            "Visibility", &visibility,
            "Class", &cls);

        auto result_iter = si_meta_query->executeQuery();
        if (!result_iter->getNext()) {
            throw SpartaException("Unable to locate SIMetadata record ")
                << "with ReportNodeID " << report_hier_node_id;
        }

        //Empty string metadata values are stored with default
        //value "unset" in the database. Inherit these values
        //now, or throw if they were never set.
        if (location.empty() || location == "unset") {
            throw SpartaException("SIMetadata record with ReportNodeID ")
                << report_hier_node_id << " did not have its Location "
                << "column set";
        }

        if (description.empty() || description == "unset") {
            throw SpartaException("SIMetadata record with ReportNodeID ")
                << report_hier_node_id << " did not have its Desc column set";
        }

        if (expr_string.empty() || expr_string == "unset") {
            throw SpartaException("SIMetadata record with ReportNodeID ")
                << report_hier_node_id << " did not have its ExprString "
                << "column set";
        }

        //Tack on any additional metadata we can find
        si_meta_query.reset(new simdb::ObjectQuery(
            obj_mgr, "RootReportNodeMetadata"));

        si_meta_query->addConstraints(
            "ReportNodeID", simdb::constraints::equal, report_hier_node_id);

        std::string meta_name, meta_value;
        si_meta_query->writeResultIterationsTo(
            "Name", &meta_name, "Value", &meta_value);

        result_iter = si_meta_query->executeQuery();
        while (result_iter->getNext()) {
            if (!meta_name.empty() && !meta_value.empty()) {
                metadata.emplace_back(meta_name, meta_value);
            }
        }
    });

    std::unique_ptr<StatisticInstance> si(
        new StatisticInstance(location, description, expr_string,
                              (StatisticDef::ValueSemantic)value_semantic,
                              (InstrumentationNode::visibility_t)visibility,
                              (InstrumentationNode::class_t)cls,
                              metadata));
    return si;
}

/*!
 * \brief This method lets SimDB-recreated Report objects
 * set placeholders this SI will soon use to get
 * SI data values directly from a SimDB blob (not
 * from an actual simulation).
 */
void StatisticInstance::setSIValueDirectLookupPlaceholder(
    const std::shared_ptr<sparta::StatInstValueLookup> & direct_lookup)
{
    direct_lookup_si_value_ = direct_lookup;
}

/*!
 * \brief Our StatInstValueLookup *placeholder* object
 * needs to bind itself to a StatInstRowIterator object,
 * since these two classes go hand in hand. Now that we're
 * being given the row iterator, we can use it to "realize"
 * our "SI direct value lookup" object now.
 */
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

/*!
 * \brief If this SI is using a StatInstValueLookup object
 * to get its SI values, ask if this direct-lookup object
 * can be used to get the current SI value.
 */
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

/*!
 * \brief Ask the StatInstValueLookup object for our current
 * SI value. Throws an exception if the direct-value object
 * is not being used.
 */
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

/*!
 * \brief Starting with the given report node database ID,
 * find its root report node ID in the provided database.
 */
simdb::DatabaseID getRootReportNodeIDFrom(
    const simdb::DatabaseID report_hier_node_id,
    const simdb::ObjectManager & obj_mgr)
{
    simdb::ObjectQuery query(obj_mgr, "ReportNodeHierarchy");

    int parent_report_hier_node_id = 0;

    query.addConstraints(
        "Id", simdb::constraints::equal, report_hier_node_id);

    query.writeResultIterationsTo(
        "ParentNodeID", &parent_report_hier_node_id);

    auto result_iter = query.executeQuery();
    if (result_iter == nullptr) {
        return report_hier_node_id;
    }

    sparta_assert(result_iter->getNext());
    if (parent_report_hier_node_id == 0) {
        return report_hier_node_id;
    }

    return getRootReportNodeIDFrom(
        parent_report_hier_node_id, obj_mgr);
}

/*!
 * \brief Go through all reports/subreports and their SI's, and
 * ask all of these objects to verify their StatInstValueLookup
 * objects are ready to being reading SimDB data values.
 */
void recursValidateSIDirectLookups(const Report & report)
{
    for (const auto & si : report.getStatistics()) {
        if (!si.second->isSIValueDirectLookupValid()) {
            throw SpartaException("Encountered an SI whose StatInstValueLookup ")
                << "could not be read. This indicates there is a bug in the SimDB "
                << "tables/records related to report hierarchies.";
        }
    }

    for (const auto & sr : report.getSubreports()) {
        recursValidateSIDirectLookups(sr);
    }
}

/*!
 * \brief SimDB->report generation workflows invoke this constructor,
 * which recreates a Report object (and its subreports and all of
 * their SI's) from a database connection and a report node database
 * ID.
 */
Report::Report(const simdb::DatabaseID report_hier_node_id,
               const simdb::ObjectManager & obj_mgr,
               const Scheduler * scheduler) :
    Report("nameless", scheduler)
{
    obj_mgr.safeTransaction([&]() {
        //To recreate a report from a database entry, start with
        //the ReportNodeHierarchy table, looking for the one whose
        //primary key equals the given database ID.
        simdb::ObjectQuery hier_query(obj_mgr, "ReportNodeHierarchy");

        hier_query.addConstraints("Id", simdb::constraints::equal,
                                  report_hier_node_id);

        //Request the report's name when found.
        hier_query.writeResultIterationsTo("Name", &name_);

        auto result_iter = hier_query.executeQuery();
        if (!result_iter->getNext()) {
            throw SpartaException("Unable to locate ReportNodeHierarchy ")
                << "record with ReportNodeID " << report_hier_node_id;
        }

        //The hierarchy query we just ran was filtered against
        //a unique database ID, so we should never have more than
        //one match.
        if (result_iter->getNext()) {
            throw SpartaException("Found more than one record in the ")
                << "ReportNodeHierarchy table with ReportNodeID "
                << report_hier_node_id << ". This is a bug.";
        }

        //The next step is to get this report's metadata. Look
        //for the record in the ReportNodeMetadata table whose
        //ReportNodeID field (foreign key) matches the given
        //database ID (ReportNodeHierarchy primary key).
        simdb::ObjectQuery meta_query(obj_mgr, "ReportNodeMetadata");
        meta_query.addConstraints("ReportNodeID",
                                  simdb::constraints::equal,
                                  report_hier_node_id);

        //Request common metadata properties such as Start/EndTick, etc.
        meta_query.writeResultIterationsTo("Author", &author_,
                                           "InfoString", &info_string_,
                                           "StartTick", &start_tick_,
                                           "EndTick", &end_tick_);

        //The only way this query would fail is if the database ID
        //that was passed into this constructor was not a valid ID
        //in the ReportNodeMetadata table.
        result_iter = meta_query.executeQuery();
        if (!result_iter->getNext()) {
            throw SpartaException("Unable to locate ReportNodeMetadata ")
                << "record with ReportNodeID " << report_hier_node_id;
        }

        //The metadata query we just ran was also filtered against
        //a unique database ID. Verify there is only one such match
        //in this database.
        if (result_iter->getNext()) {
            throw SpartaException("Found more than one record in the ")
                << "ReportNodeMetadata table with ReportNodeID "
                << report_hier_node_id << ". This is a bug.";
        }

        //Empty metadata fields have the default value "unset".
        //Clear our member variables where this is the case.
        auto clear_unset_string = [](std::string & str) {
            if (str == "unset") {
                str.clear();
            }
        };
        clear_unset_string(name_);
        clear_unset_string(author_);
        clear_unset_string(info_string_);

        //Look for any records in the ReportStyle table whose
        //ReportNodeID field (foreign key) matches the given
        //database ID (ReportNodeHierarchy primary key).
        simdb::ObjectQuery style_query(obj_mgr, "ReportStyle");
        style_query.addConstraints("ReportNodeID",
                                   simdb::constraints::equal,
                                   report_hier_node_id);

        //Styles are given in simple name-value pairs.
        std::string style_name, style_value;
        style_query.writeResultIterationsTo("StyleName", &style_name,
                                            "StyleValue", &style_value);

        result_iter = style_query.executeQuery();

        //Unlike ReportNodeHierarchy, ReportStyle is not required.
        //The original report/subreport that was written to the
        //database may not have had any style info.
        if (result_iter) {
            while (result_iter->getNext()) {
                //But if there was such a style record, its name/value
                //columns should have been set.
                if (style_name.empty() || style_value.empty()) {
                    throw SpartaException("StyleName and/or StyleValue columns ")
                        << "were not set in the ReportStyle table for the record "
                        << "with ReportNodeID " << report_hier_node_id;
                }

                style_[style_name] = style_value;
            }
        }

        //Get the next generation of nodes under this report. These
        //could be subreport nodes or SI nodes, which we'll figure
        //out shortly.
        simdb::ObjectQuery nextgen_query(obj_mgr, "ReportNodeHierarchy");
        nextgen_query.addConstraints("ParentNodeID",
                                     simdb::constraints::equal,
                                     report_hier_node_id);

        //Request these next generation's Id field (primary key),
        //and their Name, IsLeafSI, and LeftmostSIIndex field
        //values.
        simdb::DatabaseID nextgen_node_id = 0;
        std::string nextgen_node_name;
        int is_leaf_si, leaf_si_index;

        nextgen_query.writeResultIterationsTo(
            "Id", &nextgen_node_id,
            "Name", &nextgen_node_name,
            "IsLeafSI", &is_leaf_si,
            "LeftmostSIIndex", &leaf_si_index);

        result_iter = nextgen_query.executeQuery();

        //Leaf SI info includes the SI name, its database ID (primary
        //key in ReportNodeHierarchy table), and its leaf SI index.
        //
        //The SI index will let us know the offset into the contiguous
        //SI double values vector, which we'll use to set up a link
        //between each leaf SI and its associated StatInstValueLookup.
        struct LeafSIInfo {
            std::string name;
            utils::ValidValue<simdb::DatabaseID> db_id;
            utils::ValidValue<size_t> linear_idx;
        };

        std::vector<LeafSIInfo> nextgen_si_leaves;
        std::vector<simdb::DatabaseID> nextgen_subreport_db_ids;
        while (result_iter->getNext()) {
            sparta_assert(nextgen_node_id > 0);
            sparta_assert(is_leaf_si == 0 || is_leaf_si == 1);

            if (is_leaf_si) {
                sparta_assert(!nextgen_node_name.empty());
                sparta_assert(nextgen_node_name != "unset");

                LeafSIInfo info;
                info.name = nextgen_node_name;
                info.db_id = nextgen_node_id;
                info.linear_idx = leaf_si_index;

                nextgen_si_leaves.emplace_back(info);
            } else {
                nextgen_subreport_db_ids.emplace_back(nextgen_node_id);
            }
        }

        report_node_id_ = report_hier_node_id;

        //Nothing left to do if there are no next-generation nodes.
        if (nextgen_si_leaves.empty() && nextgen_subreport_db_ids.empty()) {
            return;
        }

        //If we have any SI leaves beneath this report node, we need
        //to create a "StatInstRowIterator" right now. More specifically,
        //we need to create a "placeholder" for one of these iterator
        //objects.
        //
        //It works like this:
        //
        //    1) Recreate a sparta::Report from a database ID
        //       - This recursively creates subreports from other
        //         database ID's that it finds along the way
        //
        //    2) Out of all of the report/subreport nodes in this tree,
        //       we could use any of them to create a StatInstRowIterator
        //       which they could all share. Say we have the following tree:
        //
        //                              top
        //                            (id 1)
        //                   -------------------------
        //                 core0                  core1
        //                 (id 2)                 (id 3)
        //            ----------------               |
        //             |            |               baz
        //            foo          bar            (id 6)
        //          (id 4)       (id 5)
        //
        //A StatInstRowIterator is a wrapper around an ObjectQuery.
        //It runs a query for all SI blobs using the report *root ID*
        //in its query constraints. All nodes in this example tree
        //share the same report root ID of 1 ("top").
        //
        //The sparta::StatInstRowIterator constructor runs a query
        //against the SimDB, so it is not something we want to do
        //more times than we have to. However, the object we make
        //here is a placeholders::StatInstRowIterator (note the
        //namespace is different), which gives us a place to hold
        //all of our individual report node ID's until this entire
        //sparta::Report's recursive constructor calls are done. Then,
        //we will ask one of these placeholders to turn itself into
        //a "real" sparta::StatInstRowIterator object only once, and
        //share that object with all nodes in this report structure.
        //
        //This extra step of having placeholder objects like this
        //was preferred over passing in our 'this' pointer into
        //a sparta::Report constructor, denoting the parent:
        //
        //     //Somewhere in this constructor:
        //     subreps_.emplace_back(..., this);
        //
        //     //Which calls a constructor that looks like this:
        //     Report(..., Report * parent) :
        //         si_row_iterator_(parent->si_row_iterator_),
        //         parent_(parent)
        //     {
        //         //run other DB queries to get our metadata,
        //         //and recursively create more subreports if
        //         //needed
        //     }
        //
        //It seems preferrable to reuse the "addSubreport()" method
        //out of the box, to help guarantee the recreated hierarchy
        //is identical to the original one in the simulator. As
        //opposed to creating another private constructor to do the
        //same thing. Plus, this code just seems odd, and potentially
        //has a subtle bug in it somewhere:
        //
        //        subreps_.emplace_back(..., this);
        //        ********              *********
        //     this->subreps_    Report::ctor(..., parent) :
        //                         this->parent_(parent)
        //                       {}
        //
        if (!nextgen_si_leaves.empty()) {
            si_row_iterator_.reset(new placeholders::StatInstRowIterator(
                report_hier_node_id, &obj_mgr));
        }

        //Add StatisticInstance's to this report, if any. This
        //calls an SI constructor which takes a database ID which
        //it knows how to use to recreate the same SI from the
        //original simulation.
        for (const auto & leaf_si_db_info : nextgen_si_leaves) {
            auto simdb_si = createSIFromSimDB(leaf_si_db_info.db_id, obj_mgr);
            sparta_assert(simdb_si != nullptr);
            stats_.emplace_back(leaf_si_db_info.name, simdb_si.release());
            si_node_ids_.emplace_back(leaf_si_db_info.db_id);

            //Similar to the StatInstRowIterator for report nodes, we
            //give each SI a placeholders::StatInstValueLookup object.
            //The only info this placeholder needs up front is the leaf
            //SI index. We'll resolve these placeholders at the same
            //time we resolve the StatInstRowIterator placeholder(s).
            //This is done shortly after this constructor call in the
            //SimDB->report generation code path.
            std::shared_ptr<sparta::StatInstValueLookup> direct_lookup(
                new placeholders::StatInstValueLookup(
                    leaf_si_db_info.linear_idx.getValue()));

            stats_.back().second->setSIValueDirectLookupPlaceholder(
                direct_lookup);
        }

        //Add subreports to this report, if any. Calls this same
        //Report constructor with the appropriate database ID.
        for (const auto subreport_db_id : nextgen_subreport_db_ids) {
            Report subrep(subreport_db_id, obj_mgr, scheduler);
            addSubreport(subrep);
        }
    });
}

/*!
 * \brief During SimDB->report creation workflows, this method
 * is called which sets up database/table cursors to get ready
 * to iterate over SI records/blobs.
 */
bool Report::prepareForSIDatabaseIteration_(
    const simdb::ObjectManager & obj_mgr)
{
    //First clear out some report style metadata that was only
    //stashed in the database in order to make the appropriate
    //BaseFormatter API calls.
    style_.erase("OmitZero");
    style_.erase("PrettyPrintDisabled");

    //All report/subreport nodes in this tree can share the
    //same StatInstRowIterator object. Go to the root node,
    //fetch the first row iterator object we find (closest
    //to the root, i.e. short-circuit the tree walk once
    //we find one of these iterator objects).
    auto root = getRoot();

    //Walk the report/SI tree and gather all database ID's
    //for the reports, subreports, and all their SI's. This
    //will be used for ContextCounter hierarchy recreation.
    std::unordered_map<simdb::DatabaseID, Report*> report_nodes_by_id;
    std::unordered_map<simdb::DatabaseID, const StatisticInstance*> si_nodes_by_id;
    root->recursGetReportAndSINodeDatabaseIDs_(
        report_nodes_by_id, si_nodes_by_id);

    simdb::DatabaseID unprintable_si_id;
    simdb::ObjectQuery unprintable_sis_query(obj_mgr, "UnprintableSubStatistics");
    unprintable_sis_query.writeResultIterationsTo("ReportNodeID", &unprintable_si_id);

    auto unprintable_sis = std::make_shared<db::DatabaseContextCounter::UnprintableSIs>();
    auto result_iter = unprintable_sis_query.executeQuery();
    while (result_iter->getNext()) {
        auto unprintable_iter = si_nodes_by_id.find(unprintable_si_id);
        if (unprintable_iter != si_nodes_by_id.end()) {
            unprintable_sis->insert(unprintable_iter->second);
        }
    }

    //To get the same report formatting for ContextCounter's, we
    //have a "DatabaseContextCounter" class which mimics the
    //sparta::ContextCounter<T> class. But unlike the "real" Context
    //Counter class, DatabaseContextCounter is only responsible
    //for post-simulation formatting of ContextCounter data in
    //report files.
    //
    //To build up DatabaseContextCounter objects, we need to
    //look for some sub-statistics information in the database
    //that live in 'this' report.
    std::vector<simdb::DatabaseID> report_node_ids;
    report_node_ids.reserve(report_nodes_by_id.size());
    for (const auto & node : report_nodes_by_id) {
        report_node_ids.emplace_back(node.first);
    }

    simdb::ObjectQuery substats_query(obj_mgr, "SubStatisticsNodeHierarchy");

    substats_query.addConstraints(
        "ReportNodeID", simdb::constraints::in_set, report_node_ids);

    simdb::DatabaseID report_node_id, si_node_id, parent_si_node_id;
    substats_query.writeResultIterationsTo(
        "ReportNodeID", &report_node_id,
        "SINodeID", &si_node_id,
        "ParentSINodeID", &parent_si_node_id);

    //The DBSubStatisticInstances data structure used below is defined
    //as a mapping from SI's to their sub-SI's.
    //
    //To illustrate what this data structure represents, say that we had
    //the following hierarchy in the original SPARTA simulation:
    //    Report
    //      SI                       (wraps a ContextCounter)
    //        internal_counters_[0]  (wraps a CounterBase)
    //        internal_counters_[1]  (wraps a CounterBase)
    //
    //The equivalent hierarchy when recreating the same report *after*
    //simulation looks like this:
    //
    //    Report
    //      SI                       (is the root node of a DatabaseContextCounter)
    //        SI                     (is the first sub-statistic under it)
    //        SI                     (is the second sub-statistic under it)
    //
    result_iter = substats_query.executeQuery();
    while (result_iter->getNext()) {
        DBSubStatisticInstances & substats =
            report_nodes_by_id[report_node_id]->db_sub_statistics_;

        const StatisticInstance * cc_root = si_nodes_by_id[parent_si_node_id];
        auto substats_iter = substats.find(cc_root);
        if (substats_iter == substats.end()) {
            std::shared_ptr<db::DatabaseContextCounter> db_ctx_counter =
                std::make_shared<db::DatabaseContextCounter>(cc_root, unprintable_sis);

            substats[cc_root].first = db_ctx_counter;
            substats_iter = substats.find(cc_root);
        }

        substats_iter->second.second.emplace_back(si_nodes_by_id[si_node_id]);
    }

    //Now let's setup every Report's row iterator objects.
    //Start by getting a shared row iterator from the first
    //one we find starting at the root Report node.
    auto row_iterator_placeholder =
        root->recursFindTopmostSIRowIteratorPlaceholder_();

    //Null out the report nodes' row iterator objects.
    std::shared_ptr<sparta::StatInstRowIterator> null_iter;
    root->recursSetSIRowIterator_(null_iter);

    //Now, all of the placeholder row iterator objects are
    //destroyed *except* for the one we have right here,
    //taken from the topmost node we could find which had
    //one. Let's tell this placeholder to clone itself into
    //a "realized" StatInstRowIterator.
    std::shared_ptr<sparta::StatInstRowIterator> realized_iterator(
        row_iterator_placeholder->realizePlaceholder());

    //Walk back down through the report tree, and give
    //all report nodes shared ownership of this row
    //iterator.
    root->recursSetSIRowIterator_(realized_iterator);

    //Return true on success.
    return si_row_iterator_ != nullptr;
}

/*!
 * \brief Starting at 'this' report node, recursively get
 * all mappings from Report/SI database node ID to the
 * Report or SI that lives at each node.
 */
void Report::recursGetReportAndSINodeDatabaseIDs_(
    std::unordered_map<simdb::DatabaseID, Report*> & report_nodes_by_id,
    std::unordered_map<simdb::DatabaseID, const StatisticInstance*> & si_nodes_by_id)
{
    sparta_assert(report_node_id_ > 0);
    report_nodes_by_id[report_node_id_] = this;

    sparta_assert(stats_.size() == si_node_ids_.size());
    for (size_t idx = 0; idx < stats_.size(); ++idx) {
        si_nodes_by_id[si_node_ids_[idx]] = stats_[idx].second;
    }

    for (auto & sr : subreps_) {
        sr.recursGetReportAndSINodeDatabaseIDs_(report_nodes_by_id, si_nodes_by_id);
    }
}

/*!
 * \brief Starting at 'this' report node, find the first
 * StatInstRowIterator member variable we encounter while
 * traversing in a depth-first fashion.
 */
std::shared_ptr<sparta::StatInstRowIterator>
    Report::recursFindTopmostSIRowIteratorPlaceholder_()
{
    if (si_row_iterator_ != nullptr) {
        return si_row_iterator_;
    }
    for (auto & sr : subreps_) {
        auto placeholder = sr.recursFindTopmostSIRowIteratorPlaceholder_();
        if (placeholder) {
            return placeholder;
        }
    }
    return nullptr;
}

/*!
 * \brief Set/reset/unset the StatInstRowIterator that is
 * given to us. Passing in a null row iterator is the same
 * thing as resetting this report's row iterator; it will
 * not reject a null iterator object.
 */
void Report::recursSetSIRowIterator_(
    std::shared_ptr<sparta::StatInstRowIterator> & si_row_iterator)
{
    si_row_iterator_ = si_row_iterator;
    for (auto & sr : subreps_) {
        sr.recursSetSIRowIterator_(si_row_iterator);
    }

    //Nothing left to do if this call just reset
    //our iterator to null.
    if (si_row_iterator_ == nullptr) {
        return;
    }

    //Now that we have our row iterator object set, tell
    //all of our SI's to realize their StatInstValueLookup
    //placeholders. It works like this:
    //
    //    1. Create sparta::Report/StatisticInstance objects
    //       from SimDB records. This results in:
    //         - Report nodes have a StatInstRowIterator *placeholder*
    //         - SI nodes have a StatInstValueLookup *placeholder*
    //
    //    2. Get the topmost StatInstRowIterator placeholder
    //
    //    3. Clone that placeholder into a "real" row iterator
    //
    //    4. Share this "real" row iterator with all report nodes
    //
    //    5. Let the individual SI's use this "real" row iterator
    //       to clone their StatInstValueLookup *placeholders*
    //       into "real" SI value lookups.
    //
    //Step 5 is what we're doing here:
    for (auto & si : stats_) {
        si.second->realizeSIValueDirectLookup(*si_row_iterator_);
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
bool Report::createFormattedReportFromDatabase(
    const simdb::ObjectManager & obj_mgr,
    const simdb::DatabaseID report_hier_node_id,
    const std::string & filename,
    const std::string & format,
    const Scheduler * scheduler)
{
    std::string _format(format);
    std::transform(_format.begin(), _format.end(),
                   _format.begin(), ::tolower);

    if (_format == "csv" || _format == "csv_cumulative") {
        std::cout << "Timeseries report generation is currently "
                  << "unsupported for this API" << std::endl;
        return false;
    }

    utils::ValidValue<bool> success;

    obj_mgr.safeTransaction([&]() {
        //We currently only support SimDB->report creation
        //starting with root-level report nodes (i.e. "_global"
        //as opposed to "top.core0.rob" which lives under
        //"_global").
        std::unique_ptr<simdb::ObjectQuery> query(
            new simdb::ObjectQuery(obj_mgr, "ReportNodeHierarchy"));

        query->addConstraints("Id", simdb::constraints::equal,
                              report_hier_node_id);

        simdb::DatabaseID parent_id = -1;
        query->writeResultIterationsTo("ParentNodeID", &parent_id);

        auto result_iter = query->executeQuery();
        sparta_assert(result_iter->getNext());

        if (parent_id > 0) {
            //TODO: Add support for generating any SPARTA report from
            //any node in the original SI hierarchy. There is no reason
            //the entire, potentially large, SI data set needs to be
            //written to the file. We should be able to pick off a
            //subset of the data by node path, for example in Python:
            //
            //   >>> dbconn.sim.top.core0.rob.toJsonReduced('rob_reduced.json')
            //
            throw SpartaException(
                "Report::createFormattedReportFromDatabase() "
                "API cannot be called for a ReportNodeHierarchy "
                "record which has a non-zero/non-null ParentNodeID. "
                "Only root-level report nodes can be used to create "
                "formatted report files from the SimDB.");
        }

        Report report(report_hier_node_id, obj_mgr, scheduler);

        if (_format.find("auto") == 0) {
            //We use ReportDescriptor directly to generate formatted
            //reports from the database for most formats. But auto-summary
            //formats have some "hard-coded" formatting options that other
            //formats don't have. This is the same way the app::Simulation
            //class prints the --auto-summary to stdout:
            report::format::Text summary_fmt(&report);
            summary_fmt.setValueColumn(summary_fmt.getRightmostNameColumn());
            summary_fmt.setReportPrefix("");
            summary_fmt.setQuoteReportNames(false);
            summary_fmt.setWriteContentlessReports(false);
            summary_fmt.setShowSimInfo(false);

            if (_format == "auto_verbose") {
                summary_fmt.setShowDescriptions(true);
            }

            if (filename == utils::COUT_FILENAME) {
                std::cout << summary_fmt << std::endl;
            } else {
                std::ofstream auto_fout(filename);
                if (!auto_fout) {
                    throw SpartaException("Unable to open file for write: '")
                        << filename << "'";
                }
                auto_fout << summary_fmt << std::endl;
            }

            success = true;
            return;
        }

        //Now that we have a Report (and its SI's) recreated
        //completely, use a ReportDescriptor object directly
        //so we get all of the legacy formatting code for
        //free. The format we give the descriptor will turn
        //into the right BaseFormatter under the hood for us.
        app::ReportDescriptor rd("", "", filename, _format);
        report::format::BaseFormatter * formatter =
            rd.addInstantiation(&report, nullptr, nullptr);

        //Tack on any report metadata we find in the database.
        //Give this metadata to the BaseFormatter. The formatter
        //subclasses will write this metadata out to the report
        //files.
        simdb::ObjectQuery root_metadata_query(obj_mgr, "RootReportNodeMetadata");
        root_metadata_query.addConstraints(
            "ReportNodeID", simdb::constraints::equal, report_hier_node_id);

        std::string root_meta_name, root_meta_value;
        root_metadata_query.writeResultIterationsTo(
            "Name", &root_meta_name,
            "Value", &root_meta_value);

        result_iter = root_metadata_query.executeQuery();
        while (result_iter->getNext()) {
            if (root_meta_name == "Elapsed") {
                continue;
            }
            formatter->setMetadataByNameAndStringValue(
                root_meta_name, root_meta_value);
        }

        //The "OmitZero" and "PrettyPrintDisable" report styles are
        //put in the database to signal when the original simulator
        //used the "--omit-zero-value-stats-from-json_reduced" or
        //the "--no-json-pretty-print" command line options.
        if (report.getStyle("OmitZero") == "true") {
            formatter->omitStatsWithValueZero();
        }
        if (report.getStyle("PrettyPrintDisabled") == "true") {
            formatter->disablePrettyPrint();
        }

        report.prepareForSIDatabaseIteration_(obj_mgr);
        if (report.si_row_iterator_ == nullptr) {
            success = false;
            return;
        }

        //Advance the ObjectQuery to the first row / SI blob
        if (!report.si_row_iterator_->getNext()) {
            success = false;
            return;
        }

        //Recurse down through the report nodes and ask all
        //SI's if their StatInstValueLookup objects are ready
        //to go.
        recursValidateSIDirectLookups(report);

        //Manually recreate a SimulationInfo object from
        //the contents of this database.
        std::unique_ptr<SimulationInfo> sim_info;

        /* Start by taking this report ID we were given, and
         * getting its root report ID. For example, we could
         * be recreating a report for the "top.core0" node,
         * but the root is "top".
         *
         * The connection between reports and sim info records
         * looks like this:
         *
         *   RootReportObjMgrIDs           SimInfo
         *  ----------------------        ----------
         *      RootReportNodeID     |-->  ObjMgrID
         *              ObjMgrID  <--|     Name     --\
         *                                 Cmdline     \ (SimInfo
         *                                 ...         /  metadata)
         *                                 ...      --/
         */
        query.reset(new simdb::ObjectQuery(obj_mgr, "RootReportObjMgrIDs"));

        const auto root_id = getRootReportNodeIDFrom(
            report_hier_node_id, obj_mgr);

        query->addConstraints(
            "RootReportNodeID", simdb::constraints::equal, root_id);

        int32_t obj_mgr_id = 0;
        query->writeResultIterationsTo("ObjMgrID", &obj_mgr_id);

        result_iter = query->executeQuery();
        if (result_iter->getNext()) {
            //Let the SimulationInfo's constructor take this database
            //ID and turn it into a live object. While this object is
            //in scope (during this post-simulation report generation),
            //calls to SimulationInfo::getInstance() will actually be
            //talking to this recreated object directly, not the normal
            //singleton that is active during simulations.
            sim_info.reset(new SimulationInfo(
                obj_mgr, obj_mgr_id, report_hier_node_id));
        }

        rd.writeOutput();

        //Give the BaseFormatter subclasses a chance to clear
        //any internal state before continuing. In the case of
        //exporting multiple reports from SimDB to a formatted
        //report file one at a time, any leftover state from
        //one export to the next can cause problems.
        formatter->doPostProcessingBeforeReportValidation();

        success = true;
    });

    return success;
}

/*!
 * \brief Turn the vector of char's we are holding onto into
 * a vector double's. This takes into account compression if
 * it was performed on the current SI row.
 */
bool StatInstRowIterator::getCurrentRowDoubles_()
{
    if (raw_si_was_compressed_) {
        //Re-inflate the compressed SI blob
        z_stream infstream;
        infstream.zalloc = Z_NULL;
        infstream.zfree = Z_NULL;
        infstream.opaque = Z_NULL;

        //Setup the source stream
        infstream.avail_in = (uInt)(raw_si_bytes_.size());
        infstream.next_in = (Bytef*)(&raw_si_bytes_[0]);

        //Setup the destination stream
        raw_si_values_.resize(raw_si_num_pts_);
        infstream.avail_out = (uInt)(raw_si_values_.size() * sizeof(double));
        infstream.next_out = (Bytef*)(&raw_si_values_[0]);

        //Inflate it!
        inflateInit(&infstream);
        inflate(&infstream, Z_FINISH);
        inflateEnd(&infstream);
    }

    else {
        raw_si_values_.resize(raw_si_num_pts_);

        memcpy(&raw_si_values_[0],
               &raw_si_bytes_[0],
               raw_si_num_pts_ * sizeof(double));
    }

    return !raw_si_values_.empty();
}

} // namespace sparta

//! Classes and methods in the placeholders namespace are
//! used in order to "partially create" an object when you
//! have some of, but not all of, its constructor arguments
//! on hand. When you eventually get the rest of its constructor
//! arguments later on, those arguments are forwarded to the
//! placeholder object, who returns a clone of the final,
//! *realized* object.
namespace placeholders {

//! Given a placeholders::StatInstRowIterator object, ask it
//! to clone itself into a sparta::StatInstRowIterator object.
sparta::StatInstRowIterator *
placeholders::StatInstRowIterator::realizePlaceholder()
{
    //The row iterator class expects a root-level report
    //hierarchy node. Let's take the report node ID that
    //we were originally given, get the ID of the node
    //at the top of its report tree, and realize the row
    //iterator using that root-level node ID.
    const simdb::DatabaseID root_node_id =
        sparta::getRootReportNodeIDFrom(report_hier_node_id_, *obj_mgr_);

    sparta_assert(root_node_id > 0, "Invalid report database ID");
    return new sparta::StatInstRowIterator(root_node_id, *obj_mgr_);
}

//! Given a placeholders::StatInstValueLookup object, ask it
//! to clone itself into a sparta::StatInstValueLookup object.
sparta::StatInstValueLookup *
placeholders::StatInstValueLookup::realizePlaceholder(
    const sparta::StatInstRowIterator::RowAccessorPtr & row_accessor)
{
    //The only thing this placeholders object had to begin
    //with was the SI's individual leaf index. In this method,
    //we are being given a indirect reference to the underlying
    //vector of SI values that all SI's in this report tree
    //belong to. The direct reference to this vector is through
    //the RowAccessor's getCurrentRow() method. That vector reference,
    //combined with our unique leaf SI index, is enough to realize
    //a finalized, usable, sparta::StatInstValueLookup.
    return new sparta::StatInstValueLookup(row_accessor, si_index_);
}

} // namespace placeholders
