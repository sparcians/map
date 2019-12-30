// <ReportStatisticsAggregator> -*- C++ -*-

#ifndef __SPARTA_REPORT_STATISTICS_AGGREGATOR_H__
#define __SPARTA_REPORT_STATISTICS_AGGREGATOR_H__

#include "sparta/statistics/dispatch/archives/ArchiveNode.hpp"
#include "sparta/statistics/dispatch/archives/RootArchiveNode.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveSource.hpp"
#include "sparta/statistics/dispatch/StatisticSnapshot.hpp"
#include "sparta/report/Report.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief This class is a source of statistics values used
 * by SPARTA simulators. It takes a sparta::Report object and
 * internally creates an archive tree that describes the
 * report's statistics hierarchy.
 */
class ReportStatisticsAggregator : public ArchiveSource
{
public:
    explicit ReportStatisticsAggregator(const Report & r) :
        report_(r)
    {}

    //One-time initialization of this source
    void initialize() override;

    //Return the flattened StatisticInstance's that
    //belong to this report SI aggregator. This is
    //populated from the root Report node down to
    //all the leaves in a depth-first traversal.
    const std::vector<const StatisticInstance*> & getAggregatedSIs() const {
        return aggregated_sis_;
    }

    //Get a list of all the SI's locations in this timeseries report.
    //This is equivalent to the first row of SI information in the
    //CSV file (dest_file: out.csv) which looks something like this:
    //
    //  "scheduler.ticks,scheduler.picoseconds,scheduler.seconds,..."
    const std::vector<std::string> & getStatInstLocations() const {
        return si_locations_;
    }

    std::shared_ptr<RootArchiveNode> getRoot() {
        return root_;
    }

    //All of this report's StatisticInstance's are copying
    //their stat values to a fixed location in our own double
    //vector. This occurs whenever anybody calls the SI::getValue()
    //method, which is called for every report output/update.
    //Our aggregated values vector is always up to date, and
    //we can just return a reference to it.
    const std::vector<double> & readFromSource() override {
        return aggregated_values_;
    }

private:
    //Create a 1-to-1 mapping between all StatisticInstance's
    //in this report and a reference to a double value in our
    //aggregated values vector.
    void createSnapshotLoggers_(
        const std::vector<const StatisticInstance*> & flattened_stat_insts);

    const Report & report_;
    std::shared_ptr<RootArchiveNode> root_;
    std::vector<double> aggregated_values_;
    std::vector<const StatisticInstance*> aggregated_sis_;
    std::vector<std::string> si_locations_;
};

} // namespace statistics
} // namespace sparta

#endif
