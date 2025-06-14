#pragma once

#include "simdb/apps/UniformSerializer.hpp"
#include "sparta/app/ReportDescriptor.hpp"

namespace sparta {
    class Scheduler;
    class StatisticInstance;
}

namespace sparta::report::format {
    class ReportHeader;
}

namespace sparta::app {

/// This is a SimDB application that serializes all metadata about the
/// report configuration(s), and collects data for report generation later
/// on. A key feature of this app is that it allows for the simulation to
/// run once, and then have multiple python classes handle the data in
/// different ways (HDF5 converter, CSV exporter, display in a web page,
/// etc).

class ReportStatsCollector : public simdb::UniformSerializer
{
public:
    static constexpr auto NAME = "simdb-reports";

    ReportStatsCollector() = default;

    void configPipeline(simdb::PipelineConfig& config) override;

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

    void collect(const ReportDescriptor* desc);

    void writeSkipAnnotation(const ReportDescriptor* desc,
                             const std::string& annotation);

private:
    void extendSchema_(simdb::Schema&) override;

    void postInit_(int argc, char** argv) override;

    void postSim_() override;

    void onPreTeardown_() override;

    void onPostTeardown_() override;

    std::string getByteLayoutYAML_() const override;

    void writeReportInfo_(const ReportDescriptor* desc);

    void writeReportInfo_(const ReportDescriptor* desc,
                          const Report* r,
                          std::unordered_set<std::string>& visited_stats,
                          int parent_report_id);

    class StageObserver : public simdb::PipelineStageObserver
    {
    public:
        StageObserver(ReportStatsCollector* collector) : collector_(collector) {}
        void onEnterStage(const simdb::PipelineEntry& entry, size_t stage_idx) override;
        void onLeaveStage(const simdb::PipelineEntry& entry, size_t stage_idx) override;
    private:
        ReportStatsCollector* collector_;
    };

    void postCommit_(const simdb::PipelineEntry& entry);
    friend class StageObserver;

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
    StageObserver stage_observer_{this};
    const Scheduler* scheduler_ = nullptr;
};

} // namespace sparta::app
