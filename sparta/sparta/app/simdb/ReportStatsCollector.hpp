#pragma once

#include "simdb/apps/App.hpp"
#include "simdb/utils/ConcurrentQueue.hpp"

#include <map>
#include <unordered_map>
#include <unordered_set>

namespace sparta {
    class Report;
    class Scheduler;
    class StatisticInstance;
}

namespace sparta::report::format {
    class ReportHeader;
}

namespace sparta::app {

class ReportDescriptor;

/// This is a SimDB application that serializes all metadata about the
/// report configuration(s), and collects data for report generation later
/// on. A key feature of this app is that it allows for the simulation to
/// run once, and then have multiple python classes handle the data in
/// different ways (HDF5 converter, CSV exporter, display in a web page,
/// etc).

class ReportStatsCollector : public simdb::App
{
public:
    static constexpr auto NAME = "simdb-reports";

    ReportStatsCollector(simdb::DatabaseManager* db_mgr)
        : db_mgr_(db_mgr)
    {}

    bool defineSchema(simdb::Schema&) override;

    std::unique_ptr<simdb::pipeline::Pipeline> createPipeline(
        simdb::pipeline::AsyncDatabaseAccessor* db_accessor) override;

    void setScheduler(const Scheduler* scheduler);

    void addDescriptor(const ReportDescriptor* desc);

    int getDescriptorID(const ReportDescriptor* desc,
                        bool must_exist = true) const;

    void setHeader(const ReportDescriptor* desc,
                   const report::format::ReportHeader& header);

    void updateReportMetadata(const ReportDescriptor* desc,
                              const std::string& key,
                              const std::string& value);

    void updateReportStartTime(const ReportDescriptor* desc);

    void updateReportEndTime(const ReportDescriptor* desc);

    void postInit(int argc, char** argv) override;

    void collect(const ReportDescriptor* desc);

    void writeSkipAnnotation(const ReportDescriptor* desc,
                             const std::string& annotation);

    void postSim() override;

    void teardown() override;

private:
    void writeReportInfo_(const ReportDescriptor* desc);

    void writeReportInfo_(const ReportDescriptor* desc,
                          const Report* r,
                          std::unordered_set<std::string>& visited_stats,
                          int parent_report_id);

    using Descriptor = std::tuple<std::string, std::string, std::string, std::string>;
    std::vector<std::pair<const ReportDescriptor*, Descriptor>> descriptors_;
    std::unordered_map<const ReportDescriptor*, int> descriptor_ids_;
    std::unordered_map<const ReportDescriptor*, const report::format::ReportHeader*> descriptor_headers_;
    std::unordered_map<const ReportDescriptor*, std::vector<int>> descriptor_report_ids_;
    std::unordered_map<const ReportDescriptor*, std::vector<int>> descriptor_report_style_ids_;
    std::unordered_map<const ReportDescriptor*, std::vector<int>> descriptor_report_meta_ids_;
    std::unordered_map<const ReportDescriptor*, std::vector<const StatisticInstance*>> simdb_stats_;
    std::unordered_map<const ReportDescriptor*, uint64_t> report_start_times_;
    std::unordered_map<const ReportDescriptor*, uint64_t> report_end_times_;
    std::unordered_map<const ReportDescriptor*, std::map<std::string, std::string>> report_metadata_;
    std::unordered_map<const ReportDescriptor*, std::vector<std::pair<uint64_t, std::string>>> report_skip_annotations_;
    const Scheduler* scheduler_ = nullptr;
    simdb::DatabaseManager* db_mgr_ = nullptr;

    using PipelineInT = std::tuple<const ReportDescriptor*, uint64_t, std::vector<double>>;
    simdb::ConcurrentQueue<PipelineInT>* pipeline_queue_ = nullptr;
};

} // namespace sparta::app
