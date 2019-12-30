// <ReportStatisticsArchive> -*- C++ -*-

#ifndef __SPARTA_REPORT_STATISTICS_ARCHIVE_H__
#define __SPARTA_REPORT_STATISTICS_ARCHIVE_H__

#include "sparta/statistics/dispatch/archives/ReportStatisticsAggregator.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveDispatcher.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveController.hpp"
#include "sparta/statistics/dispatch/archives/BinaryIArchive.hpp"
#include "sparta/statistics/dispatch/archives/BinaryOArchive.hpp"

namespace sparta {
namespace statistics {

class RootArchiveNode;

/*!
 * \brief This class coordinates live SPARTA simulations (source)
 * with binary output archives (sink).
 */
class ReportStatisticsArchive
{
public:
    ReportStatisticsArchive(const std::string & db_directory,
                            const std::string & db_subdirectory,
                            const Report & report)
    {
        dispatcher_.reset(new ReportStatisticsDispatcher(
            db_directory, db_subdirectory, report));
    }

    //Metadata will be forwarded along to the underlying RootArchiveNode.
    //You can get this root node object by calling getRoot()
    void setArchiveMetadata(const app::NamedExtensions & metadata) {
        dispatcher_->setArchiveMetadata(metadata);
    }

    //One-time initialization of the output binary archive
    void initialize() {
        dispatcher_->configureBinaryArchive(this);
    }

    //Access the underlying root node for our archive tree
    std::shared_ptr<RootArchiveNode> getRoot() const {
        return dispatcher_->getRoot();
    }

    //Send out all of the report's StatisticInstance current
    //values to the binary sink
    void dispatchAll() {
        dispatcher_->dispatch();
        dirty_ = true;
    }

    //Synchronize the data source with the binary sink. Returns
    //true if the flush was made, and false if the archive was
    //already in sync. Returning false is NOT a sign of an error.
    bool flushAll() {
        if (!dirty_) {
            return false;
        }

        dispatcher_->flush();
        dirty_ = false;
        return true;
    }

    //Make a deep copy of the archive, sending it to the
    //given directory. This does not invalidate the current
    //ongoing/live archive. This call can safely be made during
    //simulation.
    void saveTo(const std::string & db_directory) {
        dispatcher_->flush();
        const auto & sinks = dispatcher_->getSinks();
        for (const auto & sink : sinks) {
            copyArchiveToDirectory_(*sink, db_directory);
        }
    }

private:
    /*!
     * \brief Specialized dispatcher which sends data to a binary
     * output file.
     */
    class ReportStatisticsDispatcher : public ArchiveDispatcher
    {
    public:
        ReportStatisticsDispatcher(const std::string & db_directory,
                                   const std::string & db_subdirectory,
                                   const Report & report) :
            db_directory_(db_directory),
            db_subdirectory_(db_subdirectory)
        {
            std::unique_ptr<ReportStatisticsAggregator> source(
                new ReportStatisticsAggregator(report));

            source->initialize();
            root_ = source->getRoot();
            setStatisticsSource(std::move(source));
        }

        void setArchiveMetadata(const app::NamedExtensions & metadata) {
            root_->setMetadata(metadata);

            //All archives should have a "triggers" property, even
            //if there were no triggers used to generate the report.
            //This is to support Python, so we can give a user friendly
            //message like this:
            //
            //    >>> foo.bar.triggers.showInfo()
            //    "No triggers have been set"
            if (!root_->tryGetMetadataValue<app::TriggerKeyValues>("trigger")) {
                app::TriggerKeyValues no_triggers;
                root_->setMetadataValue("trigger", no_triggers);
            }
        }

        void configureBinaryArchive(ReportStatisticsArchive * source)
        {
            //Give the root archive node a controller it can use to
            //save the archive to another directory, synchronize the
            //data source / data sink, etc.
            std::shared_ptr<ArchiveController> controller(
                new LiveSimulationArchiveController(source));

            root_->setArchiveController(controller);
            root_->initialize();

            //Append a time stamp to the database directory we were given.
            //This is a static string which will be the same for all archive
            //sinks in the tempdir for this simulation.
            const std::string & time_stamp = ArchiveDispatcher::getSimulationTimeStamp_();

            std::unique_ptr<BinaryOArchive> sink(new BinaryOArchive);
            sink->setPath(db_directory_ + "/" + time_stamp);
            sink->setSubpath(db_subdirectory_);
            sink->setRoot(root_);
            sink->initialize();
            addStatisticsSink(std::move(sink));
        }

        std::shared_ptr<RootArchiveNode> getRoot() {
            return root_;
        }

    private:
        const std::string db_directory_;
        const std::string db_subdirectory_;
        std::shared_ptr<RootArchiveNode> root_;
    };

    //Copy all archive files that belong to an ongoing data sink,
    //and put the copies in the given directory. This does not
    //invalidate the ongoing sink, or change any internal state
    //in any way.
    void copyArchiveToDirectory_(const ArchiveSink & original_sink,
                                 const std::string & destination_dir) const
    {
        BinaryIArchive binary_source;
        binary_source.setPath(original_sink.getPath());
        binary_source.setSubpath(original_sink.getSubpath());
        binary_source.initialize();

        BinaryOArchive copied_sink;
        copied_sink.setPath(destination_dir);
        copied_sink.setSubpath(original_sink.getSubpath());
        copied_sink.initialize();

        while (true) {
            const std::vector<double> & binary_data = binary_source.readFromSource();
            if (binary_data.empty()) {
                break;
            }
            copied_sink.sendToSink(binary_data);
        }

        copied_sink.copyMetadataFrom(&original_sink);
    }

    std::unique_ptr<ReportStatisticsDispatcher> dispatcher_;
    bool dirty_ = true;
};

} // namespace statistics
} // namespace sparta

#endif
