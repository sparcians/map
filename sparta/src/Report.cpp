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
#include <filesystem>

//SQLite-specific headers
#include <zlib.h>

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

        /*!
         * \brief Are we accepting the stats inside the current content block?
         * An example of when we do not accept stats is when we are parsing
         * a content block for an arch that does not match the --arch at the
         * command line.
         */
        bool skip_content_leaves_ = false;

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
                    if(skip_content_leaves_){
                        verbose() << indent_() << "Skipping content due to arch mismatch ("
                                  << assoc_key << " : " << value << ")" << std::endl;
                        return;
                    }

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

        bool handleLeafScalarUnknownKey_(TreeNode* node_context,
                                         const std::string& value,
                                         const std::string& assoc_key,
                                         const NavNode& scope) override
        {
            sparta_assert(node_context);
            bool in_content = in_content_stack_.top();

            auto add_expression = [this, &node_context, &value, &scope] (Expression & expr)
                                  {
                                      // Build the StatisticInstance responsible for evaluating.
                                      StatisticInstance si(std::move(expr));
                                      si.setContext(node_context);
                                      std::string full_name = value;
                                      auto& captures = scope.second;
                                      Report* const r = report_map_.at(scope.uid);
                                      if(this->getSubstituteForStatName(full_name, node_context, captures)){
                                          r->add(si, full_name);
                                      }
                                  };

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
                                    path_in_report.empty() ? node_context : node_context->getChild(path_in_report);

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

                                    // Add the expresssion
                                    add_expression(expr);
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

                                        // Add the expresssion
                                        add_expression(expr);
                                    }
                                }
                            }
                            else if (!skip_content_leaves_){
                                Report* const r = report_map_.at(scope.uid);
                                statistics::expression::Expression expr(assoc_key, node_context, r->getStatistics());

                                // Add the expresssion
                                add_expression(expr);
                            }
                            else {
                                return true;
                            }
                        }
                        else{
                            Report* const r = report_map_.at(scope.uid);
                            statistics::expression::Expression expr(assoc_key, node_context, r->getStatistics());

                            // Add the expresssion
                            add_expression(expr);
                        }
                    }catch(SpartaException& ex){
                        std::stringstream ss;
                        ss << "Unable to parse expression: \"" << assoc_key << "\" within context: "
                            << node_context->getLocation() << " in report file \"" <<  getFilename()
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

            std::filesystem::path filepath = filename;
            if(false == std::filesystem::is_regular_file(filepath.native())){
                std::filesystem::path curfile(getFilename());
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
            if (key.find("-arch-content") != std::string::npos) {
                return true;
            }

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
                }else{
                    if (!tryHandleArchContent_(key)) {
                        return false;
                    }
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
                    if (!tryHandleArchContent_(key)) {
                        return false;
                    }

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
                }else{
                    const auto idx = key.find("-arch-content");
                    if (idx != std::string::npos) {
                        skip_content_leaves_ = false;
                        return false;
                    }
                }
            }

            return true; // Handle normally
        }

        bool tryHandleArchContent_(const std::string& key) {
            const auto idx = key.find("-arch-content");
            if (idx != std::string::npos) {
                app::Simulation *sim = nullptr;
                if (base_report_) {
                    if (auto ctx = base_report_->getContext()) {
                        sim = ctx->getSimulation();
                    }
                }

                if (!sim) {
                    throw SpartaException("Could not get the app::Simulation to parse key: ") << key;
                }

                if (auto sim_config = sim->getSimulationConfiguration()) {
                    skip_content_leaves_ = true;
                    bool dash_arch_given = false;
                    for (const auto &kvp : sim_config->getRunMetadata()) {
                        if (kvp.first == "arch") {
                            dash_arch_given = true;
                            if (kvp.second + "-arch-content" == key) {
                                skip_content_leaves_ = false;
                                break;
                            }
                        }
                    }

                    if (!dash_arch_given) {
                        skip_content_leaves_ = false;
                        verbose() << indent_() << "WARNING: You should consider using --arch at "
                                  << "the command line together with the *-arch-content blocks "
                                  << "in your report definition YAML file. This content block "
                                  << "will be treated as normal (not filtered for --arch)."
                                  << std::endl;
                    }

                    if (skip_content_leaves_) {
                        verbose() << indent_() << "Skipping '" << key << "' block since it does "
                                  << "not match the --arch given at the command line.";
                    }

                    in_content_stack_.push(true);
                    return false;
                } else {
                    throw SpartaException("Could not get the app::SimulationConfiguration to parse key: ") << key;
                }
            }

            return true;
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
                          << " not found in map. Inheriting parent uid " << parent->uid << std::endl;
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

Report::StatAdder Report::add(const StatisticInstance& si, const std::string& name) {
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be pointing to a StatisticInstance "
            << si.getExpressionString();
    }

    // Track a new stat with helpful exception wrapping
    addField_(name, si);

    if(name != ""){ stat_names_.insert(name); }

    addSubStatistics_(&si);

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(StatisticInstance&& si, const std::string& name) {
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be pointing to a StatisticInstance "
            << si.getExpressionString();
    }

    // Track a new stat with helpful exception wrapping
    addField_(name, si);

    if(name != ""){ stat_names_.insert(name); }

    addSubStatistics_(&si);

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(StatisticDef* sd, const std::string& name) {
    sparta_assert(sd);
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be the the statistic def at "
            << sd->getLocation() << " with the expression \""
            << sd->getExpression() << "\"";
    }

    // Track a new stat with helpful exception wrapping
    addField_(name, sd);

    if(name != ""){ stat_names_.insert(name); }

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(CounterBase* ctr, const std::string& name) {
    sparta_assert(ctr);
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be the counter to "
            << ctr->getLocation();
    }

    // Track a new stat with helpful exception wrapping
    addField_(name, ctr);

    if(name != ""){ stat_names_.insert(name); }

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(const TreeNode* n, const std::string& name) {
    sparta_assert(n);
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be the node to "
            << n->getLocation();
    }

    // Track a new stat with helpful exception wrapping
    addField_(name, n);

    if(name != ""){ stat_names_.insert(name); }

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(const std::string& expression, const std::string& name) {
    if(name != "" && stat_names_.find(name) != stat_names_.end()){
        throw SpartaException("There is already a statistic instance in this Report (")
            << getName() << ") named \"" << name << "\" pointing to "
            << getStatistic(name).getLocation()
            << " and the new stat would be the expression \""
            << expression << "\"";
    }
    if(nullptr == context_){
        throw SpartaException("This report currently has no context. To add an item by "
                              "expression \"")
            << expression << "\", specify a context TreeNode using setContext as the "
            "context from which TreeNodes can be searched for";
    }

    if(TreeNodePrivateAttorney::hasChild(context_, expression)){
        // Add as a TreeNode statistic
        add(TreeNodePrivateAttorney::getChild(context_, expression), name);
    }else{
        statistics::expression::Expression expr(expression, context_);
        StatisticInstance si(std::move(expr));
        add(std::move(si), name);
    }

    return Report::StatAdder(*this);
}

Report::StatAdder Report::add(const std::vector<TreeNode*>& nv) {
    for(TreeNode* n : nv){
        add(n);
    }
    return Report::StatAdder(*this);
}

Report::StatAdder Report::addSubStats(StatisticDef * n, const std::string & name_prefix) {
    sparta_assert(auto_expand_context_counter_stats_,
                  "Call to Report::addSubStats(StatisticDef*, name_prefix) is not "
                  "allowed since ContextCounter auto-expansion is disabled. Enable "
                  "this by calling Report::enableContextCounterAutoExpansion()");
    for (const auto & sub_stat : n->getSubStatistics()) {
        TreeNode * sub_stat_node = const_cast<TreeNode*>(sub_stat.getNode());
        const std::string prefix =
            !name_prefix.empty() ? name_prefix : sub_stat_node->getLocation();
        const std::string sub_stat_name = prefix + "." + sub_stat.getName();
        add(sub_stat_node, sub_stat_name);
    }
    return Report::StatAdder(*this);
}

void Report::accumulateStats() const {
    for (const auto & stat : stats_) {
        stat.second->accumulateStatistic();
    }
    for (const auto & sr : getSubreports()) {
        sr.accumulateStats();
    }
}

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

} // namespace sparta
