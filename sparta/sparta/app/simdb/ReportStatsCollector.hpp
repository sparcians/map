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

    static void defineSchema(simdb::Schema&);

    void createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr) override;

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

    void postTeardown() override;

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

    class ReportAtTick
    {
    public:
        ReportAtTick(const ReportDescriptor* descriptor, uint64_t tick)
            : descriptor_(descriptor)
            , tick_(tick)
        {}

        // Default constructor needed to read these out of simdb::ConcurrentQueue's
        ReportAtTick() = default;

        const ReportDescriptor* getDescriptor() const
        {
            return descriptor_;
        }

        uint64_t getTick() const
        {
            return tick_;
        }

    private:
        const ReportDescriptor* descriptor_ = nullptr;
        uint64_t tick_ = 0;
    };

    class ReportStatsAtTick : public ReportAtTick
    {
    public:
        ReportStatsAtTick(const ReportDescriptor* descriptor,
                          uint64_t tick,
                          std::vector<double>&& stats)
            : ReportAtTick(descriptor, tick)
            , stats_(std::move(stats))
        {}

        // Default constructor needed to read these out of simdb::ConcurrentQueue's
        ReportStatsAtTick() = default;

        std::vector<char> compress() const;

    private:
        std::vector<double> stats_;
    };

    class CompressedReportStatsAtTick : public ReportAtTick
    {
    public:
        CompressedReportStatsAtTick(ReportStatsAtTick&& uncompressed)
            : ReportAtTick(uncompressed.getDescriptor(), uncompressed.getTick())
            , bytes_(uncompressed.compress())
        {}

        // Default constructor needed to read these out of simdb::ConcurrentQueue's
        CompressedReportStatsAtTick() = default;

        const std::vector<char>& getBytes() const
        {
            return bytes_;
        }

    private:
        std::vector<char> bytes_;
    };

    simdb::ConcurrentQueue<ReportStatsAtTick>* pipeline_queue_ = nullptr;
};

} // namespace sparta::app
