
#include <boost/algorithm/string/split.hpp>
#include <cstddef>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/any.hpp>
#include <boost/date_time/date.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/date_time/gregorian/gregorian_io.hpp>
#include <boost/date_time/gregorian_calendar.hpp>
#include <boost/date_time/posix_time/posix_time_config.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <algorithm>
#include <istream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "sparta/statistics/dispatch/archives/ReportStatisticsArchive.hpp"
#include "sparta/statistics/dispatch/archives/StatisticsArchives.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveController.hpp"
#include "sparta/statistics/dispatch/ReportStatisticsHierTree.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/dispatch/StatisticSnapshot.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveDispatcher.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveNode.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsAggregator.hpp"
#include "sparta/statistics/dispatch/archives/RootArchiveNode.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta {
namespace statistics {

//! Get a human-readable time stamp that we can append to
//! archive directories in the temp dir
std::string getCurrentTimeStamp()
{
    boost::posix_time::ptime time_local =
        boost::posix_time::second_clock::local_time();

    auto date = time_local.date();
    auto time_of_day = time_local.time_of_day();

    std::ostringstream oss;
    oss << "m"  << date.month()           << "_"
        << "d"  << date.day()             << "_"
        << "y"  << date.year()            << "_"
        << "hh" << time_of_day.hours()    << "_"
        << "mm" << time_of_day.minutes()  << "_"
        << "ss" << time_of_day.seconds();

    return oss.str();
}

//! Static initialization of a simulation's time stamp. The
//! same time stamp is appended to all archive directories
//! in the same simulation.
std::string ArchiveDispatcher::simulation_time_stamp_ =
    getCurrentTimeStamp();


//! One-time initialization of a report statistics aggregator.
void ReportStatisticsAggregator::initialize()
{
    using HierTree = ReportStatisticsHierTree<ArchiveNode>;
    HierTree tree_builder(&report_);

    //Build the archive tree from the RootArchiveNode down
    //through all subreports
    root_.reset(new RootArchiveNode(report_.getName()));
    const std::vector<HierTree::LeafNodeSI> leaves =
        tree_builder.buildFrom(root_.get(), &si_locations_);

    //Tell all leaf ArchiveNode's their leaf index number,
    //and get a flattened list of SI's that correspond to
    //the leaves of this archive tree
    aggregated_sis_.clear();
    aggregated_sis_.reserve(leaves.size());
    for (size_t leaf_idx = 0; leaf_idx < leaves.size(); ++leaf_idx) {
        aggregated_sis_.emplace_back(leaves[leaf_idx].second);
        leaves[leaf_idx].first->setLeafIndex(leaf_idx);
    }

    createSnapshotLoggers_(aggregated_sis_);
}

//Finalize the 1-to-1 mapping from StatisticInstance's to their
//location in our vector<double> that will let us take aggregate
//snapshots throughout simulation with very little overhead.
void ReportStatisticsAggregator::createSnapshotLoggers_(
    const std::vector<const StatisticInstance*> & flattened_stat_insts)
{
    aggregated_values_.clear();
    aggregated_values_.reserve(flattened_stat_insts.size());
    for (const auto & stat : flattened_stat_insts) {
        aggregated_values_.emplace_back(0);
        StatisticSnapshot snapshot(aggregated_values_.back());
        stat->addSnapshotLogger(snapshot);
    }
}

//ArchiveNode's at the leaves of an archive tree can return
//an ArchiveDataSeries object on demand. Those objects may
//need to synchronize themselves with the data source to
//ensure the sink is all the way up to date with the source.
void ArchiveDataSeries::synchronize_()
{
    //Read in and cache our archived data if:
    //  1. The archive root required a forced synchronization,
    //     which means it was at least a little out of date...
    //
    //  2. We have no data values cached in memory at all. Maybe
    //     this method has never been called for us yet.
    //
    //Note that if data_values_ is not empty, we have previously
    //been asked for our data. If the call to the root's synchronize()
    //method returns false, and we already have *some* data values in
    //memory, it is guaranteed that we actually have *all* data values
    //in memory already, and we can short-circuit the expensive call
    //that goes back to disk.
    if (root_->synchronize() || data_values_.empty()) {
        readAllDataFromArchive_();
    }
}

//Deep read of archived data values into our memory cache
void ArchiveDataSeries::readAllDataFromArchive_()
{
    const std::string ar_filename =
        root_->getMetadataValue<std::string>("output_filename");

    std::ifstream fin(ar_filename, std::ios::binary);
    if (!fin) {
        throw SpartaException(
            "Unable to open archive file for read: ") << ar_filename;
    }

    fin.seekg(0, fin.end);
    const unsigned long db_num_bytes = fin.tellg();
    fin.seekg(0, fin.beg);

    const size_t db_chunk_num_bytes = root_->getTotalNumLeaves() * sizeof(double);
    sparta_assert(db_num_bytes % db_chunk_num_bytes == 0);
    const size_t my_data_series_num_points = db_num_bytes / db_chunk_num_bytes;

    //Early return if our data vector is already up to date
    if (data_values_.size() == my_data_series_num_points) {
        return;
    }

    data_values_.resize(my_data_series_num_points);
    for (size_t data_idx = 0; data_idx < data_values_.size(); ++data_idx) {
        //Position the file pointer:
        //   - to the start of this data blob
        size_t file_offset = db_chunk_num_bytes * data_idx;
        //   - advance the offset to the start of this leaf's data point
        file_offset += leaf_index_ * sizeof(double);

        //Get the data point from the file and put it into the vector
        char * dest_ptr = reinterpret_cast<char*>(&data_values_[data_idx]);
        fin.seekg(file_offset, fin.beg);
        fin.read(dest_ptr, sizeof(double));
    }
}

//Save (or re-save) all archives
void StatisticsArchives::saveTo(const std::string & dir)
{
    auto names = getRootNames();
    for (const auto & archive_name : names) {
        getRootByName(archive_name)->saveTo(dir);
    }
}

//Synchronize the data source with the data sink, if needed. The
//underlying archive controller will decide if it's necessary.
bool RootArchiveNode::synchronize()
{
    sparta_assert(archive_controller_, "Archive controller was never set");
    return archive_controller_->synchronize();
}

//Save (or re-save) one archive
void RootArchiveNode::saveTo(const std::string & dir)
{
    sparta_assert(archive_controller_, "Archive controller was never set");
    archive_controller_->saveTo(dir);
}

//Lazily walk up to the top of the tree to get the root node
RootArchiveNode * ArchiveNode::getRoot()
{
    if (cached_root_ == nullptr) {
        ArchiveNode * node = this;
        ArchiveNode * parent = parent_;
        while (parent) {
            node = parent;
            parent = parent->parent_;
        }
        sparta_assert(node);
        cached_root_ = dynamic_cast<RootArchiveNode*>(node);
        sparta_assert(cached_root_,
                    "Top node in an archive tree was "
                    "not a RootArchiveNode object");
    }
    return cached_root_;
}

//Lazily create an archive data series object for a leaf archive node
ArchiveDataSeries * ArchiveNode::createDataSeries()
{
    if (!canAccessDataSeries()) {
        throw SpartaException(
            "Invalid call to ArchiveNode::createDataSeries() - "
            "this node is either not a leaf node, or has not had "
            "its leaf index assigned.");
    }
    if (ar_data_series_ == nullptr) {
        //Create the data series object with the shared RootArchiveNode,
        //and the unique leaf index (byte offset) that corresponds to
        //'this' node, a leaf.
        RootArchiveNode * root = getRoot();
        ar_data_series_.reset(new ArchiveDataSeries(leaf_index_, root));
    }
    return ar_data_series_.get();
}

//Live simulation archives synchronize operation (sinks will be
//forced to flush their data)
bool LiveSimulationArchiveController::synchronize()
{
    return live_archive_->flushAll();
}

//Live simulation archives save operation
void LiveSimulationArchiveController::saveTo(const std::string & dir)
{
    live_archive_->saveTo(dir);
}

//Offline archives do not need to synchronize anything
bool OfflineArchiveController::synchronize()
{
    return false;
}

//Offline archives save operation
void OfflineArchiveController::saveTo(const std::string & dir)
{
    //Directory structure looks like this:
    //
    //    db_directory
    //      db_subdirectory
    //        values.bin
    //        archive_tree.bin
    //
    //We just need to copy the two files into their new location, but
    //first throwing away any stale archive files that may already live
    //in the destination directory.

    const std::string binary_filename = source_archive_dir_ + "/values.bin";
    if (!boost::filesystem::exists(binary_filename)) {
        throw SpartaException("Archive file does not exist: ") << binary_filename;
    }

    const std::string archive_tree_filename = source_archive_dir_ + "/archive_tree.bin";
    if (!boost::filesystem::exists(archive_tree_filename)) {
        throw SpartaException("Archive file does not exist: ") << archive_tree_filename;
    }

    //Our source directory was given to us as something like:
    //    "MySavedSimData/out.csv"
    //
    //And we are given a directory that looks something like:
    //    "AnotherFolderHere"
    //
    //Let's split up the original source directory so we can get:
    //    "AnotherFolderHere/out.csv"
    std::vector<std::string> split;
    boost::split(split, source_archive_dir_, boost::is_any_of("/"));

    std::ostringstream oss;
    oss << dir << "/";
    for (size_t idx = 1; idx < split.size(); ++idx) {
        oss << split[idx] << "/";
    }
    const std::string dest_archive_dir = oss.str();

    const std::string new_binary_filename = dest_archive_dir + "/values.bin";
    if (boost::filesystem::exists(new_binary_filename)) {
        boost::filesystem::remove(new_binary_filename);
    }

    const std::string new_archive_tree_filename = dest_archive_dir + "/archive_tree.bin";
    if (boost::filesystem::exists(new_archive_tree_filename)) {
        boost::filesystem::remove(new_archive_tree_filename);
    }

    //Create the directories and copy the files over
    boost::filesystem::create_directories(dest_archive_dir);
    boost::filesystem::copy_file(binary_filename, new_binary_filename);
    boost::filesystem::copy_file(archive_tree_filename, new_archive_tree_filename);
}

} // namespace statistics
} // namespace sparta
