// <ReportDescriptor.cpp> -*- C++ -*-

/**
 * \file   SpartaException.cpp
 * \brief  Implements exception class for all of SPARTA.
 */

#include "sparta/app/ReportDescriptor.hpp"

#include <yaml-cpp/parser.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stack>

#include "sparta/app/ReportConfigInspection.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/parsers/YAMLTreeEventHandler.hpp"
#include "sparta/utils/File.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/trigger/SkippedAnnotators.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsArchive.hpp"
#include "sparta/statistics/dispatch/streams/StreamNode.hpp"
#include "sparta/statistics/dispatch/ReportStatisticsHierTree.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/report/format/BaseFormatter.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"

#if SIMDB_ENABLED
#include "sparta/app/simdb/ReportStatsCollector.hpp"
#endif

namespace YAML {
class EventHandler;
}  // namespace YAML

#ifdef SPARTA_PYTHON_SUPPORT
#include "python/sparta_support/module_sparta.hpp"
#endif

namespace sparta {
namespace app {

const char* ReportDescriptor::GLOBAL_KEYWORD = "_global";

bool ReportDescriptor::isValidFormatName(const std::string& format)
{
    return sparta::report::format::BaseFormatter::isValidFormatName(format);
}

ReportDescriptor::ReportDescriptor(const std::string& _loc_pattern,
                                   const std::string& _def_file,
                                   const std::string& _dest_file,
                                   const std::string& _format) :
    writes_(0),
    updates_(0),
    orig_dest_file_(_dest_file),
    loc_pattern(_loc_pattern),
    def_file(_def_file),
    dest_file(_dest_file),
    format(toLower(_format))
{
    sparta_assert(isValidFormatName(format),
                      "Report format \"" << format << "\" is not a known report format");

    // Determine formater
    std::string lower_filename = toLower(_dest_file);
    fact_ = sparta::report::format::BaseFormatter::determineFactory(lower_filename, format);
    sparta_assert(fact_ != nullptr,
                      "Report Formatter could not determine factory type for filename/format");

    // Ensure that if writing to stdout/stderr that the formatter is an ostream
    // formatter. Later, it will be assumed that the formatter can be cast to a
    // BaseOstreamFormatter
    if(dest_file == utils::COUT_FILENAME || dest_file == utils::CERR_FILENAME){
        // Create dummy factory to check whether it is an ostream formatter
        // This also ensures that this factory is capable of creating Formatters
        std::unique_ptr<sparta::report::format::BaseFormatter>fmt(fact_->factory(nullptr, ""));
        if(!fmt){
            throw SpartaException("Report Formatter factory \"") << fact_->desc << "\" failed to "
                "create a Formatter";
        }
        sparta::report::format::BaseOstreamFormatter* osfmt = \
            dynamic_cast<sparta::report::format::BaseOstreamFormatter*>(fmt.get());
        if(!osfmt){
            throw SpartaException("Cannot save a report to stdout or stderr using ")
                << "the formatter for '" << fact_->exts.at(0) << "' because "
                << "it is not an ostream formatter";
        }
    }
}

std::shared_ptr<statistics::ReportStatisticsArchive> ReportDescriptor::logOutputValuesToArchive(
    const std::string & dir)
{
    const std::vector<Report*> reports = getAllInstantiations();

    if (reports.empty()) {
        return nullptr;
    }

    //There is currently a strict 1-to-1 mapping between ReportDescriptors
    //and their report archive. It is not well understood what a report
    //archive should look like for a single descriptor that has more than
    //one report instantiation on it.
    //
    //  TODO: Right now, there are no sparta_core_example tests that produce
    //        a ReportDescriptor with multiple report instantiations. This
    //        may be a gap in test coverage that would need to be addressed
    //        first before designing an archive hierarchy for this scenario.
    if (reports.size() == 1) {
        const Report * r = reports[0];
        report_archive_.reset(new statistics::ReportStatisticsArchive(dir, dest_file, *r));
        report_archive_->setArchiveMetadata(extensions_);
        report_archive_->initialize();
        return report_archive_;
    }

    //This descriptor is not able to be archived to the database
    std::cerr << "Report descriptor for output file '" << dest_file << "' "
              << "cannot be logged to the statistics archive. It "
              << "has too many report instantiations in it: \n";
    for (const auto r : reports) {
        std::cerr << "    " << r->getName() << "\n";
    }
    std::cerr << "Archives are currently only allowed for descriptors "
              << "that have exactly 1 report instantiation.\n"
              << std::endl;

    return nullptr;
}

std::shared_ptr<statistics::StreamNode> ReportDescriptor::createRootStatisticsStream()
{
    const std::vector<Report*> reports = getAllInstantiations();

    if (reports.empty()) {
        return nullptr;
    }

    //There is currently a strict 1-to-1 mapping between ReportDescriptors
    //and their statistics stream.
    if (reports.size() == 1) {
        const Report * r = *reports.begin();

        //Stream hierarchies are built up of subreport nodes at the
        //top and in the middle of the tree, and statistic instance
        //nodes at the leaves.
        using SRNode = statistics::ReportStreamNode;
        using SINode = statistics::StatisticInstStreamNode;
        using HierTree = statistics::ReportStatisticsHierTree<SRNode, SINode>;

        HierTree tree_builder(r);
        streaming_stats_root_.reset(new SRNode(r->getName(), r));
        tree_builder.buildFrom(static_cast<SRNode*>(streaming_stats_root_.get()));

        return streaming_stats_root_;
    }

    //This descriptor is not able to be streamed during simulation
    std::cerr << "Report descriptor for output file '" << dest_file << "' "
              << "cannot be used as a streaming statistics source. It has "
              << "too many report instantiations in it: \n";
    for (const auto r : reports) {
        std::cerr << "    " << r->getName() << "\n";
    }
    std::cerr << "Statistics streams are currently only allowed for descriptors "
              << "that have exactly 1 report instantiation.\n"
              << std::endl;

    return nullptr;
}

bool ReportDescriptor::configSimDbReports(app::ReportStatsCollector* collector)
{
#if SIMDB_ENABLED
    if (!isEnabled()) {
        return false;
    }

    const std::vector<Report*> reports = getAllInstantiations();
    if (reports.empty()) {
        return false;
    }

    if (!collector) {
        return false;
    }

    collector_ = collector;
    collector_->addDescriptor(this);
    return true;
#else
    (void)collector;
    return false;
#endif
}

void ReportDescriptor::sweepSimDbStats_()
{
#if SIMDB_ENABLED
    collector_->collect(this);
#endif
}

void ReportDescriptor::skipSimDbStats_()
{
#if SIMDB_ENABLED
    if (collector_ == nullptr || skipped_annotator_ == nullptr) {
        return;
    }

    const std::string annotation = skipped_annotator_->currentAnnotation();
    collector_->writeSkipAnnotation(this, annotation);
#endif
}

report::format::BaseFormatter* ReportDescriptor::addInstantiation(Report* r,
                                                                  Simulation* sim,
                                                                  std::ostream* out)
{
    if (r->hasTriggeredBehavior()) {
        triggered_reports_.insert(r);
    }

    std::string simulation_name;
    if (sim != nullptr) {
        simulation_name = sim->getSimName();
    }

    uint32_t idx = instantiations_.size(); // index of next instantiation
    std::string filename = computeFilename(r, simulation_name, idx);

    if(out){
        *out << "  Placing report on node " << r->getContext()->getLocation()
             << " for: Report \"" << def_file << "\" applied at \"" << loc_pattern
             << "\" -> \"" << filename << "\"";
        if(format.size() != 0){
            *out << " (format=" << format << ")";
        }
        *out << std::endl;
    }

    report::format::BaseFormatter * formatter = nullptr;
    auto f = formatters_.find(filename);
    if (f != formatters_.end()) {
        formatter = f->second.get();
    } else {
        formatter = fact_->factory(r, filename);
        sparta_assert(formatter != nullptr); // Ensured at construction
        formatters_[filename].reset(formatter);
    }

    if(filename == utils::COUT_FILENAME){
        auto osfmt = dynamic_cast<sparta::report::format::BaseOstreamFormatter*>(formatter);
        sparta_assert(osfmt != nullptr); // Guaranteed in ReportDescriptor ctor
        osfmt->setOstream(&std::cout, "stdout");
    }else if(filename == utils::CERR_FILENAME){
        auto osfmt = dynamic_cast<sparta::report::format::BaseOstreamFormatter*>(formatter);
        sparta_assert(osfmt != nullptr); // Guaranteed in ReportDescriptor ctor
        osfmt->setOstream(&std::cerr, "stderr");
    }else{
        // Formatter already has correct output file
    }

    instantiations_.emplace_back(r, formatter);

    // Clear the output filenmae
    std::ofstream os(filename, std::ios::out);
    if(os.fail()){
        throw SpartaException("Failed to open report destination file: \"") << filename
              << "\" When clearing report files in preparation for run. This path may "
              "refer to a directory that does not exist or a file for which the current "
              "user does not have permission";
    }

    if (sim != nullptr) {
        app::SimulationConfiguration * sim_config = sim->getSimulationConfiguration();
        std::vector<std::pair<std::string, std::string>> metadata_kv_pairs(
            1, {"report_format", format});

        if (sim_config != nullptr) {
            const auto & run_metadata = sim_config->getRunMetadata();
            metadata_kv_pairs.insert(metadata_kv_pairs.end(),
                                     run_metadata.begin(),
                                     run_metadata.end());

            const std::string extension = std::filesystem::path(filename).extension();
            if (sim_config->getDisabledPrettyPrintFormats().count(extension)) {
                formatter->disablePrettyPrint();
            }

            utils::lowercase_string my_format = format;
            if (sim_config->getReportFormatsWhoOmitStatsWithValueZero().count(my_format)) {
                formatter->omitStatsWithValueZero();
            }
        }

        for (const auto & meta : metadata_kv_pairs) {
            formatter->setMetadataByNameAndStringValue(meta.first, meta.second);
        }
    }

    // Write header for this formatter if it is updatable. Otherwise it will be
    // written in its entirity at the end of simulation
    if(formatter->supportsUpdate()){
        //TODO: Deprecate "during simulation" formatters
        formatter->writeHeader();
        //TODO cnyce: need to capture the report range right now and "fix" it in the collector
    }

    return formatter;
}

ReportDescriptor::~ReportDescriptor()
{
    try {
        if (!idle_reports_.empty()) {
            std::vector<inst_t> to_flush;
            for (auto & inst : instantiations_) {
                if (idle_reports_.find(inst.first) != idle_reports_.end()) {
                    to_flush.push_back(inst);
                }
            }

            instantiations_.swap(to_flush);
            triggered_reports_.clear();

            this->updateOutput(nullptr);
            this->writeOutput(nullptr);
        }

        if (!legacy_reports_enabled_) {
            std::filesystem::remove(dest_file);
        }
    } catch (...) {
        //destructors should never throw
    }
}

bool ReportDescriptor::updateReportActiveState_(const Report * r)
{
    bool report_active = true;
    auto iter = triggered_reports_.find(r);
    if(iter != triggered_reports_.end()) {
        report_active = (*iter)->isActive();
    }

    if (!report_active) {
        idle_reports_.insert(r);
    } else {
        idle_reports_.erase(r);
    }

    return report_active;
}

uint32_t ReportDescriptor::writeOutput(std::ostream* out)
{
    writes_++;
    uint32_t num_saved = 0;

     // Write all reports in the order of instantiation
    for(auto & inst : getInstantiations()){
        const bool report_active = this->updateReportActiveState_(inst.first);
        if (report_active && false == inst.second->supportsUpdate()) {
            //TODO: Deprecate "during simulation" formatters
            if (legacy_reports_enabled_) {
                inst.second->write();
            }
            if (collector_) {
                sweepSimDbStats_();
            }
            num_saved++;

            // User information
            if(out){
                *out << "    Report instantiated at ";
                if(inst.first->getContext()){
                    *out << inst.first->getContext()->getLocation();
                }else{
                    *out << "\"\"";
                }
                *out << ", written to \"" << inst.second->getTarget() << "\"" << std::endl;
            }
        }
    }

    if (report_archive_ != nullptr) {
        report_archive_->dispatchAll();
    }
    if (streaming_stats_root_ != nullptr) {
        //For now, all streams will be processed on the main thread.
        //This is temporary - these streams are going to Python, and
        //more work needs to be done to run the simulation and the
        //C++/Python streams concurrently.
        streaming_stats_root_->pushStreamUpdateToListeners();
    }

    return num_saved;
}

uint32_t ReportDescriptor::updateOutput(std::ostream* out)
{
    if (update_tracker_.checkIfDuplicateUpdate()) {
        return 0;
    }

    if (report_stopped_) {
        return 0;
    }

    updates_++;
    uint32_t num_updated = 0;
    for(auto & inst : getInstantiations()){
        const bool report_active = this->updateReportActiveState_(inst.first);
        if(report_active && inst.second->supportsUpdate()){
            bool capture_update_values = true;
            if (skipped_annotator_ != nullptr) {
                if (skipped_annotator_->currentSkipCount() > 0) {
                    //TODO: Deprecate "during simulation" formatters
                    if (legacy_reports_enabled_) {
                        inst.second->skip(skipped_annotator_.get());
                    }
                    inst.first->start();
                    capture_update_values = false;
                    if (collector_) {
                        skipSimDbStats_();
                    }
                }
                skipped_annotator_->reset();
            }
            if (capture_update_values) {
                //TODO: Deprecate "during simulation" formatters
                if (legacy_reports_enabled_) {
                    inst.second->update();
                }
                if (collector_) {
                    sweepSimDbStats_();
                }
            }
            num_updated++;

            // User information
            if(out){
                *out << "    Report instantiated at ";
                if(inst.first->getContext()){
                    *out << inst.first->getContext()->getLocation();
                }else{
                    *out << "\"\"";
                }
                *out << ", updated to \"" << inst.second->getTarget() << "\"" << std::endl;
            }

            // Start a new window for calculating deltas
            // Note that this is only safe if report formatters that support updating are handled
            // exclusively through this function and those that don't through writeOutput instead.
            // This will ensure that reports written at simulation-end do not capture deltas (except
            // from when they were first created if not at t=0)
            //
            // Do not "start" the report again unless it supports updates. Formatters not supporting
            // updates will contain absolute data
            inst.first->start();
        }
    }

    if (report_archive_ != nullptr) {
        report_archive_->dispatchAll();
    }
    if (streaming_stats_root_ != nullptr) {
        //For now, all streams will be processed on the main thread.
        //This is temporary - these streams are going to Python, and
        //more work needs to be done to run the simulation and the
        //C++/Python streams concurrently.
        streaming_stats_root_->pushStreamUpdateToListeners();
    }

    return num_updated;
}

void ReportDescriptor::skipOutput()
{
    if (skipped_annotator_ != nullptr) {
        skipped_annotator_->skip();
    }
}

void ReportDescriptor::capUpdatesToOncePerTick(const Scheduler * scheduler)
{
    update_tracker_.enable(scheduler);
}

void ReportDescriptor::setSkippedAnnotator(
    std::shared_ptr<sparta::trigger::SkippedAnnotatorBase> annotator)
{
    skipped_annotator_ = annotator;
}

void ReportDescriptor::clearDestinationFiles(const Simulation& sim)
{
    // Clear all report files
    uint32_t idx = 0;
    for(auto & inst : getInstantiations()){
        if(dest_file != utils::COUT_FILENAME && dest_file != utils::CERR_FILENAME){
            std::string filename = computeFilename(inst.first, sim.getSimName(), idx);
            std::ofstream os(filename, std::ios::out);
            if(os.fail()){
                throw SpartaException("Faield to open report destination file: \"") << filename
                      << "\" When clearing report files in preparation for run. This path may "
                      "refer to a directory that does not exist or a file for which the current "
                      "user does not have permission";
            }
        }
        ++idx;
    }
}

std::string ReportDescriptor::computeFilename(const Report* r,
                                              const std::string& sim_name,
                                              uint32_t idx) const {
    sparta_assert(r != nullptr);
    std::string location;
    if(r->getContext()){
        location = r->getContext()->getLocation();
    }
    return utils::computeOutputFilename(dest_file,
                                        location,
                                        idx,
                                        sim_name);

}

void ReportDescriptor::DescUpdateTracker::enable(const Scheduler * scheduler)
{
    enabled_ = true;
    scheduler_ = scheduler;
    sparta_assert(scheduler_, "Null scheduler given to a ReportDescriptor");
}

bool ReportDescriptor::DescUpdateTracker::checkIfDuplicateUpdate()
{
    if (!enabled_.isValid() || !enabled_) {
        return false;
    }

    const bool first_check = !last_update_at_tick_.isValid();
    const uint64_t current_tick = scheduler_->getCurrentTick();

    if (first_check) {
        last_update_at_tick_ = current_tick;
    } else if (last_update_at_tick_ == current_tick) {
        return true;
    }

    last_update_at_tick_ = current_tick;
    return false;
}

/*!
 * \brief YAML parser class to turn multi-report definition files:
 *
 *   content:
 *     report:
 *       def_file:  stats.yaml
 *       dest_file: foo.html
 *       pattern:   top.core0
 *       format:    html
 *       <extension>:
 *         <key>: <value>
 *         <key>: <value>
 *           :       :
 *     report:
 *       def_file:  stats.yaml
 *         :
 *
 *   Into a vector of ReportDescriptor objects. This class does NOT
 *   create any reports for you! It only creates otherwise empty
 *   descriptors.
 */
class ReportDescriptorFileParserYAML
{
    /*!
     * \brief Event handler for YAML parser
     */
    class ReportDescriptorFileEventHandlerYAML :
        public sparta::YAMLTreeEventHandler
    {
    private:
        std::stack<bool> in_report_stack_;
        bool in_trigger_definition_ = false;
        bool in_header_metadata_ = false;
        ReportDescVec completed_descriptors_;

        std::string loc_pattern_;
        std::string dest_file_;
        std::string def_file_;
        std::string format_;

        bool skip_current_report_ = false;
        bool auto_expand_context_counter_stats_ = false;

        app::TriggerKeyValues trigger_kv_pairs_;
        app::MetaDataKeyValues header_metadata_kv_pairs_;

        /*!
         * \brief Reserved keywords for this parser's dictionary
         */
        static constexpr char KEY_CONTENT[]         = "content";
        static constexpr char KEY_REPORT[]          = "report";
        static constexpr char KEY_DEF_FILE[]        = "def_file";
        static constexpr char KEY_DEST_FILE[]       = "dest_file";
        static constexpr char KEY_PATTERN[]         = "pattern";
        static constexpr char KEY_FORMAT[]          = "format";
        static constexpr char KEY_TRIGGER[]         = "trigger";
        static constexpr char KEY_START[]           = "start";
        static constexpr char KEY_STOP[]            = "stop";
        static constexpr char KEY_WHENEVER[]        = "whenever";
        static constexpr char KEY_UPDATE_TIME[]     = "update-time";
        static constexpr char KEY_UPDATE_CYCLE[]    = "update-cycles";
        static constexpr char KEY_UPDATE_COUNT[]    = "update-count";
        static constexpr char KEY_UPDATE_WHENEVER[] = "update-whenever";
        static constexpr char KEY_TAG[]             = "tag";
        static constexpr char KEY_SKIP[]            = "skip";
        static constexpr char KEY_AUTO_EXPAND_CC[]  = "expand-cc";
        static constexpr char KEY_METADATA[]        = "header_metadata";
        static constexpr char KEY_START_COUNTER[]   = "start_counter";
        static constexpr char KEY_STOP_COUNTER[]    = "stop_counter";
        static constexpr char KEY_UPDATE_COUNTER[]  = "update_counter";

        virtual bool handleEnterMap_(
            const std::string & key,
            NavVector & context) override final
        {
            (void) context;

            if (key == KEY_CONTENT) {
                return false;
            }

            if (key == KEY_REPORT) {
                if (!in_report_stack_.empty()) {
                    throw SpartaException(
                        "Nested report definitions are not supported");
                }
                this->prepareForNextDescriptor_();
                in_report_stack_.push(true);
                return false;
            } else if (key == KEY_TRIGGER) {
                if (in_trigger_definition_) {
                    throw SpartaException(
                        "Nested trigger definitions are not supported");
                }
                in_trigger_definition_ = true;
                return false;
            } else if (key == KEY_METADATA) {
                in_header_metadata_ = true;
                return false;
            }

            if (!key.empty()) {
                throw SpartaException(
                    "Unrecognized key found in definition file: ") << key;
            }
            return false;
        }

        virtual void handleLeafScalar_(
            TreeNode * n,
            const std::string & value,
            const std::string & assoc_key,
            const std::vector<std::string> & captures,
            node_uid_t uid) override final
        {
            (void) n;
            (void) captures;
            (void) uid;

            if (in_trigger_definition_) {
                trigger_kv_pairs_[assoc_key] = value;
            } else {
                if (assoc_key == KEY_PATTERN) {
                    loc_pattern_ = value;
                } else if (assoc_key == KEY_DEF_FILE) {
                    def_file_ = value;
                } else if (assoc_key == KEY_DEST_FILE) {
                    dest_file_ = value;
                } else if (assoc_key == KEY_FORMAT) {
                    format_ = value;
                } else if (assoc_key == KEY_SKIP) {
                    if (value == "true" || value == "1") {
                        skip_current_report_ = true;
                    } else {
                        size_t skip_value = 0;
                        std::istringstream ss(value);
                        ss >> skip_value;
                        skip_current_report_ = (skip_value == 1);
                    }
                } else if (assoc_key == KEY_AUTO_EXPAND_CC) {
                    if (value == "true" || value == "1") {
                        auto_expand_context_counter_stats_ = true;
                    }
                } else {
                    std::ostringstream oss;
                    oss << "Unrecognized key in report definition "
                           "file: '" << assoc_key << "'";
                    throw SpartaException(oss.str());
                }
            }
        }

        virtual bool handleLeafScalarUnknownKey_(
            TreeNode * n,
            const std::string & value,
            const std::string & assoc_key,
            const NavNode& scope) override final
        {
            if (in_header_metadata_) {
                sparta_assert(isMetadataReservedKey_(assoc_key) == false,
                    "Metadata key \""+assoc_key+"\" is reserved");
                header_metadata_kv_pairs_[assoc_key] = value;
                return true;
            }
            return false;
        }

        virtual bool handleExitMap_(
            const std::string & key,
            const NavVector & context) override final
        {
            (void) context;

            if (key == KEY_REPORT) {
                if (def_file_.empty()) {
                    throw SpartaException(
                        "Each report section must contain a 'def_file' entry");
                }
                if (dest_file_.empty()) {
                    throw SpartaException(
                        "Each report section must contain a 'dest_file' entry");
                }

                sparta_assert(!loc_pattern_.empty());
                sparta_assert(!format_.empty());
                sparta_assert(!in_report_stack_.empty());

                in_report_stack_.pop();

                if (skip_current_report_) {
                    skip_current_report_ = false;
                    return false;
                }

                completed_descriptors_.emplace_back(
                    loc_pattern_,
                    def_file_,
                    dest_file_,
                    format_);

                if (!trigger_kv_pairs_.empty()) {
                    std::unordered_map<std::string, boost::any> trigger_extensions;
                    trigger_extensions["trigger"] = trigger_kv_pairs_;

                    auto & descriptor = completed_descriptors_.back();
                    descriptor.extensions_.swap(trigger_extensions);
                }
                if (!header_metadata_kv_pairs_.empty()) {
                    auto & descriptor = completed_descriptors_.back();
                    descriptor.header_metadata_.swap(header_metadata_kv_pairs_);
                }
                if (auto_expand_context_counter_stats_) {
                    auto & descriptor = completed_descriptors_.back();
                    descriptor.extensions_["expand-cc"] = true;
                }
            } else if (key == KEY_TRIGGER) {
                in_trigger_definition_ = false;
            } else if (key == KEY_METADATA) {
                in_header_metadata_ = false;
            }

            return false;
        }

        virtual bool isReservedKey_(
            const std::string & key) const override final
        {
            return (key == KEY_CONTENT          ||
                    key == KEY_REPORT           ||
                    key == KEY_DEF_FILE         ||
                    key == KEY_DEST_FILE        ||
                    key == KEY_PATTERN          ||
                    key == KEY_FORMAT           ||
                    key == KEY_TRIGGER          ||
                    key == KEY_START            ||
                    key == KEY_STOP             ||
                    key == KEY_WHENEVER         ||
                    key == KEY_UPDATE_TIME      ||
                    key == KEY_UPDATE_CYCLE     ||
                    key == KEY_UPDATE_COUNT     ||
                    key == KEY_UPDATE_WHENEVER  ||
                    key == KEY_TAG              ||
                    key == KEY_SKIP             ||
                    key == KEY_AUTO_EXPAND_CC   ||
                    key == KEY_METADATA);
        }

        bool isMetadataReservedKey_(
            const std::string & key) const
        {
            return (key == KEY_START_COUNTER  ||
                    key == KEY_STOP_COUNTER   ||
                    key == KEY_UPDATE_COUNTER);
        }

        void prepareForNextDescriptor_()
        {
            loc_pattern_ = "_global";
            dest_file_.clear();
            def_file_.clear();
            format_ = "text";
            trigger_kv_pairs_.clear();
            auto_expand_context_counter_stats_ = false;
        }

    public:
        ReportDescriptorFileEventHandlerYAML(
            const std::string & def_file,
            NavVector device_trees) :
                sparta::YAMLTreeEventHandler(def_file, device_trees, false)
        {
        }

        ReportDescVec getDescriptors()
        {
            return completed_descriptors_;
        }
    };

public:
    explicit ReportDescriptorFileParserYAML(const std::string & def_file) :
        fin_(),
        parser_(),
        def_file_(def_file)
    {
        sparta_assert(std::filesystem::exists(def_file_),
                    ("File '" + def_file + "' cannot be found"));
        fin_.open(def_file.c_str(), std::ios::in);
        sparta_assert(fin_.is_open());
        parser_.reset(new YP::Parser(fin_));
    }

    explicit ReportDescriptorFileParserYAML(std::istream & content) :
        fin_(),
        parser_(new YP::Parser(content)),
        def_file_("<istream>")
    {
    }

    ReportDescVec parseIntoDescriptors(sparta::TreeNode * context)
    {
        std::shared_ptr<sparta::YAMLTreeEventHandler::NavNode> scope(
            new sparta::YAMLTreeEventHandler::NavNode({
                nullptr, context, {}, 0}));

        ReportDescriptorFileEventHandlerYAML handler(def_file_, {scope});

        while(parser_->HandleNextDocument(*((YP::EventHandler*)&handler))) {}

        return handler.getDescriptors();
    }

private:
    std::ifstream fin_;
    std::unique_ptr<YP::Parser> parser_;
    std::string def_file_;
};

/*!
 * \brief Parse a YAML file containing key-value pairs into a
 * single ReportYamlReplacements data structure.
 */
ReportYamlReplacements createReplacementsFromYaml(
    const std::string & replacements_yaml)
{
    std::ifstream fin(replacements_yaml);
    if (!fin) {
        throw SpartaException("Unable to open replacements file for read: ")
            << replacements_yaml;
    }

    ReportYamlReplacements replacements;
    std::string line;
    while (std::getline(fin, line)) {
        std::vector<std::string> key_value;
        boost::split(key_value, line, boost::is_any_of(":"));
        if (key_value.size() != 2) {
            throw SpartaException("Unable to parse replacements yaml: '")
               << line << "'";
        }
        std::string key = key_value[0];
        std::string value = key_value[1];
        boost::trim(key);
        boost::trim(value);
        replacements.emplace_back(key, value);
    }
    return replacements;
}

/*!
 * \brief Given a multi-report definition YAML file, parse it out
 * into individual descriptors, one for each report defined in the
 * file
 */
ReportDescVec createDescriptorsFromFile(
    const std::string & def_file,
    TreeNode * context)
{
    ReportDescriptorFileParserYAML parser(def_file);
    return parser.parseIntoDescriptors(context);
}

/*!
 * \brief Given a multi-report definition YAML file, apply the
 * provided %PLACEHOLDER% names with their respective values,
 * and return the resulting report descriptors
 */
ReportDescVec createDescriptorsFromFileWithPlaceholderReplacements(
    const std::string & def_file,
    TreeNode * context,
    const ReportYamlReplacements & placeholder_key_value_pairs)
{
    //Read in the entire file into memory first
    std::ifstream fin(def_file);
    if (!fin) {
        throw SpartaException("Unable to open report yaml file for read: ") << def_file;
    }

    std::ostringstream buf;
    buf << fin.rdbuf();
    std::string file_contents = buf.str();

    //Now replace each %PLACEHOLDER% with the corresponding value
    for (const auto & kv_pair : placeholder_key_value_pairs) {
        const std::string & PLACEHOLDER = kv_pair.first;
        const std::string & value = kv_pair.second;
        const std::string to_replace = "%" + PLACEHOLDER + "%";
        boost::replace_all(file_contents, to_replace, value);
    }

    //Create and return the descriptors
    return createDescriptorsFromDefinitionString(file_contents, context);
}

/*!
 * \brief Given a multi-report definition string, parse it out
 * into individual descriptors
 */
ReportDescVec createDescriptorsFromDefinitionString(
    const std::string & def_string,
    TreeNode * context)
{
    std::stringstream ss(def_string);
    ReportDescriptorFileParserYAML parser(ss);
    return parser.parseIntoDescriptors(context);
}

ReportConfiguration::ReportConfiguration(SimulationConfiguration * sim_config,
                                         ReportDescriptorCollection * collection,
                                         RootTreeNode * root) :
    sim_config_(sim_config),
    collection_(collection),
    root_(root)
{
    sparta_assert(sim_config_,
                "Cannot give null SimulationConfiguration to a "
                "report configuration object");

    sparta_assert(collection_,
                "Cannot give null ReportDescriptorCollection to a "
                "report configuration object");

    sparta_assert(root_,
                "Cannot give null RootTreeNode to a report "
                "configuration object");

    republishReportCollection_();
}

void ReportConfiguration::addReport(const ReportDescriptor & rd)
{
    if (!allow_descriptor_changes_) {
        throw SpartaException("Changes to report descriptors are no longer allowed");
    }

    collection_->push_back(rd);
    republishReportCollection_();
}

void ReportConfiguration::addReportsFromYaml(const std::string & yaml_file)
{
    if (!allow_descriptor_changes_) {
        throw SpartaException("Changes to report descriptors are no longer allowed");
    }

    auto new_descriptors = app::createDescriptorsFromFile(yaml_file, root_);
    for (const auto & rd : new_descriptors) {
        collection_->push_back(rd);
    }
    republishReportCollection_();
}

void ReportConfiguration::removeReportByName(const std::string & rd_name)
{
    if (!allow_descriptor_changes_) {
        throw SpartaException("Changes to report descriptors are no longer allowed");
    }

    collection_->getDescriptorByName(rd_name).disable();
    republishReportCollection_();
}

void ReportConfiguration::addMemoryReportsFromYaml(const std::string & yaml_file)
{
    //Memory usage report feature is currently limited to just
    //one report YAML per simulation
    if (!sim_config_->getMemoryUsageDefFile().empty()) {
        std::cout << "Multiple memory usage reports is not supported. YAML file '"
            << sim_config_->getMemoryUsageDefFile() << "' will be used; '"
            << yaml_file << "' will be ignored. \n"
            << std::endl;
        return;
    }

#ifdef SPARTA_PYTHON_SUPPORT
    if (Py_IsInitialized()) {
        std::cout << "Note: Once added to a simulation, memory usage reports \n"
            << "cannot be disabled. They will not show up in the tab-completed \n"
            << "list in the Python shell: 'report_config.descriptors.<tab>'\n"
            << std::endl;
    }
#endif

    sim_config_->setMemoryUsageDefFile(yaml_file);
}

void ReportConfiguration::showAllReportDescriptorInfo()
{
#ifdef SPARTA_PYTHON_SUPPORT
    for (const app::ReportDescriptor & rd : *collection_) {
        if (!rd.isEnabled()) {
            continue;
        }
        std::cout << "- - - - - - - - - - - - - - - - - - - - - "
                     "- - - - - - - - - - - - - - - - - - - - - \n";
        facade::showReportDescriptorInfo(&rd);
        std::cout << "\n";
    }
#else
    std::cout << "Only supported in Python-built SPARTA\n";
#endif
}

void ReportConfiguration::serializeAllDescriptorsToYaml()
{
    if (collection_->empty()) {
        return;
    }

#ifdef SPARTA_PYTHON_SUPPORT
    std::cout << "content:\n";
    for (auto & rd : *collection_) {
        if (!rd.isEnabled()) {
            continue;
        }
        facade::serializeDescriptorToYaml(&rd);
    }
#else
    std::cout << "Only supported in Python-built SPARTA\n";
#endif
}

ReportDescriptorCollection * ReportConfiguration::getDescriptors()
{
    if (!allow_descriptor_changes_) {
        throw SpartaException("Changes to report descriptors are no longer allowed");
    }
    return collection_;
}

const ReportDescriptorCollection * ReportConfiguration::getDescriptors() const
{
    return collection_;
}

void ReportConfiguration::republishReportCollection_()
{
    if (!allow_descriptor_changes_) {
        throw SpartaException("Changes to report descriptors are no longer allowed");
    }

#ifdef SPARTA_PYTHON_SUPPORT
    if (Py_IsInitialized()) {
        bp::object main = bp::import("__main__");
        bp::object global_ns = main.attr("__dict__");

        auto o = WrapperCache<app::ReportDescriptorCollection>().wrap(collection_);

        //Clear out all variables in the dictionary. We will
        //repopulate it from scratch.
        bp::dict d = bp::extract<bp::dict>(o.attr("__dict__"));
        d.clear();

        //Update the __dict__ to only contain the enabled descriptors
        const std::vector<std::string> names = collection_->getAllDescriptorNames();
        for (const auto & rd_name : names) {
            bp::object pyname = bp::str(rd_name);
            sparta::app::ReportDescriptor * rd = &collection_->getDescriptorByName(rd_name);
            sparta_assert(rd->isEnabled());
            d[rd_name] = WrapperCache<sparta::app::ReportDescriptor>().wrap(rd);
        }

        //Update __members__ list as well
        bp::list members;
        for (const auto & rd_name : names) {
            members.append(bp::str(rd_name));
        }
        bp::setattr(o, "__members__", members);
    }
#endif
}

void ReportConfiguration::finishPythonInteraction_()
{
#ifdef SPARTA_PYTHON_SUPPORT
    bp::object main = bp::import("__main__");
    auto global_ns = main.attr("__dict__");

    if (global_ns.contains("report_config")) {
        std::cout << "* Report Configuration (COMPLETE): \n";
        std::cout << "* * * You can no longer access the 'report_config' object or \n"
                  << "* * * any of its descriptors. \n"
                  << std::endl;
        for (const auto & rd : *getDescriptors()) {
            removeElementFromWrapperCache(&rd);
        }
        removeElementFromWrapperCache(getDescriptors());
        removeElementFromWrapperCache(this);
        global_ns["report_config"].del();
    }
#endif
}

void ReportConfiguration::disallowChangesToDescriptors_()
{
    allow_descriptor_changes_ = false;
}

//! \brief Ask this descriptor if it has at least one trigger
bool hasAnyReportTriggers(const ReportDescriptor * rd)
{
    auto iter = rd->extensions_.find("trigger");
    if (iter == rd->extensions_.end()) {
        return false;
    }

    using TriggerDefn = std::unordered_map<std::string, std::string>;
    try {
        const auto & triggers = boost::any_cast<const TriggerDefn&>(iter->second);
        return !triggers.empty();
    } catch (const boost::bad_any_cast &) {
    }
    return false;
}

//! \brief Ask this descriptor if it has any triggers that
//! matches the provided trigger type (i.e. the yaml trigger
//! keyword such as "start", "update-count", etc.)
bool hasTriggerOfType(const ReportDescriptor * rd,
                      const std::string & yaml_type)
{
    auto iter = rd->extensions_.find("trigger");
    if (iter == rd->extensions_.end()) {
        return false;
    }

    using TriggerDefn = std::unordered_map<std::string, std::string>;
    try {
        const auto & triggers = boost::any_cast<const TriggerDefn&>(iter->second);
        return triggers.find(yaml_type) != triggers.end();
    } catch (const boost::bad_any_cast &) {
    }
    return false;
}

//! \brief Ask this descriptor if it has any triggers that
//! match one of the provided trigger types (i.e. the yaml
//! trigger keyword such as "start", "update-count", etc.)
bool hasTriggerOfType(const ReportDescriptor * rd,
                      const std::unordered_set<std::string> & yaml_types)
{
    auto iter = rd->extensions_.find("trigger");
    if (iter == rd->extensions_.end()) {
        return false;
    }

    using TriggerDefn = std::unordered_map<std::string, std::string>;
    try {
        const auto & triggers = boost::any_cast<const TriggerDefn&>(iter->second);
        return std::find_if(triggers.begin(), triggers.end(),
                            [&](const std::pair<std::string, std::string> & trig_kv)
        {
            return yaml_types.find(trig_kv.first) != yaml_types.end();
        }) != triggers.end();
    } catch (const boost::bad_any_cast &) {
    }
    return false;
}

//! \brief Ask this descriptor if it has any start trigger
bool hasStartTrigger(const ReportDescriptor * rd)
{
    return hasTriggerOfType(rd, "start");
}

//! \brief Ask this descriptor if it has any update trigger
bool hasUpdateTrigger(const ReportDescriptor * rd)
{
    return hasTriggerOfType(rd, {"update-count", "update-cycles", "update-time"});
}

//! \brief Ask this descriptor if it has any stop trigger
bool hasStopTrigger(const ReportDescriptor * rd)
{
    return hasTriggerOfType(rd, "stop");
}

//! \brief Ask this descriptor if it has any toggle trigger
bool hasToggleTrigger(const ReportDescriptor * rd)
{
    return hasTriggerOfType(rd, "whenever");
}

//! \brief Ask this descriptor if it has any on-demand trigger
bool hasOnDemandTrigger(const ReportDescriptor * rd)
{
    return hasTriggerOfType(rd, "update-whenever");
}

//! \brief Ask this descriptor if it has a trigger that is
//! tied to a NotificationSource by the given name
bool hasNotifSourceTriggerNamed(const ReportDescriptor * rd,
                                const std::string & notif_source_name,
                                const std::string & yaml_type)
{
    auto iter = rd->extensions_.find("trigger");
    if (iter == rd->extensions_.end()) {
        return false;
    }

    using TriggerDefn = std::unordered_map<std::string, std::string>;
    try {
        const auto & triggers = boost::any_cast<const TriggerDefn&>(iter->second);
        auto trig_iter = triggers.find(yaml_type);
        if (trig_iter == triggers.end()) {
            return false;
        }

        const std::string full_trigger_source_name = "notif." + notif_source_name;
        return trig_iter->second.find(full_trigger_source_name) != std::string::npos;
    } catch (const boost::bad_any_cast &) {
    }
    return false;
}

//! \brief Ask this descriptor if it has any trigger that is
//! tied to a NotificationSource by the given name
bool hasNotifSourceTriggerNamed(const ReportDescriptor * rd,
                                const std::string & notif_source_name,
                                const std::unordered_set<std::string> & yaml_types)
{
    auto iter = rd->extensions_.find("trigger");
    if (iter == rd->extensions_.end()) {
        return false;
    }

    using TriggerDefn = std::unordered_map<std::string, std::string>;
    try {
        const auto & triggers = boost::any_cast<const TriggerDefn&>(iter->second);
        const std::string full_trigger_source_name = "notif." + notif_source_name;

        return std::find_if(triggers.begin(), triggers.end(),
                            [&](const std::pair<std::string, std::string> & trig_kv)
        {
            return yaml_types.find(trig_kv.first) != yaml_types.end() &&
                   trig_kv.second.find(full_trigger_source_name) != std::string::npos;
        }) != triggers.end();
    } catch (const boost::bad_any_cast &) {
    }
    return false;
}

//! \brief Ask this descriptor if it has any *start* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceStartTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name)
{
    return hasNotifSourceTriggerNamed(rd, notif_source_name, "start");
}

//! \brief Ask this descriptor if it has any *update* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceUpdateTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name)
{
    return hasNotifSourceTriggerNamed(
        rd, notif_source_name,
        std::unordered_set<std::string>{"whenever", "update-whenever"});
}

//! \brief Ask this descriptor if it has any *stop* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceStopTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name)
{
    return hasNotifSourceTriggerNamed(rd, notif_source_name, "stop");
}

//! \brief Ask this descriptor for its full trigger expression
//! of the given type.
utils::ValidValue<std::string> getTriggerExpression(
    const ReportDescriptor * rd,
    const std::string & yaml_type)
{
    utils::ValidValue<std::string> expression;
    auto ext_iter = rd->extensions_.find("trigger");
    if (ext_iter == rd->extensions_.end()) {
        return expression;
    }

    try {
        using TriggerDefn = std::unordered_map<std::string, std::string>;
        const auto & triggers = boost::any_cast<const TriggerDefn&>(ext_iter->second);
        auto trig_iter = triggers.find(yaml_type);
        if (trig_iter == triggers.end()) {
            return expression;
        }

        expression = trig_iter->second;
        std::string & expr_str = expression.getValue();
        expr_str.erase(std::remove(expr_str.begin(), expr_str.end(), ' '),
                       expr_str.end());
    } catch (const boost::bad_any_cast &) {
    }

    return expression;
}

//! \brief Local helper to extract "this_notif_name" from a trigger
//! expression of "notif.this_notif_name != 900"
utils::ValidValue<std::string> getNotifSourceNameForTriggerOfType(
    const ReportDescriptor * rd,
    const std::string & yaml_type)
{
    utils::ValidValue<std::string> notif_name;
    static const std::string NOTIF_KEYWORD = "notif.";

    auto ext_iter = rd->extensions_.find("trigger");
    if (ext_iter == rd->extensions_.end()) {
        return notif_name;
    }

    try {
        using TriggerDefn = std::unordered_map<std::string, std::string>;
        const auto & triggers = boost::any_cast<const TriggerDefn&>(ext_iter->second);
        auto trig_iter = triggers.find(yaml_type);
        if (trig_iter == triggers.end()) {
            return notif_name;
        }

        if (trig_iter->second.find(NOTIF_KEYWORD) == 0) {
            std::string cropped_expression =
                trig_iter->second.substr(NOTIF_KEYWORD.size());

            std::string comparison_str;
            std::pair<std::string, std::string> operands;
            if (trigger::ExpressionTrigger::splitComparisonExpression(
                    cropped_expression, operands, comparison_str))
            {
                notif_name = operands.first;
                boost::trim(notif_name.getValue());
            }
        }
    } catch (const boost::bad_any_cast &) {
    }

    return notif_name;
}

//! \brief Get the notif.THIS_NAME of the descriptor's start trigger.
utils::ValidValue<std::string> getNotifSourceForStartTrigger(
    const ReportDescriptor * rd)
{
    return getNotifSourceNameForTriggerOfType(rd, "start");
}

//! \brief Get the notif.THIS_NAME of the descriptor's update trigger.
utils::ValidValue<std::string> getNotifSourceForUpdateTrigger(
    const ReportDescriptor * rd)
{
    utils::ValidValue<std::string> notif_name;

    //There are a handful of update trigger types. This code is
    //not performance critical, but we try to parse out the notif
    //source name roughly in decreasing order of "most commonly
    //used" update trigger types to try to get an earlier return.
    notif_name = getNotifSourceNameForTriggerOfType(rd, "update-count");
    if (notif_name.isValid()) {
        return notif_name;
    }

    notif_name = getNotifSourceNameForTriggerOfType(rd, "update-cycles");
    if (notif_name.isValid()) {
        return notif_name;
    }

    notif_name = getNotifSourceNameForTriggerOfType(rd, "update-time");
    if (notif_name.isValid()) {
        return notif_name;
    }

    notif_name = getNotifSourceNameForTriggerOfType(rd, "update-whenever");
    if (notif_name.isValid()) {
        return notif_name;
    }

    notif_name = getNotifSourceNameForTriggerOfType(rd, "whenever");
    if (notif_name.isValid()) {
        return notif_name;
    }

    return notif_name;
}

//! \brief Get the notif.THIS_NAME of the descriptor's stop trigger.
utils::ValidValue<std::string> getNotifSourceForStopTrigger(
    const ReportDescriptor * rd)
{
    return getNotifSourceNameForTriggerOfType(rd, "stop");
}

} // namespace sparta
} // namespace app
