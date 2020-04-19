// <AsyncNonTimeseriesReport> -*- C++ -*-

#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/ObjectManager.hpp"
#include "sparta/report/db/SingleUpdateReport.hpp"
#include "sparta/report/db/ReportNodeHierarchy.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsAggregator.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
namespace async {

/*!
 * \brief This class is used to serialize all non-timeseries
 * report formats to a SimDB. It writes all SI values on a
 * background thread, typically on the same thread as other
 * database-related tasks.
 */
class AsyncNonTimeseriesReport
{
public:
    //! Construct with a shared worker thread / task queue,
    //! a shared SimDB object, and the sparta::Report you wish
    //! to write to this database.
    AsyncNonTimeseriesReport(
            simdb::AsyncTaskEval * task_queue,
            simdb::ObjectManager * sim_db,
            const Report & report,
            const app::FeatureConfiguration::FeatureOptions * simdb_opts) :
        task_queue_(task_queue),
        sim_db_(sim_db),
        report_(report)
    {
        if (simdb_opts) {
            using_compression_ = simdb_opts->getOptionValue<std::string>(
                "compression", "enabled") == "enabled";
        }
    }

    //! Write a stringized piece of metadata for this report
    //! in the database.
    void setStringMetadataByNameAndValue(
        const std::string & name,
        const std::string & value)
    {
        using simdb::constraints;

        //If we already have our report DB node ID, write the metadata now.
        if (root_report_node_id_ > 0) {
            auto meta_tbl = sim_db_->getTable("RootReportNodeMetadata");
            if (meta_tbl->updateRowValues("Value", value).
                          forRecordsWhere(
                              "ReportNodeID", constraints::equal, root_report_node_id_,
                              "Name", constraints::equal, name)) {
            } else {
                //The metadata overwrite failed, which means that this
                //piece of metadata was never written to begin with.
                //Create a new record for it.
                meta_tbl->createObjectWithArgs(
                    "ReportNodeID", root_report_node_id_,
                    "Name", name,
                    "Value", value);
            }
        }

        //If we do not have our report DB node ID yet, save this
        //metadata value for later. We'll write it when we find out
        //our report DB ID.
        else {
            string_metadata_[name] = value;
        }
    }

    //! Capture our report's current SI values and write them to
    //! the database on a background thread. Since this class is
    //! meant to serialize non-timeseries reports (i.e. single-
    //! update reports), you typically would only call this method
    //! once. But you can call it as many times as you want for
    //! any reason, and the SI values will just be overwritten
    //! in the database with each call.
    void writeCurrentValues()
    {
        //In order to match what the legacy formatters write into the
        //reports (json, text, etc.) we will serialize all metadata
        //for this report now, at the same time we write the actual
        //SI values to the database.
        serializeReportMetadata_();

        const auto & all_stat_insts = si_aggregator_->getAggregatedSIs();
        for (const StatisticInstance * si : all_stat_insts) {
            //These SI's are connected to their fixed spot in
            //the aggregator's values vector. We just have to
            //make the getValue() call.
            si->getValue();
        }

        const std::vector<double> & si_values = si_aggregator_->readFromSource();
        queueStatInstValuesOnWorker_(si_values);
    }

    //! Get the root-level report node's database ID. This will
    //! equal 0 (unset) until writeCurrentValues() is called,
    //! which occurs at the end of simulation during the call
    //! to Simulation::saveReports()
    simdb::DatabaseID getRootReportNodeDatabaseID() const {
        return root_report_node_id_;
    }

    //! Get the SimDB object we are using. This is the same database
    //! that we share with the app::Simulation.
    simdb::ObjectManager * getSimulationDatabase() {
        return sim_db_;
    }

private:
    //! SI values writer which is invoked on a background thread.
    class StatInstValuesWriter : public simdb::WorkerTask
    {
    public:
        //! This object is only used to forward an SI values vector
        //! along to the *actual* SingleUpdateReport object which
        //! does the DB write. Construct with the writer object
        //! and the SI values you want queued on the background
        //! thread.
        StatInstValuesWriter(std::shared_ptr<db::SingleUpdateReport> & si_writer,
                             const std::vector<double> & si_values,
                             const bool using_compression) :
            si_values_writer_(si_writer),
            si_values_(si_values),
            using_compression_(using_compression)
        {}

    private:
        //WorkerTask implementation. Called on a background thread.
        void completeTask() override;

        //Wrapper around the database record who holds our SI values.
        std::shared_ptr<db::SingleUpdateReport> si_values_writer_;

        //Aggregated / contiguous SI values flattened into one
        //vector of doubles
        const std::vector<double> si_values_;

        //Compression is enabled by default, but can be explicitly
        //disabled if desired
        const bool using_compression_;
    };

    //! Put a deep copy of the incoming SI values onto the
    //! background task thread to be written to the database
    //! shortly.
    void queueStatInstValuesOnWorker_(
        const std::vector<double> & values)
    {
        if (values.empty()) {
            return;
        }

        std::unique_ptr<simdb::WorkerTask> async_writer(
            new StatInstValuesWriter(si_values_writer_, values, using_compression_));

        if (task_queue_) {
            task_queue_->addWorkerTask(std::move(async_writer));
        } else {
            async_writer->completeTask();
        }
    }

    //Write out the physical hierarchy of this report, including all
    //subreports and all SI's, and all their metadata as well.
    void serializeReportMetadata_()
    {
        if (root_report_node_id_ > 0) {
            return;
        }

        sim_db_->safeTransaction([&]() {
            statistics::ReportNodeHierarchy serializer(&report_);
            root_report_node_id_ = serializer.serializeHierarchy(*sim_db_);
            sparta_assert(root_report_node_id_ > 0);

            si_values_writer_.reset(
                new db::SingleUpdateReport(*sim_db_, root_report_node_id_));

            si_aggregator_.reset(new statistics::ReportStatisticsAggregator(report_));
            si_aggregator_->initialize();

            serializer.serializeReportNodeMetadata(*sim_db_);
            serializer.serializeReportStyles(*sim_db_);

            for (const auto & meta : string_metadata_) {
                const auto & name = meta.first;
                const auto & val = meta.second;
                serializer.setMetadataCommonToAllNodes(name, val, *sim_db_);
            }
            string_metadata_.clear();
        });
    }

    //! Shared worker thread object. We will give DB writes
    //! to this task queue which will take care of them on a
    //! background thread.
    simdb::AsyncTaskEval * task_queue_ = nullptr;

    //! Shared database which holds all SI values. This object
    //! is shared with the app::Simulation and maybe others.
    simdb::ObjectManager * sim_db_ = nullptr;

    //! SI values are aggregated into one vector<double> with
    //! the help of this object.
    std::unique_ptr<statistics::ReportStatisticsAggregator> si_aggregator_;

    //! SimDB wrapper around the tables that are used for
    //! serializing single-update, non-timeseries report
    //! formats.
    std::shared_ptr<db::SingleUpdateReport> si_values_writer_;

    //! Report from which we collect all SI values and header info
    const Report & report_;

    //! ID of the root-level report node in the database
    simdb::DatabaseID root_report_node_id_ = 0;

    //! Name-value pairs of metadata to be written to the database
    std::map<std::string, std::string> string_metadata_;

    //! This report writer supports compressed and uncompressed values
    bool using_compression_ = true;
};

} // namespace async
} // namespace sparta

