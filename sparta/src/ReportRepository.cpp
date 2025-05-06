// <ReportRepository> -*- C++ -*-

#include "sparta/report/ReportRepository.hpp"

#include <cstddef>
#include <cstdint>
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <utility>
#include <filesystem>

#include "sparta/report/Report.hpp"
#include "sparta/report/SubContainer.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/trigger/ExpiringExpressionTrigger.hpp"
#include "sparta/report/format/ReportHeader.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsArchive.hpp"
#include "sparta/statistics/dispatch/archives/StatisticsArchives.hpp"
#include "sparta/statistics/dispatch/streams/StatisticsStreams.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/utils/ValidValue.hpp"

#if SIMDB_ENABLED
#include "simdb/sqlite/DatabaseManager.hpp"
#endif

namespace sparta::report::format {
class BaseFormatter;
}  // namespace sparta::report::format

namespace sparta::statistics {
class StreamNode;
}  // namespace sparta::statistics

namespace sparta {

/*!
 * \brief Directory class which contains just ONE ReportDescriptor object.
 * All report instances added to this directory will be added to that descriptor.
 * Any runtime configuration (trigger expressions etc.) found in the directory's
 * report descriptor - the one given to the constructor - will be applied to every
 * report contained in the directory.
 */
class Directory
{
public:
    Directory(const app::ReportDescriptor & desc) :
        desc_(desc)
    {
        sparta_assert(desc_.getUsageCount() == 0);
        sparta_assert(!desc_.def_file.empty());
        sparta_assert(!desc_.dest_file.empty());
        sparta_assert(!desc_.format.empty());
        if (desc_.format.find("cumulative") != std::string::npos) {
            is_cumulative_ = true;
        }
    }

    ~Directory()
    {
        referenced_directories_.erase(referenced_directory_key_);
    }

    void setReportSubContainer(std::shared_ptr<SubContainer> sc)
    {
        sub_container_ = sc;
    }

    void addReport(std::unique_ptr<Report> report)
    {
        auto & extensions = desc_.extensions_;
        auto iter = extensions.find("pending-reports");
        if (iter == extensions.end()) {
            std::vector<Report*> empty_reports;
            extensions["pending-reports"] = empty_reports;
            iter = extensions.find("pending-reports");
        }

        //Report descriptors that have a start trigger will not
        //be given those report instantiations until the start
        //trigger fires. But they still need to be aware of them
        //so they can have their StatisticsArchive handle created
        //at the beginning of the simulation.
        std::vector<Report*> & pending_reports =
            boost::any_cast<std::vector<Report*>&>(iter->second);

        pending_reports.emplace_back(report.get());
        reports_.emplace_back(report.release());
    }

    /*!
     * \brief Write all metadata about our report (and its subreports
     * and statistics) to SimDB.
     */
    void configSimDbReports(simdb::DatabaseManager* db_mgr, RootTreeNode* root)
    {
    #if SIMDB_ENABLED
        db_mgr_ = db_mgr;
        desc_simdb_id_ = desc_.configSimDbReports(db_mgr, root);

        if (desc_simdb_id_ == 0) {
            return;
        }

        const auto& header = reports_[0]->getHeader();
        const auto start_counter_loc = header.getStringified("start_counter");
        const auto stop_counter_loc = header.getStringified("stop_counter");
        const auto update_counter_loc = header.getStringified("update_counter");

        std::ostringstream cmd;
        cmd << "UPDATE Reports SET "
            << "StartCounter = '" << start_counter_loc << "', "
            << "StopCounter = '" << stop_counter_loc << "', "
            << "UpdateCounter = '" << update_counter_loc << "'"
            << " WHERE ReportDescID = " << desc_simdb_id_
            << " AND ParentReportID = 0";

        db_mgr_->EXECUTE(cmd.str());
    #endif
    }

    app::ReportDescriptor & getDescriptor()
    {
        return desc_;
    }

    bool commit(app::Simulation * sim, TreeNode * context)
    {
        sim_ = sim;
        this->consumeDescriptorExtensions_(context);
        return true;
    }

    void setTriggeredNotificationSource(
        const std::shared_ptr<sparta::NotificationSource<std::string>> & on_triggered_notifier)
    {
        on_triggered_notifier_ = on_triggered_notifier;
    }

    std::vector<std::unique_ptr<Report>> saveReports(size_t & num_written)
    {
        if (formatters_.empty()) {
            this->startReports_();
        }

        if (reports_.empty()) {
            return {};
        }

        report_start_trigger_.reset();
        report_stop_trigger_.reset();
        report_update_trigger_.reset();

        num_written += desc_.updateOutput();
        num_written += desc_.writeOutput();
        std::cout << "  [out] Wrote Final Report " << desc_.stringize() << " (updated "
                  << desc_.getNumUpdates() << " times):\n";

        return std::move(reports_);
    }

private:
    void consumeDescriptorExtensions_(TreeNode * context)
    {
        const std::unordered_map<
            std::string,
            boost::any> & cfg_extensions = desc_.extensions_;

        std::string start_expression;
        std::string stop_expression;
        std::string toggle_expression;
        std::string update_expression;
        std::string update_whenever_expression;
        std::string tag;

        //  trigger:
        //    tag:    nickname
        //    start:  expression
        //    stop:   expression
        //    update: expression
        auto iter = cfg_extensions.find("trigger");
        if (iter != cfg_extensions.end()) {
            typedef std::unordered_map<std::string, std::string> TriggerKeyValues;

            const TriggerKeyValues & kv_pairs =
                boost::any_cast<TriggerKeyValues>(iter->second);

            auto get_expression = [&kv_pairs](const std::string & key,
                                              std::string & expression) {
                auto iter = kv_pairs.find(key);
                if (iter != kv_pairs.end()) {
                    expression = iter->second;
                }
            };

            get_expression("start",       start_expression);
            get_expression("stop",        stop_expression);
            get_expression("whenever",    toggle_expression);
            get_expression("tag",         tag);
            get_expression("update-time", update_expression);

            if (!update_expression.empty()) {
                domain_for_pending_update_trigger_ = TriggerDomain::WallClock;
                pending_update_expression_ = update_expression;
            }

            if (!domain_for_pending_update_trigger_.isValid()) {
                get_expression("update-cycles", update_expression);
                if (!update_expression.empty()) {
                    domain_for_pending_update_trigger_ = TriggerDomain::Cycle;
                    pending_update_expression_ = update_expression;
                }
            }

            if (!domain_for_pending_update_trigger_.isValid()) {
                get_expression("update-count", update_expression);
                if (!update_expression.empty()) {
                    domain_for_pending_update_trigger_ = TriggerDomain::Count;
                    pending_update_expression_ = update_expression;
                }
            }

            get_expression("update-whenever", update_whenever_expression);
            if (!update_whenever_expression.empty()) {
                if (domain_for_pending_update_trigger_.isValid()) {
                    throw SpartaException(
                        "You may not specify an 'update-whenever' expression "
                        "together with an 'update-count', 'update-cycles', or "
                        "'update-time' expression in the same YAML report definition.");
                }
                domain_for_pending_update_trigger_ = TriggerDomain::Whenever;
                pending_update_expression_ = update_whenever_expression;
            }

            if (domain_for_pending_update_trigger_.isValid()) {
                switch (domain_for_pending_update_trigger_.getValue()) {
                case TriggerDomain::Whenever:
                    on_update_reschedule_policy_ = OnUpdateReschedulePolicy::StayActive;
                    break;
                default:
                    on_update_reschedule_policy_ = OnUpdateReschedulePolicy::Reschedule;
                    break;
                }
            }

            TreeNode * ctx = context;
            if (desc_.loc_pattern != app::ReportDescriptor::GLOBAL_KEYWORD) {
                ctx = ctx->getChild(desc_.loc_pattern, false);
                if (ctx != nullptr) {
                    context = ctx;
                }
            }

            this->configureStartTrigger_(start_expression, tag, context);
            this->configureStopTrigger_(stop_expression, tag, context);
            this->setDirectoryLocationInTree_(context);

            if (!toggle_expression.empty()) {
                if (!domain_for_pending_update_trigger_.isValid()) {
                    std::cerr <<
                        "     [trigger] Toggle triggers are being used without any "
                        "update trigger (update-count, update-cycles, or update-time)" << std::endl;
                }
                if (desc_.format != "csv") {
                    throw SpartaException(
                        "Toggle triggers may only be used with reports in CSV format");
                }
                this->configureToggleTrigger_(toggle_expression, context);
                enabled_ = false;
                desc_.capUpdatesToOncePerTick(context->getScheduler());
            }

            if (report_start_trigger_ != nullptr) {
                report_start_trigger_->setTriggeredNotificationSource(on_triggered_notifier_);
            }
            if (report_stop_trigger_ != nullptr) {
                report_stop_trigger_->setTriggeredNotificationSource(on_triggered_notifier_);
            }

            if (domain_for_pending_update_trigger_.isValid()) {
                if (report_start_trigger_ == nullptr) {
                    this->configureUpdateTrigger_(pending_update_expression_, context);
                    domain_for_pending_update_trigger_.clearValid();
                } else {
                    directory_context_ = context;
                }
            }

            update_descriptor_when_asked_ =
                (report_start_trigger_ == nullptr || !legacy_start_trigger_);

            if (report_start_trigger_ == nullptr) {
                this->initializeReportInstantiations_();
            }
        } else {
            this->startReports_();
        }

        const bool is_start_or_stop_triggered =
            report_start_trigger_ != nullptr ||
            report_stop_trigger_ != nullptr;

        if (is_start_or_stop_triggered) {
            for (const auto & r : reports_) {
                if (r->hasTriggeredBehavior()) {
                    throw SpartaException(
                        "You may not specify triggers for a report "
                        "and any of its subreports at the same time");
                }
            }
        }
    }

    void configureStartTrigger_(const std::string & start_expression,
                                const std::string & tag,
                                TreeNode * context)
    {
        auto configure = [&](TreeNode * context) {
            if (!start_expression.empty()) {
                SpartaHandler cb = CREATE_SPARTA_HANDLER(Directory, startReports_);

                report_start_trigger_.reset(new trigger::ExpressionTrigger(
                    "ReportSetup", cb, start_expression, context, sub_container_));

                report_start_trigger_->setReferenceEvent(tag, "start");

                trigger::ExpressionTrigger::SingleCounterTrigCallback legacy_start_cb = std::bind(
                    &Directory::legacyDelayedStart_, this, std::placeholders::_1);

                legacy_start_trigger_ = report_start_trigger_->
                    switchToSingleCounterTriggerCallbackIfAble(legacy_start_cb);

                if (legacy_start_trigger_) {
                    referenced_directory_key_ = tag + ".start";
                    referenced_directories_[referenced_directory_key_] = this;
                }
                start_expression_ = start_expression;
            }
        };

        try {
            configure(context);
        } catch (...) {
            configure(sim_->getRoot()->getSearchScope());
        }
    }

    void configureStopTrigger_(const std::string & stop_expression,
                               const std::string & tag,
                               TreeNode * context)
    {
        auto configure = [&](TreeNode * context) {
            if (!stop_expression.empty()) {
                SpartaHandler cb = CREATE_SPARTA_HANDLER(Directory, stopReports_);

                report_stop_trigger_.reset(new trigger::ExpressionTrigger(
                    "ReportTeardown", cb, stop_expression, context, sub_container_));

                report_stop_trigger_->setReferenceEvent(tag, "stop");

                trigger::ExpressionTrigger::SingleCounterTrigCallback legacy_stop_cb = std::bind(
                    &Directory::legacyEarlyStop_, this, std::placeholders::_1);

                legacy_stop_trigger_ = report_stop_trigger_->
                    switchToSingleCounterTriggerCallbackIfAble(legacy_stop_cb);
            }
        };

        try {
            configure(context);
        } catch (...) {
            configure(sim_->getRoot()->getSearchScope());
        }
    }

    void configureToggleTrigger_(const std::string & enabled_expression,
                                 TreeNode * context)
    {
        auto configure = [&](TreeNode * context) {
            if (!enabled_expression.empty()) {
                SpartaHandler on_enable_cb = CREATE_SPARTA_HANDLER(Directory, enableReports_);
                SpartaHandler on_disable_cb = CREATE_SPARTA_HANDLER(Directory, disableReports_);
                auto cfg = sim_->getSimulationConfiguration();

                report_toggle_trigger_.reset(new trigger::ExpressionToggleTrigger(
                    "ReportEnable",
                    enabled_expression,
                    on_enable_cb,
                    on_disable_cb,
                    context,
                    cfg));
            }
        };

        try {
            configure(context);
        } catch (...) {
            configure(sim_->getRoot()->getSearchScope());
        }
    }

    void configureUpdateTrigger_(const std::string & update_expression,
                                 TreeNode * context)
    {
        auto configure = [&](TreeNode * context) {
            if (!update_expression.empty()) {
                SpartaHandler cb = CREATE_SPARTA_HANDLER(Directory, updateReports_);

                switch (domain_for_pending_update_trigger_.getValue()) {
                    case TriggerDomain::WallClock: {
                        report_update_trigger_.reset(new trigger::ExpressionTimeTrigger(
                            "ReportUpdate", cb, update_expression, context));
                        break;
                    }
                    case TriggerDomain::Count: {
                        trigger::ExpressionTrigger::SingleCounterTrigCallback legacy_update_cb = std::bind(
                            &Directory::legacyUpdate_, this, std::placeholders::_1);

                        if (legacy_start_trigger_) {
                            update_descriptor_when_asked_ = true;
                        }

                        if (context != nullptr) {
                            report_update_trigger_.reset(new trigger::ExpressionCounterTrigger(
                                "ReportUpdate", cb, update_expression, true, context));
                        } else {
                            sparta_assert(false, "Attempting to create a report update cycle "
                                        "trigger without a valid context tree node!");
                        }

                        legacy_update_trigger_ = report_update_trigger_->
                            switchToSingleCounterTriggerCallbackIfAble(legacy_update_cb);

                        break;
                    }
                    case TriggerDomain::Cycle: {
                        if(context != nullptr) {
                            report_update_trigger_.reset(new trigger::ExpressionCycleTrigger(
                                "ReportUpdate", cb, update_expression, context));
                        } else {
                            sparta_assert(false, "Attempting to create a report update cycle "
                                        "trigger without a valid context tree node!");
                        }
                        break;
                    }
                    case TriggerDomain::Whenever: {
                        report_update_trigger_.reset(new trigger::ExpressionTrigger(
                            "ReportUpdate", cb, update_expression, context, nullptr));
                        auto & internals = report_update_trigger_->getInternals();
                        if (internals.num_counter_triggers_ > 0 ||
                            internals.num_cycle_triggers_ > 0 ||
                            internals.num_time_triggers_ > 0) {
                            throw SpartaException(
                                "Only 'notif.*' triggers are allowed in 'update-whenever' expressions");
                        }
                        break;
                    }
                    default:
                        sparta_assert("Unreachable");
                }

                //Do not call ExpressionTrigger::setReferenceEvent() for update
                //triggers... reusing periodic triggers in other expressions
                //is not a well-defined behavior

                //Turn off trigger reporting - updates can happen often and clutter std::cout!
                report_update_trigger_->disableMessages();
            } else {
                this->initializeReportInstantiations_();
            }
        };

        try {
            configure(context);
        } catch (...) {
            switch (domain_for_pending_update_trigger_.getValue()) {
                case TriggerDomain::WallClock:
                case TriggerDomain::Cycle: {
                    configure(sim_->getRoot());
                    break;
                }
                case TriggerDomain::Count: {
                    configure(sim_->getRoot()->getSearchScope());
                    break;
                }
                default:
                    break;
            }
        }

        if (report_update_trigger_ != nullptr && report_toggle_trigger_ != nullptr) {
            desc_.setSkippedAnnotator(report_update_trigger_->getSkippedAnnotator());
        }
    }

    void startReports_()
    {
        for (auto & r : reports_) {
            r->start();
        }

        if (domain_for_pending_update_trigger_.isValid()) {
            this->configureUpdateTrigger_(
                pending_update_expression_,
                directory_context_);

            domain_for_pending_update_trigger_.clearValid();
        }

        const bool first = formatters_.empty();
        this->initializeReportInstantiations_();

        if (is_cumulative_) {
            for (auto & r : reports_) {
                r->accumulateStats();
            }
        }

    #if SIMDB_ENABLED
        if (first && db_mgr_ != nullptr && desc_simdb_id_ != 0) {
            // Note that all of our reports (and their subreports) have
            // the same start tick.
            std::ostringstream cmd;
            cmd << "UPDATE Reports SET StartTick = "
                << reports_[0]->getStart()
                << " WHERE ReportDescID = " << desc_simdb_id_
                << " AND ParentReportID = 0";

            db_mgr_->EXECUTE(cmd.str());
        }
    #endif
    }

    void stopReports_()
    {
        if (formatters_.empty()) {
            this->startReports_();
        }

        if (!legacy_stop_trigger_) {
            for (auto & r : reports_) {
                std::cout << "     [trigger] Now stopping report '" << r->getName()
                          << "' at tick " << r->getScheduler()->getCurrentTick()
                          << std::endl;
            }
        }

        for (auto & r : reports_) {
            r->end();
        }

        desc_.updateOutput();
        desc_.ignoreFurtherUpdates();
    }

    void updateReports_()
    {
        if (formatters_.empty()) {
            this->startReports_();
        }

        const bool force_update = false;
        updateReportsWithoutReschedule_(force_update);
        rescheduleUpdateTrigger_();
    }

    void updateReportsWithoutReschedule_(const bool force_update)
    {
        if (formatters_.empty()) {
            this->startReports_();
        }

        if (enabled_ || force_update) {
            desc_.updateOutput();
        } else {
            desc_.skipOutput();
        }
    }

    void rescheduleUpdateTrigger_()
    {
        if (report_update_trigger_ != nullptr) {
            const bool stay_active =
                on_update_reschedule_policy_.isValid() &&
                on_update_reschedule_policy_ == OnUpdateReschedulePolicy::StayActive;

            if (stay_active) {
                report_update_trigger_->stayActive();
                report_update_trigger_->awaken();
            } else {
                report_update_trigger_->reschedule();
            }
        }
    }

    /*!
     * \brief Callback for diagnostic / trigger status printout on report start
     */
    void legacyDelayedStart_(const trigger::CounterTrigger * trigger)
    {
        sparta_assert(legacy_start_trigger_);

        auto ctr = trigger->getCounter();
        auto clk = trigger->getClock();
        auto scheduler = clk->getScheduler();

        for (const auto & r : reports_) {
            std::cout << "     [trigger] Now starting report '" << r->getName()
                      << "' after warmup delay of " << trigger->getTriggerPoint()
                      << " on counter: " << ctr << ". Occurred at tick "
                      << scheduler->getCurrentTick() << " and cycle "
                      << clk->currentCycle() << " on clock " << clk << std::endl;
        }

        start_trigger_counter_ = ctr;
        this->startReports_();
    }

    /*!
    * \brief Callback for diagnostic / trigger status printout on report stop
    */
    void legacyEarlyStop_(const trigger::CounterTrigger * trigger)
    {
        sparta_assert(legacy_stop_trigger_);

        auto ctr = trigger->getCounter();
        auto clk = trigger->getClock();
        auto scheduler = clk->getScheduler();

        for (const auto & r : reports_) {
            std::cout << "     [trigger] Now stopping report '" << r->getName()
                      << "' after specified terminate of " << trigger->getTriggerPoint()
                      << " on counter: " << ctr << ". Occurred at tick "
                      << scheduler->getCurrentTick() << " and cycle "
                      << clk->currentCycle() << " on clock " << clk << std::endl;
        }

        this->stopReports_();
    }

    void legacyUpdate_(const trigger::CounterTrigger * trigger)
    {
        if (formatters_.empty()) {
            this->startReports_();
        }

        uint64_t target_value = trigger->getTriggerPoint();
        const uint64_t counter_value = trigger->getCounter()->get();
        sparta_assert(target_value <= counter_value);

        if (!update_delta_.isValid()) {
            update_delta_ = dynamic_cast<trigger::ExpressionCounterTrigger*>(
                report_update_trigger_.get())->getOriginalTargetValue();
        }

        while(target_value <= counter_value){
            target_value += update_delta_.getValue();
        }

        if (update_descriptor_when_asked_) {
            if (enabled_) {
                desc_.updateOutput();
            } else {
                desc_.skipOutput();
            }
        }

        const_cast<trigger::CounterTrigger*>(trigger)->resetAbsolute(target_value);
        update_descriptor_when_asked_ = true;
    }

    void enableReports_()
    {
        if (update_reports_when_enabled_) {
            update_reports_when_enabled_ = false;
            const bool force_update = true;
            updateReportsWithoutReschedule_(force_update);
        }
        if (reports_have_started_ || report_start_trigger_ == nullptr) {
            startReports_();
        }
        enabled_ = true;
    }

    void disableReports_()
    {
        if (reports_have_started_) {
            update_reports_when_enabled_ = false;
            const bool force_update = true;
            updateReportsWithoutReschedule_(force_update);
        }
        enabled_ = false;
    }

    void initializeReportInstantiations_()
    {
        sparta_assert(formatters_.empty() || report_toggle_trigger_ != nullptr);
        if (!formatters_.empty()) {
            return;
        }

        this->setHeaderInfoForReports_();

        for (auto & r : reports_) {
            formatters_.insert(desc_.addInstantiation(r.get(), sim_));
        }

        if(desc_.getUsageCount() == 0){
            throw SpartaException(
                "Device tree fully realized but the following report "
                "description was not used because there were no tree "
                "locations matching the locations at which they were applied:\n")
                << desc_.stringize();
        }

        if (desc_.loc_pattern.empty()) {
            desc_.loc_pattern = app::ReportDescriptor::GLOBAL_KEYWORD;
        }

        reports_have_started_ = true;
        if (!enabled_) {
            update_reports_when_enabled_ = true;
        }
    }

    void setHeaderInfoForReports_()
    {
        for (auto & r : reports_) {
            auto & header = r->getHeader();

            auto set_header_trigger_content =
                [](report::format::ReportHeader & header,
                   const std::string key,
                   const trigger::ExpiringExpressionTrigger & trigger) -> std::string {
                if (trigger) {
                    const auto counter = trigger->getCounter();
                    if(counter) {
                        header.set(key, counter->getLocation());
                        return counter->getLocation();
                    }
                }

                return "";
            };

            auto ctr_loc = set_header_trigger_content(header, "start_counter",  report_start_trigger_);
            updateSimDbReportMeta_("StartCounter", ctr_loc);

            ctr_loc = set_header_trigger_content(header, "stop_counter",   report_stop_trigger_);
            updateSimDbReportMeta_("StopCounter", ctr_loc);

            ctr_loc = set_header_trigger_content(header, "update_counter", report_update_trigger_);
            updateSimDbReportMeta_("UpdateCounter", ctr_loc);

            const auto header_metadata = getDescriptor().header_metadata_;
            for(auto [key, value] : header_metadata) {
                header.set(key, value);
            }
        }
    }

    void updateSimDbReportMeta_(const std::string & key,
                                const std::string & value)
    {
    #if SIMDB_ENABLED
        if (key.empty() || value.empty()) {
            return;
        }

        if (db_mgr_ != nullptr && desc_simdb_id_ != 0) {
            std::ostringstream cmd;
            cmd << "UPDATE Reports SET " << key << " = '" << value
                << "' WHERE ReportDescID = " << desc_simdb_id_
                << " AND ParentReportID = 0";

            db_mgr_->EXECUTE(cmd.str());
        }
    #endif
    }

    void setDirectoryLocationInTree_(TreeNode * tree_location)
    {
        device_tree_location_ = tree_location;
    }

    utils::ValidValue<uint64_t> getMaxInstRetired_() const
    {
        utils::ValidValue<uint64_t> max_retired;
        if (sim_ == nullptr) {
            return max_retired;
        }

        if (sim_->getRoot() == device_tree_location_ ||
            sim_->getRoot()->getSearchScope() == device_tree_location_) {
            return getMaxInstRetiredForAllCores_();
        }

        using Strictness = app::DefaultValues::RetiredInstPathStrictness;
        const std::pair<std::string, Strictness> retired_inst_counter =
            sim_->getSimulationConfiguration()->path_to_retired_inst_counter;

        auto core_tn = getCoreRootTreeNode_(device_tree_location_);
        auto core_retired = core_tn->getChildAs<const CounterBase*>(
            retired_inst_counter.first, false);
        if (core_retired == nullptr) {
            if (retired_inst_counter.second == Strictness::Strict) {
                if (start_trigger_counter_.isValid()) {
                    max_retired = start_trigger_counter_.getValue()->get();
                } else {
                    throw SpartaException("Unable to locate a tree node at path '") <<
                        core_tn->getLocation() << "." << retired_inst_counter.first << "'";
                }
            }
        } else {
            max_retired = core_retired->get();
        }
        return max_retired;
    }

    utils::ValidValue<uint64_t> getMaxInstRetiredForAllCores_() const
    {
        auto scope = sim_->getRoot()->getSearchScope();

        utils::ValidValue<uint64_t> max_retired;
        uint32_t core_index = 0;

        while (true) {
            std::ostringstream oss;
            oss << "top.core" << core_index;
            auto core_tn = scope->getChild(oss.str(), false);
            if (core_tn != nullptr) {
                using Strictness = app::DefaultValues::RetiredInstPathStrictness;
                const std::pair<std::string, Strictness> retired_inst_counter =
                    sim_->getSimulationConfiguration()->path_to_retired_inst_counter;
                auto core_retired = core_tn->getChildAs<const CounterBase*>(
                    retired_inst_counter.first, false);
                if (core_retired == nullptr && retired_inst_counter.second == Strictness::Strict) {
                    throw SpartaException("Unable to locate a tree node at path '") <<
                        core_tn->getLocation() << "." << retired_inst_counter.first << "'";
                } else if (core_retired != nullptr) {
                    if (!max_retired.isValid()) {
                        max_retired = core_retired->get();
                    } else {
                        max_retired = std::max(max_retired.getValue(),
                                               core_retired->get());
                    }
                }
            } else {
                break;
            }
            ++core_index;
        }

        if (max_retired.isValid()) {
            return max_retired;
        }
        if (start_trigger_counter_.isValid()) {
            max_retired = start_trigger_counter_.getValue()->get();
        }
        return max_retired;
    }

    const TreeNode * getCoreRootTreeNode_(TreeNode * from_here) const
    {
        auto scope = sim_->getRoot()->getSearchScope();
        std::vector<TreeNode*> top_descendents;
        scope->getChildren(top_descendents);

        std::set<TreeNode*> possible_core_roots;
        for (auto tn : top_descendents) {
            std::vector<TreeNode*> children;
            tn->getChildren(children);
            possible_core_roots.insert(children.begin(), children.end());
        }

        auto check_node = [&possible_core_roots](TreeNode * check) {
            if (possible_core_roots.find(check) != possible_core_roots.end()) {
                return true;
            }
            return false;
        };

        TreeNode * core_tn = from_here;
        while (core_tn != nullptr && !check_node(core_tn)) {
            auto parent = core_tn->getParent();
            if (parent == nullptr) {
                break;
            }
            core_tn = parent;
        }
        return core_tn;
    }

    app::ReportDescriptor desc_;
    std::vector<std::unique_ptr<Report>> reports_;
    std::set<report::format::BaseFormatter*> formatters_;

    trigger::ExpiringExpressionTrigger report_start_trigger_;
    trigger::ExpiringExpressionTrigger report_stop_trigger_;
    trigger::ExpiringExpressionTrigger report_update_trigger_;
    std::unique_ptr<trigger::ExpressionToggleTrigger> report_toggle_trigger_;

    static std::map<std::string, Directory*> referenced_directories_;
    std::string referenced_directory_key_;
    std::string start_expression_;
    std::shared_ptr<sparta::NotificationSource<std::string>> on_triggered_notifier_;

    bool legacy_start_trigger_ = true;
    bool legacy_stop_trigger_ = true;
    bool legacy_update_trigger_ = true;
    bool enabled_ = true;
    bool update_reports_when_enabled_ = false;
    bool reports_have_started_ = false;
    bool update_descriptor_when_asked_ = true;
    bool is_cumulative_ = false;

    utils::ValidValue<uint64_t> update_delta_;
    utils::ValidValue<const CounterBase*> start_trigger_counter_;

    enum TriggerDomain {
        WallClock,
        Cycle,
        Count,
        Whenever
    };
    utils::ValidValue<TriggerDomain> domain_for_pending_update_trigger_;
    std::string pending_update_expression_;

    enum class OnUpdateReschedulePolicy {
        StayActive,
        Reschedule
    };
    utils::ValidValue<OnUpdateReschedulePolicy> on_update_reschedule_policy_;
    TreeNode * directory_context_ = nullptr;
    TreeNode * device_tree_location_ = nullptr;
    app::Simulation * sim_ = nullptr;
    std::shared_ptr<SubContainer> sub_container_;

    simdb::DatabaseManager * db_mgr_ = nullptr;
    int desc_simdb_id_ = 0;
};

std::map<std::string, Directory*> Directory::referenced_directories_;

/*!
 * \brief Implementation class for ReportRepository
 */
class ReportRepository::Impl
{
public:
    Impl(app::Simulation * sim) :
      Impl(sim, sim->getRoot()->getSearchScope())
    {
    }

    Impl(TreeNode * context) :
      Impl(nullptr, context)
    {
    }

    ~Impl()
    {
        // If we're not in the middle of an exception, we can save
        // reports, unless report_on_error is defined
        bool save_reports = true;
        if(nullptr != sim_)
        {
            save_reports = sim_->simulationSuccessful();
            if(false == save_reports)
            {
                // Check simulation configuration to see if we still need to report on error
                save_reports = (sim_->getSimulationConfiguration() &&
                                sim_->getSimulationConfiguration()->report_on_error);
            }
        }

        if(save_reports)
        {
            try {
                this->saveReports();
            } catch (...) {
                std::cerr << "WARNING: Error saving reports to file" << std::endl;
            }
        }
    }

    ReportRepository::DirectoryHandle createDirectory(
        const app::ReportDescriptor & desc)
    {
        std::unique_ptr<Directory> direc(new Directory(desc));

        direc->setReportSubContainer(sub_container_);
        ReportRepository::DirectoryHandle handle = direc.get();
        directories_[handle].swap(direc);
        directories_by_creation_date_.push(handle);

        return handle;
    }

    void addReport(ReportRepository::DirectoryHandle handle,
                   std::unique_ptr<Report> report)
    {
        auto iter = directories_.find(handle);
        if (iter == directories_.end()) {
            throw SpartaException("Invalid directory handle");
        }
        iter->second->addReport(std::move(report));
    }

    bool commit(ReportRepository::DirectoryHandle * handle)
    {
        sparta_assert(handle);
        auto iter = directories_.find(*handle);
        if (iter == directories_.end()) {
            throw SpartaException("Invalid directory handle");
        }

        iter->second->setTriggeredNotificationSource(on_triggered_notifier_);
        const bool success = iter->second->commit(sim_, context_);
        if (!success) {
            directories_.erase(*handle);
            *handle = nullptr;
        }

        return success;
    }

    void postBuildTree()
    {
        if (sim_ && on_triggered_notifier_ == nullptr) {
            on_triggered_notifier_.reset(new sparta::NotificationSource<std::string>(
                sim_->getRoot(),
                "sparta_expression_trigger_fired",
                "Notification channel used to post named notifications when triggers hit",
                "sparta_expression_trigger_fired"));
        }
    }

    void postFinalizeFramework()
    {
    #if SIMDB_ENABLED
        bool any_enabled = false;
        for (auto& kvp : directories_) {
            auto& report_desc = kvp.second->getDescriptor();
            if (report_desc.isEnabled() && !report_desc.getAllInstantiations().empty()) {
                any_enabled = true;
                break;
            }
        }

        if (!any_enabled) {
            return;
        }

        const auto& simdb_config = sim_->getSimulationConfiguration()->simdb_config;
        if (simdb_config.simDBReportsEnabled()) {
            simdb::Schema schema;
            using dt = simdb::SqlDataType;

            auto& report_desc_tbl = schema.addTable("ReportDescriptors");
            report_desc_tbl.addColumn("LocPattern", dt::string_t);
            report_desc_tbl.addColumn("DefFile", dt::string_t);
            report_desc_tbl.addColumn("DestFile", dt::string_t);
            report_desc_tbl.addColumn("Format", dt::string_t);

            auto& run_meta_tbl = schema.addTable("ReportDescriptorMeta");
            run_meta_tbl.addColumn("ReportDescID", dt::int32_t);
            run_meta_tbl.addColumn("MetaName", dt::string_t);
            run_meta_tbl.addColumn("MetaValue", dt::string_t);

            auto& report_tbl = schema.addTable("Reports");
            report_tbl.addColumn("ReportDescID", dt::int32_t);
            report_tbl.addColumn("ParentReportID", dt::int32_t);
            report_tbl.addColumn("Name", dt::string_t);
            report_tbl.addColumn("StartTick", dt::int64_t);
            report_tbl.addColumn("EndTick", dt::int64_t);
            report_tbl.addColumn("InfoString", dt::string_t);
            report_tbl.addColumn("StartCounter", dt::string_t);
            report_tbl.addColumn("StopCounter", dt::string_t);
            report_tbl.addColumn("UpdateCounter", dt::string_t);
            report_tbl.setColumnDefaultValue("StartCounter", std::string());
            report_tbl.setColumnDefaultValue("StopCounter", std::string());
            report_tbl.setColumnDefaultValue("UpdateCounter", std::string());

            auto& report_meta_tbl = schema.addTable("ReportMetadata");
            report_meta_tbl.addColumn("ReportDescID", dt::int32_t);
            report_meta_tbl.addColumn("ReportID", dt::int32_t);
            report_meta_tbl.addColumn("MetaName", dt::string_t);
            report_meta_tbl.addColumn("MetaValue", dt::string_t);

            auto& stat_insts_tbl = schema.addTable("StatisticInsts");
            stat_insts_tbl.addColumn("ReportID", dt::int32_t);
            stat_insts_tbl.addColumn("StatisticName", dt::string_t);
            stat_insts_tbl.addColumn("StatisticLoc", dt::string_t);

            const auto& simdb_file = simdb_config.getSimDBFile();
            db_mgr_ = std::make_unique<simdb::DatabaseManager>(simdb_file, true);
            db_mgr_->createDatabaseFromSchema(schema);

            // Use a heartbeat of 1 so that SimDB does not try to optimize the
            // database size by using a pseudo-RLE algo to compress the stats. This
            // makes the python exporter faster and simpler.
            //
            // Note that the database records are still compressed, so the .db
            // size should not be an issue (still a lot smaller than the legacy
            // formatted reports).
            constexpr auto heartbeat = 1;
            db_mgr_->enableCollection(heartbeat);
            auto collection_mgr = db_mgr_->getCollectionMgr();

            // Since a single report can have StatisticInstance's from different
            // clock domains, we will just tell the collector that all stats are
            // on the "root" clock.
            //
            // Note that this doesn't have any real effect on the reports, and
            // differentiating between clocks is more of a concern for Argos.
            // We just happen to be reusing the SimDB collection system for
            // StatisticInstance reports.
            constexpr auto assumed_root_period = 1;
            collection_mgr->addClock("root", assumed_root_period);

            db_mgr_->safeTransaction([&]() {
                for (auto& kvp : directories_) {
                    kvp.second->configSimDbReports(db_mgr_.get(), sim_->getRoot());
                }
                db_mgr_->finalizeCollections();
                return true;
            });
        }
    #endif // SIMDB_ENABLED
    }

    statistics::StatisticsArchives * getStatsArchives()
    {
        if (stats_archives_ == nullptr) {
            stats_archives_.reset(new statistics::StatisticsArchives);

            //Statistics archives get written to the temp dir by default
            auto tempdir = std::filesystem::temp_directory_path();
            const std::string db_dir = tempdir.string();

            for (auto & dir : directories_) {
                app::ReportDescriptor & rd = dir.second->getDescriptor();
                if (!rd.isEnabled()) {
                    continue;
                }

                std::shared_ptr<statistics::ReportStatisticsArchive> archive =
                    rd.logOutputValuesToArchive(db_dir);

                if (archive) {
                    //Archive is valid. Append it.
                    const std::string & archive_name = rd.dest_file;
                    auto archive_root = archive->getRoot();
                    stats_archives_->addHierarchyRoot(archive_name, archive_root);
                }
            }
        }

        return stats_archives_.get();
    }

    statistics::StatisticsStreams * getStatsStreams()
    {
        if (stats_streams_ == nullptr) {
            stats_streams_.reset(new statistics::StatisticsStreams);

            for (auto & dir : directories_) {
                app::ReportDescriptor & rd = dir.second->getDescriptor();
                if (!rd.isEnabled()) {
                    continue;
                }

                std::shared_ptr<statistics::StreamNode> stream_root =
                    rd.createRootStatisticsStream();

                if (stream_root) {
                    //Stream is valid. Append it.
                    const std::string & stream_name = rd.dest_file;
                    stats_streams_->addHierarchyRoot(stream_name, stream_root);
                }
            }
        }

        return stats_streams_.get();
    }

    std::vector<std::unique_ptr<Report>> saveReports()
    {
        std::vector<std::unique_ptr<Report>> saved_reports;
        utils::ValidValue<size_t> num_written;

        ReportRepository::DirectoryHandle handle = nullptr;
        while (!directories_by_creation_date_.empty()) {
            handle = directories_by_creation_date_.front();
            directories_by_creation_date_.pop();

            size_t written = 0;
            auto reports = directories_[handle]->saveReports(written);
            if (!num_written.isValid()) {
                num_written = written;
            } else {
                num_written += written;
            }

            for (auto & report : reports) {
                saved_reports.emplace_back(report.release());
            }
        }

        //Do not print anything if we didn't have any reports
        //to save in the first place (this method is also called
        //implicitly from ~ReportRepository::Impl)
        if (num_written.isValid()) {
            std::cout << "  " << num_written.getValue()
                      << " reports written"
                      << std::endl << std::endl;
        }

    #if SIMDB_ENABLED
        if (db_mgr_) {
            db_mgr_->postSim();
            db_mgr_->closeDatabase();
            db_mgr_.reset();
        }
    #endif // SIMDB_ENABLED

        directories_.clear();

        // %%% ... if(sim_) { ... %%%
        //   This same method can get called again from our own
        //   destructor, *after* the Simulation has already been
        //   deleted. Note the 'sim_' member variable is a raw
        //   pointer. Null it out now so we don't end up using
        //   a dangling pointer.
        sim_ = nullptr;
        return saved_reports;
    }

private:
    Impl(app::Simulation * sim,
         TreeNode * context) :
      sim_(sim),
      context_(context),
      sub_container_(new SubContainer)
    {
    }

    app::Simulation * sim_ = nullptr;
    TreeNode *const context_;
    std::shared_ptr<SubContainer> sub_container_;

    std::unordered_map<
        ReportRepository::DirectoryHandle,
        std::unique_ptr<Directory>> directories_;

    std::queue<ReportRepository::DirectoryHandle> directories_by_creation_date_;
    std::shared_ptr<sparta::NotificationSource<std::string>> on_triggered_notifier_;
    std::unique_ptr<statistics::StatisticsArchives> stats_archives_;
    std::unique_ptr<statistics::StatisticsStreams> stats_streams_;

    #if SIMDB_ENABLED
    std::unique_ptr<simdb::DatabaseManager> db_mgr_;
    #endif
};

ReportRepository::ReportRepository(app::Simulation * sim) :
    impl_(new ReportRepository::Impl(sim))
{
}

ReportRepository::ReportRepository(TreeNode * context) :
    impl_(new ReportRepository::Impl(context))
{
}

ReportRepository::DirectoryHandle ReportRepository::createDirectory(
    const app::ReportDescriptor & desc)
{
    return impl_->createDirectory(desc);
}

void ReportRepository::addReport(
    DirectoryHandle handle,
    std::unique_ptr<Report> report)
{
    impl_->addReport(handle, std::move(report));
}

bool ReportRepository::commit(DirectoryHandle * handle)
{
    return impl_->commit(handle);
}

void ReportRepository::postBuildTree()
{
    impl_->postBuildTree();
}

void ReportRepository::postFinalizeFramework()
{
    impl_->postFinalizeFramework();
}

statistics::StatisticsArchives * ReportRepository::getStatsArchives()
{
    return impl_->getStatsArchives();
}

statistics::StatisticsStreams * ReportRepository::getStatsStreams()
{
    return impl_->getStatsStreams();
}

std::vector<std::unique_ptr<Report>> ReportRepository::saveReports()
{
    return impl_->saveReports();
}

} // namespace sparta
