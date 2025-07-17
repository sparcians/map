#include "sparta/app/simdb/ReportStatsCollector.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/report/format/JavascriptObject.hpp"
#include "sparta/report/format/ReportHeader.hpp"
#include "simdb/pipeline/Pipeline.hpp"
#include "simdb/pipeline/elements/Function.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/utils/Compress.hpp"

namespace sparta::app {

REGISTER_SIMDB_APPLICATION(ReportStatsCollector);

bool ReportStatsCollector::defineSchema(simdb::Schema& schema)
{
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

    auto& report_styles_tbl = schema.addTable("ReportStyles");
    report_styles_tbl.addColumn("ReportDescID", dt::int32_t);
    report_styles_tbl.addColumn("ReportID", dt::int32_t);
    report_styles_tbl.addColumn("StyleName", dt::string_t);
    report_styles_tbl.addColumn("StyleValue", dt::string_t);
    report_styles_tbl.createCompoundIndexOn({"ReportDescID", "ReportID", "StyleName"});

    auto& stat_insts_tbl = schema.addTable("StatisticInsts");
    stat_insts_tbl.addColumn("ReportID", dt::int32_t);
    stat_insts_tbl.addColumn("StatisticName", dt::string_t);
    stat_insts_tbl.addColumn("StatisticLoc", dt::string_t);
    stat_insts_tbl.addColumn("StatisticDesc", dt::string_t);
    stat_insts_tbl.addColumn("StatisticVis", dt::int32_t);
    stat_insts_tbl.addColumn("StatisticClass", dt::int32_t);
    stat_insts_tbl.createIndexOn("ReportID");

    auto& stat_defn_meta_tbl = schema.addTable("StatisticDefnMetadata");
    stat_defn_meta_tbl.addColumn("StatisticInstID", dt::int32_t);
    stat_defn_meta_tbl.addColumn("MetaName", dt::string_t);
    stat_defn_meta_tbl.addColumn("MetaValue", dt::string_t);
    stat_defn_meta_tbl.createIndexOn("StatisticInstID");
    stat_defn_meta_tbl.disableAutoIncPrimaryKey();

    auto& siminfo_tbl = schema.addTable("SimulationInfo");
    siminfo_tbl.addColumn("SimName", dt::string_t);
    siminfo_tbl.addColumn("SimVersion", dt::string_t);
    siminfo_tbl.addColumn("SpartaVersion", dt::string_t);
    siminfo_tbl.addColumn("ReproInfo", dt::string_t);
    siminfo_tbl.addColumn("SimEndTick", dt::int64_t);
    siminfo_tbl.setColumnDefaultValue("SimEndTick", -1);
    siminfo_tbl.disableAutoIncPrimaryKey();

    auto& siminfo_header_pairs_tbl = schema.addTable("SimulationInfoHeaderPairs");
    siminfo_header_pairs_tbl.addColumn("HeaderName", dt::string_t);
    siminfo_header_pairs_tbl.addColumn("HeaderValue", dt::string_t);
    siminfo_header_pairs_tbl.disableAutoIncPrimaryKey();

    auto& vis_tbl = schema.addTable("Visibilities");
    vis_tbl.addColumn("Hidden", dt::int32_t);
    vis_tbl.addColumn("Support", dt::int32_t);
    vis_tbl.addColumn("Detail", dt::int32_t);
    vis_tbl.addColumn("Normal", dt::int32_t);
    vis_tbl.addColumn("Summary", dt::int32_t);
    vis_tbl.addColumn("Critical", dt::int32_t);
    vis_tbl.disableAutoIncPrimaryKey();

    auto& js_json_leaf_nodes_tbl = schema.addTable("JsJsonLeafNodes");
    js_json_leaf_nodes_tbl.addColumn("ReportName", dt::string_t);
    js_json_leaf_nodes_tbl.addColumn("IsParentOfLeafNodes", dt::int32_t);
    js_json_leaf_nodes_tbl.setColumnDefaultValue("IsParentOfLeafNodes", -1);
    js_json_leaf_nodes_tbl.disableAutoIncPrimaryKey();

    // In the case of multiple reports, we will end up with more than
    // one CollectionRecords rows with the same Tick value. We use this
    // table in the python exporter to determine which records belong
    // to which report.
    auto& desc_records_tbl = schema.addTable("DescriptorRecords");
    desc_records_tbl.addColumn("ReportDescID", dt::int32_t);
    desc_records_tbl.addColumn("Tick", dt::int64_t);
    desc_records_tbl.addColumn("DataBlob", dt::blob_t);
    desc_records_tbl.createIndexOn("ReportDescID");

    // For timeseries reports that use toggle triggers, we need to
    // annotate the .csv files with a special annotation which tells
    // the user how many ticks/cycles/picoseconds were skipped while
    // the report/trigger was inactive.
    auto& csv_skip_annotations_tbl = schema.addTable("CsvSkipAnnotations");
    csv_skip_annotations_tbl.addColumn("ReportDescID", dt::int32_t);
    csv_skip_annotations_tbl.addColumn("Tick", dt::int64_t);
    csv_skip_annotations_tbl.addColumn("Annotation", dt::string_t);
    csv_skip_annotations_tbl.createIndexOn("ReportDescID");

    return true;
}

std::unique_ptr<simdb::pipeline::Pipeline> ReportStatsCollector::createPipeline(
    simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
{
    auto pipeline = std::make_unique<simdb::pipeline::Pipeline>(db_mgr_, NAME);

    // Stage 1:
    //   - Function:          Compress statistics values
    //   - Input type:        std::tuple<const ReportDescriptor*, uint64_t, std::vector<double>>
    //   - Output type:       std::tuple<const ReportDescriptor*, uint64_t, std::vector<char>>
    using CompressionIn = PipelineInT;
    using CompressionOut = std::tuple<const ReportDescriptor*, uint64_t, std::vector<char>>;
    using ZlibElement = simdb::pipeline::Function<CompressionIn, CompressionOut>;

    auto zlib_task = simdb::pipeline::createTask<ZlibElement>(
        [](CompressionIn&& in, simdb::ConcurrentQueue<CompressionOut>& out)
        {
            CompressionOut compressed;
            std::get<0>(compressed) = std::get<0>(in);
            std::get<1>(compressed) = std::get<1>(in);
            const auto& uncompressed_bytes = std::get<2>(in);
            auto& compressed_bytes = std::get<2>(compressed);
            auto data_ptr = uncompressed_bytes.data();
            auto num_bytes = uncompressed_bytes.size() * sizeof(double);
            simdb::compressData(data_ptr, num_bytes, compressed_bytes);
            out.emplace(std::move(compressed));
        }
    );

    // Stage 2:
    //   - Function:          Write to database
    //   - Input type:        std::tuple<const ReportDescriptor*, uint64_t, std::vector<char>>
    //   - Output type:       none
    using DatabaseIn = CompressionOut;
    using DatabaseOut = void;

    auto sqlite_task = db_accessor->createAsyncWriter<DatabaseIn, DatabaseOut>(
        SQL_TABLE("DescriptorRecords"),
        SQL_COLUMNS("ReportDescID", "Tick", "DataBlob"),
        [this](DatabaseIn&& in, simdb::PreparedINSERT* inserter)
        {
            const auto descriptor = std::get<0>(in);
            const auto descriptor_id = getDescriptorID(descriptor);
            const auto tick = std::get<1>(in);
            const auto& bytes = std::get<2>(in);

            inserter->setColumnValue(0, descriptor_id);
            inserter->setColumnValue(1, tick);
            inserter->setColumnValue(2, bytes);
            inserter->createRecord();
        }
    );

    // Connect tasks -------------------------------------------------------------------
    *zlib_task >> *sqlite_task;

    // Get the pipeline input (head) ---------------------------------------------------
    pipeline_queue_ = zlib_task->getTypedInputQueue<PipelineInT>();

    // Assign threads (task groups) ----------------------------------------------------
    pipeline->createTaskGroup("Compression")
        ->addTask(std::move(zlib_task));

    // We only have to setup the non-DB processing tasks. All calls to createAsyncWriter()
    // implicitly put those created tasks on the shared DB thread.

    return pipeline;
}

void ReportStatsCollector::setScheduler(const Scheduler* scheduler)
{
    scheduler_ = scheduler;
}

void ReportStatsCollector::addDescriptor(const ReportDescriptor* desc)
{
    const auto& pattern = desc->loc_pattern;
    const auto& def_file = desc->def_file;
    const auto& dest_file = desc->dest_file;
    const auto& format = desc->format;
    descriptors_.emplace_back(desc, std::make_tuple(pattern, def_file, dest_file, format));

    writeReportInfo_(desc);
}

int ReportStatsCollector::getDescriptorID(const ReportDescriptor* desc, bool must_exist) const
{
    auto it = descriptor_ids_.find(desc);
    if (it != descriptor_ids_.end()) {
        return it->second;
    }

    if (must_exist) {
        throw SpartaException("ReportDescriptor not found");
    }

    return 0; // Invalid database ID
}

void ReportStatsCollector::setHeader(const ReportDescriptor* desc,
                                     const report::format::ReportHeader& header)
{
    descriptor_headers_[desc] = &header;
}

void ReportStatsCollector::updateReportMetadata(
    const ReportDescriptor* desc,
    const std::string& key,
    const std::string& value)
{
    report_metadata_[desc][key] = value;
}

void ReportStatsCollector::updateReportStartTime(const ReportDescriptor* desc)
{
    auto start_tick = desc->getAllInstantiations()[0]->getStart();
    report_start_times_[desc] = start_tick;
}

void ReportStatsCollector::updateReportEndTime(const ReportDescriptor* desc)
{
    auto end_tick = desc->getAllInstantiations()[0]->getEnd();
    if (end_tick == Scheduler::INDEFINITE) {
        end_tick = scheduler_->getCurrentTick();
    }
    report_end_times_[desc] = end_tick;
}

void ReportStatsCollector::postInit(int argc, char** argv)
{
    db_mgr_->INSERT(
        SQL_TABLE("SimulationInfo"),
        SQL_COLUMNS("SimName", "SimVersion", "SpartaVersion", "ReproInfo"),
        SQL_VALUES(SimulationInfo::getInstance().sim_name,
                   SimulationInfo::getInstance().simulator_version,
                   SimulationInfo::getInstance().getSpartaVersion(),
                   SimulationInfo::getInstance().reproduction_info));

    db_mgr_->INSERT(
        SQL_TABLE("Visibilities"),
        SQL_COLUMNS("Hidden", "Support", "Detail", "Normal", "Summary", "Critical"),
        SQL_VALUES(static_cast<int>(sparta::InstrumentationNode::VIS_HIDDEN),
                   static_cast<int>(sparta::InstrumentationNode::VIS_SUPPORT),
                   static_cast<int>(sparta::InstrumentationNode::VIS_DETAIL),
                   static_cast<int>(sparta::InstrumentationNode::VIS_NORMAL),
                   static_cast<int>(sparta::InstrumentationNode::VIS_SUMMARY),
                   static_cast<int>(sparta::InstrumentationNode::VIS_CRITICAL)));

    report::format::JavascriptObject::writeLeafNodeInfoToDB(db_mgr_);

    for (const auto& [desc, descriptor] : descriptors_) {
        const auto& [pattern, def_file, dest_file, format] = descriptor;

        auto record = db_mgr_->INSERT(
            SQL_TABLE("ReportDescriptors"),
            SQL_COLUMNS("LocPattern", "DefFile", "DestFile", "Format"),
            SQL_VALUES(pattern, def_file, dest_file, format));

        const int report_desc_id = record->getId();
        descriptor_ids_[desc] = report_desc_id;
    }

    for (const auto& [desc, header] : descriptor_headers_) {
        const auto descriptor_id = getDescriptorID(desc);
        const auto start_counter_loc = header->getStringified("start_counter");
        const auto stop_counter_loc = header->getStringified("stop_counter");
        const auto update_counter_loc = header->getStringified("update_counter");

        std::ostringstream cmd;
        cmd << "UPDATE Reports SET "
            << "StartCounter = '" << start_counter_loc << "', "
            << "StopCounter = '" << stop_counter_loc << "', "
            << "UpdateCounter = '" << update_counter_loc << "'"
            << " WHERE ReportDescID = " << descriptor_id
            << " AND ParentReportID = 0";

        db_mgr_->EXECUTE(cmd.str());
    }
}

void ReportStatsCollector::collect(const ReportDescriptor* desc)
{
    std::vector<double> stats;
    stats.reserve(simdb_stats_.at(desc).size());
    for (const auto stat : simdb_stats_[desc]) {
        stats.push_back(stat->getValue());
    }

    PipelineInT in = std::make_tuple(desc, scheduler_->getCurrentTick(), std::move(stats));
    pipeline_queue_->emplace(std::move(in));
}

void ReportStatsCollector::writeSkipAnnotation(
    const ReportDescriptor* desc,
    const std::string& annotation)
{
    auto tick = scheduler_->getCurrentTick();
    report_skip_annotations_[desc].emplace_back(tick, annotation);
}

void ReportStatsCollector::postSim()
{
    for (const auto& [name, value] : SimulationInfo::getInstance().getHeaderPairs()) {
        db_mgr_->INSERT(
            SQL_TABLE("SimulationInfoHeaderPairs"),
            SQL_COLUMNS("HeaderName", "HeaderValue"),
            SQL_VALUES(name, value));
    }

    for (const auto& other : SimulationInfo::getInstance().other) {
        db_mgr_->INSERT(
            SQL_TABLE("SimulationInfoHeaderPairs"),
            SQL_COLUMNS("HeaderName", "HeaderValue"),
            SQL_VALUES("Other", other));
    }

    std::ostringstream oss;
    oss << "UPDATE SimulationInfo SET SimEndTick = "
        << scheduler_->getCurrentTick();

    db_mgr_->EXECUTE(oss.str());
}

void ReportStatsCollector::teardown()
{
    for (const auto& [desc, db_ids] : descriptor_report_ids_) {
        const auto report_desc_id = getDescriptorID(desc);
        for (const auto& report_id : db_ids) {
            std::ostringstream cmd;
            cmd << "UPDATE Reports SET ReportDescID = " << report_desc_id
                << " WHERE Id = " << report_id;
            db_mgr_->EXECUTE(cmd.str());
        }
    }

    for (const auto& [desc, db_ids] : descriptor_report_style_ids_) {
        const auto report_desc_id = getDescriptorID(desc);
        for (const auto& style_id : db_ids) {
            std::ostringstream cmd;
            cmd << "UPDATE ReportStyles SET ReportDescID = " << report_desc_id
                << " WHERE Id = " << style_id;
            db_mgr_->EXECUTE(cmd.str());
        }
    }

    for (const auto& [desc, db_ids] : descriptor_report_meta_ids_) {
        const auto report_desc_id = getDescriptorID(desc);
        for (const auto& meta_id : db_ids) {
            std::ostringstream cmd;
            cmd << "UPDATE ReportMetadata SET ReportDescID = " << report_desc_id
                << " WHERE Id = " << meta_id;
            db_mgr_->EXECUTE(cmd.str());
        }
    }

    for (const auto& [desc, meta] : report_metadata_) {
        const auto report_desc_id = getDescriptorID(desc);
        for (const auto& [meta_name, meta_value] : meta) {
            std::ostringstream cmd;
            cmd << "UPDATE Reports SET " << meta_name << " = '" << meta_value
                << "' WHERE ReportDescID = " << report_desc_id
                << " AND ParentReportID = 0";
            db_mgr_->EXECUTE(cmd.str());
        }
    }

    for (const auto& [desc, start_time] : report_start_times_) {
        const auto report_desc_id = getDescriptorID(desc);
        std::ostringstream cmd;
        cmd << "UPDATE Reports SET StartTick = " << start_time
            << " WHERE ReportDescID = " << report_desc_id
            << " AND ParentReportID = 0";
        db_mgr_->EXECUTE(cmd.str());
    }

    for (const auto& [desc, end_time] : report_end_times_) {
        const auto report_desc_id = getDescriptorID(desc);
        std::ostringstream cmd;
        cmd << "UPDATE Reports SET EndTick = " << end_time
            << " WHERE ReportDescID = " << report_desc_id
            << " AND ParentReportID = 0";
        db_mgr_->EXECUTE(cmd.str());
    }

    for (const auto& [desc, annotation_pairs] : report_skip_annotations_) {
        const auto report_desc_id = getDescriptorID(desc);
        for (const auto& [tick, annotation] : annotation_pairs) {
            db_mgr_->INSERT(
                SQL_TABLE("CsvSkipAnnotations"),
                SQL_COLUMNS("ReportDescID", "Tick", "Annotation"),
                SQL_VALUES(report_desc_id, tick, annotation));
        }
    }
}

void ReportStatsCollector::writeReportInfo_(const ReportDescriptor* desc)
{
    sparta_assert(desc->isEnabled());
    auto reports = desc->getAllInstantiations();
    sparta_assert(!reports.empty());

    std::unordered_set<std::string> visited_stats;
    for (auto r : reports) {
        writeReportInfo_(desc, r, visited_stats, 0);
    }
}

void ReportStatsCollector::writeReportInfo_(
    const ReportDescriptor* desc,
    const Report* r,
    std::unordered_set<std::string>& visited_stats,
    int parent_report_id)
{
    const auto report_name = r->getName();
    const auto report_start_tick = r->getStart();
    const auto report_end_tick = r->getEnd();
    const auto report_info = r->getInfoString();

    const auto report_record = db_mgr_->INSERT(
        SQL_TABLE("Reports"),
        SQL_COLUMNS("ReportDescID", "ParentReportID", "Name", "StartTick", "EndTick", "InfoString"),
        SQL_VALUES(0, parent_report_id, report_name, report_start_tick, report_end_tick, report_info));

    const auto report_id = report_record->getId();
    descriptor_report_ids_[desc].push_back(report_id);

    for (const auto& [name, value] : r->getAllStyles()) {
        auto record = db_mgr_->INSERT(
            SQL_TABLE("ReportStyles"),
            SQL_COLUMNS("ReportDescID", "ReportID", "StyleName", "StyleValue"),
            SQL_VALUES(0, report_id, name, value));

        descriptor_report_style_ids_[desc].push_back(record->getId());
    }

    const auto& stats = r->getStatistics();
    for (const auto& si : stats) {
        const auto& si_name = si.first;
        const auto si_loc = si.second->getLocation();
        const auto si_desc = si.second->getDesc(false);
        const auto si_vis = static_cast<int>(si.second->getVisibility());
        const auto si_class = static_cast<int>(si.second->getClass());

        if (!visited_stats.insert(si_loc).second) {
            continue;
        }

        auto si_record = db_mgr_->INSERT(
            SQL_TABLE("StatisticInsts"),
            SQL_COLUMNS("ReportID", "StatisticName", "StatisticLoc", "StatisticDesc", "StatisticVis", "StatisticClass"),
            SQL_VALUES(report_id, si_name, si_loc, si_desc, si_vis, si_class));

        if (auto stat_def = si.second->getStatisticDef()) {
            const auto si_id = si_record->getId();
            for (const auto& pair : stat_def->getMetadata()) {
                const auto& meta_name = pair.first;
                const auto& meta_value = pair.second;

                db_mgr_->INSERT(
                    SQL_TABLE("StatisticDefnMetadata"),
                    SQL_COLUMNS("StatisticInstID", "MetaName", "MetaValue"),
                    SQL_VALUES(si_id, meta_name, meta_value));
            }
        }

        simdb_stats_[desc].push_back(si.second.get());
    }

    for (const auto& [report, formatter] : desc->getInstantiations()) {
        if (report == r) {
            auto meta_kv_pairs = formatter->getMetadataKVPairs();
            meta_kv_pairs["PrettyPrint"] = formatter->prettyPrintEnabled() ? "true" : "false";
            meta_kv_pairs["OmitZeros"] = formatter->statsWithValueZeroAreOmitted() ? "true" : "false";

            for (const auto& [meta_name, meta_value] : meta_kv_pairs) {
                auto record = db_mgr_->INSERT(
                    SQL_TABLE("ReportMetadata"),
                    SQL_COLUMNS("ReportDescID", "ReportID", "MetaName", "MetaValue"),
                    SQL_VALUES(0, report_id, meta_name, meta_value));

                descriptor_report_meta_ids_[desc].push_back(record->getId());
            }

            break;
        }        
    }

    for (const auto& sr : r->getSubreports()) {
        writeReportInfo_(desc, &sr, visited_stats, report_id);
    }
}

} // namespace sparta::app
