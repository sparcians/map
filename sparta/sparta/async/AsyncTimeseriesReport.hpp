// <AsyncTimeseriesReport> -*- C++ -*-

#pragma once

#include <math.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/statistics/db/SINodeHierarchy.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsAggregator.hpp"
#include "sparta/statistics/db/SIValuesBuffer.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/report/db/Schema.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace simdb {
class ObjectManager;
}  // namespace simdb

namespace sparta {
    namespace db {
        class ReportHeader;
    }
}  // namespace sparta::db

namespace sparta {
class StatisticInstance;

namespace async {

/*!
 * \brief Use this class when you want to stream all of a report's
 * StatisticInstance values (and optionally any header metadata)
 * to a database. All database writes will be committed off the
 * main thread.
 *
 * The shared AsyncTaskEval object given to the constructor is the
 * one who creates and owns the worker thread. You can create just
 * one worker thread, and share it among as many timeseries objects
 * as you need, by doing something like this:
 *
 * \code
 * std::shared_ptr<AsyncTaskEval> report_thread(new ...)
 *
 * std::unique_ptr<AsyncTimeseriesReport> async_report1(new
 *     AsyncTimeseriesReport(report_thread, report1));
 *
 * std::unique_ptr<AsyncTimeseriesReport> async_report2(new
 *     AsyncTimeseriesReport(report_thread, report2));
 * \endcode
 */
class AsyncTimeseriesReport : public simdb::Notifiable
{
public:
    //! Construct a timeseries report object with:
    //!   - task_queue: A background / worker thread running on a timer.
    //!                 Used so we can push expensive DB writes off the main thread.
    //!   - sim_db:     A shared database object, typically (primarily) owned by the
    //!                 app::Simulation, and shared with its reports / descriptors.
    //!   - root_clk:   The app::Simulation's root clock.
    //!   - report:     The sparta::Report that goes with this database timeseries.
    AsyncTimeseriesReport(simdb::AsyncTaskEval * task_queue,
                          simdb::ObjectManager * sim_db,
                          const Clock & root_clk,
                          const Report & report,
                          const app::FeatureConfiguration::FeatureOptions * feature_opts) :
        task_queue_(task_queue),
        sim_db_(sim_db),
        db_timeseries_(new db::ReportTimeseries(*sim_db_)),
        si_aggregator_(new statistics::ReportStatisticsAggregator(report)),
        report_(report),
        root_clk_(root_clk)
    {
        //Get the aggregator to flatten out all the leaf SI's, and
        //give us that SI vector.
        si_aggregator_->initialize();
        const auto & all_stat_insts = si_aggregator_->getAggregatedSIs();

        //Figure out the compression and row/column-major ordering
        const bool compress = feature_opts->getOptionValue<std::string>(
            "compression", "enabled") == "enabled";
        const bool row_major = feature_opts->getOptionValue<std::string>(
            "si-ordering", "row-major") == "row-major";

        //We currently only support two modes: all of the SI's are
        //compressed, or none of the SI's are compressed. More work
        //needs to be done to design something that is flexible enough
        //to put report update SI values back together when some of
        //those values were compressed, and some were not.
        //
        //Note that this means all of the supportsCompression() methods
        //throughout the stats-related classes are rendered meaningless
        //until we support "half compressed, half uncompressed" report
        //updates. (**We may decide against it entirely and just use
        //compression for all SI's or no compression at all**)
        std::vector<const StatisticInstance*> compression_enabled_sis;
        std::vector<const StatisticInstance*> compression_disabled_sis;
        for (const auto si : all_stat_insts) {
            if (compress) {
                //TODO: Incorporate calls to SI::supportsCompression()
                compression_enabled_sis.emplace_back(si);
            } else {
                compression_disabled_sis.emplace_back(si);
            }
        }

        if (compress) {
            //We'll use an SI value/blob buffer of size 1 for all of
            //the SI's that said "no, I do not support compression".
            //These will be pushed to the task queue for async writes
            //to the SI blob table during each report update.
            if (!compression_disabled_sis.empty()) {
                uncompressed_si_buffer_.reset(new statistics::SIValuesBuffer(
                    compression_disabled_sis,
                    root_clk_));

                if (row_major) {
                    uncompressed_si_buffer_->useRowMajorOrdering();
                } else {
                    uncompressed_si_buffer_->useColumnMajorOrdering();
                }
            }

            //All SI's that support compression will be buffered into
            //a larger SI values buffer, and after enough report updates
            //have hit to fill up this larger buffer, those SI's will be
            //sent to the task queue for async compression / async writes
            //to the SI blob table.
            if (!compression_enabled_sis.empty()) {
                compressed_si_buffer_.reset(new statistics::SIValuesBuffer(
                    compression_enabled_sis,
                    root_clk_));

                if (row_major) {
                    compressed_si_buffer_->useRowMajorOrdering();
                } else {
                    compressed_si_buffer_->useColumnMajorOrdering();
                }

                //Let's aim for a 1MB cap on the amount of memory that an
                //*entire* SI blob will take up after it has been inflated.
                //Start with 1MB available:
                int64_t num_available_bytes_for_compression_buffers = (1 << 20);

                //Subtract away the bytes needed for *uncompressed* SI's...
                num_available_bytes_for_compression_buffers -=
                    (compression_disabled_sis.size() * sizeof(double));

                //Just in case we have more than 1MB of SI's in one report update...
                if (num_available_bytes_for_compression_buffers < 0) {
                    num_available_bytes_for_compression_buffers = 0;
                }

                //And calculate the number of report updates that can be
                //buffered into the other SIValuesBuffer for compression.
                const size_t initial_num_compression_buffers =
                    num_available_bytes_for_compression_buffers /
                    (compression_enabled_sis.size() * sizeof(double));

                compressed_si_buffer_->initializeNumSIBuffers(
                    std::max(initial_num_compression_buffers, 1ul));

                //Since we're using compression, initialize our running
                //tally on the number of bytes pre- and post-compression.
                total_num_bytes_sent_for_compression_ = 0;
                total_num_bytes_after_compression_ = 0;
            }
        }

        else {
            //Put all stats into the uncompressed buffer. We don't have
            //enough SI's that are good candidates for compression.
            uncompressed_si_buffer_.reset(new statistics::SIValuesBuffer(
                all_stat_insts,
                root_clk_));

            if (row_major) {
                uncompressed_si_buffer_->useRowMajorOrdering();
            } else {
                uncompressed_si_buffer_->useColumnMajorOrdering();
            }
        }

        //Uncompressed SI buffers just use one internal buffer. In
        //other words, they don't buffer anything, and we'll send
        //these SI values to the task thread on each update.
        if (uncompressed_si_buffer_ != nullptr) {
            //If the feature options specify "none" for "si-ordering", that means
            //that we are not supposed to use in-memory buffers at all. Every report
            //update will result in its own timeseries chunk/blob record.
            if (feature_opts->getOptionValue<std::string>("si-ordering", "none") == "none") {
                uncompressed_si_buffer_->initializeNumSIBuffers(1);
            } else {
                //Let's aim for 1MB SI chunks
                double target_num_bytes = (1 << 20);
                target_num_bytes /= (compression_disabled_sis.size() * sizeof(double));

                const size_t num_si_buffers = std::max((size_t)target_num_bytes, (size_t)1);
                uncompressed_si_buffer_->initializeNumSIBuffers(num_si_buffers);
            }
        }

        //Register ourselves for notifications that the task queue
        //is about to be flushed
        task_queue_->registerForPreFlushNotifications(*this);

        //One-time population of the entire SI node hierarchy for
        //this timeseries.
        statistics::SINodeHierarchy serializer(*db_timeseries_, report_);
        root_report_node_id_ = serializer.serializeHierarchy(*sim_db_);
    }

    //! Get the root-level report node's database ID.
    simdb::DatabaseID getRootReportNodeDatabaseID() const {
        return root_report_node_id_;
    }

    //! Destruction. Print out a message saying how much compression
    //! we achieved if we have been sending compressed SI blobs to
    //! the database.
    ~AsyncTimeseriesReport() {
        if (compressed_si_buffer_ != nullptr) {
            //Note that we can now safely access our compression variables
            //without acquiring the mutex since there's nothing waiting in
            //the task queue that will call back into us.
            const uint64_t total_num_bytes = total_num_bytes_sent_for_compression_;
            const uint64_t compressed_num_bytes = total_num_bytes_after_compression_;

            const double si_compression_pct =
                1 - (static_cast<double>(compressed_num_bytes) /
                     static_cast<double>(total_num_bytes));

            // Display as a percentage with one decimal point (89.7%)
            const double pretty_print_pct = (floor(si_compression_pct * 10) / 10) * 100.0;

            std::cout << "  [si-compression] Compressed SI blobs ended up being "
                << pretty_print_pct << "% smaller than the raw SI values." << std::endl;
        }  else {
            std::cout << "  [si-compression] We did not perform compression "
                         "of SI values for this report." << std::endl;
        }
    }

    //! Get the sparta::Report this timeseries writer is bound to.
    const Report & getReport() const {
        return report_;
    }

    //! Get the database header object for this timeseries writer.
    //! You can use this to write (or overwrite) report metadata.
    db::ReportHeader & getTimeseriesHeader() {
        return db_timeseries_->getHeader();
    }

    //! Get a list of all the SI's locations in this timeseries report.
    //! This is equivalent to the first row of SI information in the
    //! CSV file (dest_file: out.csv) which looks something like this:
    //!
    //! "scheduler.ticks, scheduler.seconds, top.core0.rob.ipc, ..."
    const std::vector<std::string> & getStatInstLocations() const {
        return si_aggregator_->getStatInstLocations();
    }

    //! Grab all current StatisticInstance values in this report
    //! and queue them up in the background thread to be written
    //! to disk.
    void writeCurrentValues() {
        if (uncompressed_si_buffer_ != nullptr) {
            uncompressed_si_buffer_->bufferCurrentSIValues();
            if (uncompressed_si_buffer_->buffersAreFilled()) {
                const std::vector<double> & values =
                    uncompressed_si_buffer_->getBufferedSIValues();

                uint64_t starting_picoseconds = 0;
                uint64_t ending_picoseconds = 0;
                uint64_t starting_cycles = 0;
                uint64_t ending_cycles = 0;

                uncompressed_si_buffer_->getBeginningAndEndingTimestampsForBufferedSIs(
                    starting_picoseconds,
                    ending_picoseconds,
                    starting_cycles,
                    ending_cycles);

                //Add these values to the task queue for asynchronous processing
                queueStatInstValuesOnWorker_(
                    values,
                    starting_picoseconds,
                    ending_picoseconds,
                    starting_cycles,
                    ending_cycles);

                //Reset the buffer for uncompressed SI's during each report
                //update. Pass in FALSE so the buffer is not initialized with
                //nan's (unnecessary performance hit to do so).
                uncompressed_si_buffer_->resetSIBuffers(false);
            }
        }

        //If we have any compression-enabled SI's, buffer their
        //values right now.
        if (compressed_si_buffer_ != nullptr) {
            compressed_si_buffer_->bufferCurrentSIValues();

            //If the buffer is full, give a deep copy of the raw SI
            //values to a compressor object for async processing
            //on the task thread.
            if (compressed_si_buffer_->buffersAreFilled()) {
                const std::vector<double> & uncompressed_values =
                    compressed_si_buffer_->getBufferedSIValues();

                uint64_t starting_picoseconds = 0;
                uint64_t ending_picoseconds = 0;
                uint64_t starting_cycles = 0;
                uint64_t ending_cycles = 0;

                compressed_si_buffer_->getBeginningAndEndingTimestampsForBufferedSIs(
                    starting_picoseconds,
                    ending_picoseconds,
                    starting_cycles,
                    ending_cycles);

                queueCompressionEnabledStatInstValuesOnWorker_(
                    uncompressed_values,
                    starting_picoseconds,
                    ending_picoseconds,
                    starting_cycles,
                    ending_cycles);

                //Reset the buffer internals. Pass in FALSE so that
                //it knows not to initialize the SI values to nan.
                compressed_si_buffer_->resetSIBuffers(false);
            }
        }
    }

private:
    //! This class makes a deep copy of SI data values on the
    //! main thread, and is added to the background thread /
    //! worker queue for async DB writes.
    class StatInstValuesWriter : public simdb::WorkerTask
    {
    public:
        StatInstValuesWriter(const std::shared_ptr<db::ReportTimeseries> & db_timeseries,
                             const std::vector<double> & values,
                             const db::MajorOrdering major_ordering,
                             const uint64_t starting_picoseconds,
                             const uint64_t ending_picoseconds,
                             const uint64_t starting_cycles,
                             const uint64_t ending_cycles) :
            db_timeseries_(db_timeseries),
            si_values_(values),
            major_ordering_(major_ordering),
            starting_picoseconds_(starting_picoseconds),
            ending_picoseconds_(ending_picoseconds),
            starting_cycles_(starting_cycles),
            ending_cycles_(ending_cycles)
        {}

    private:
        //WorkerTask implementation. Called on a background thread.
        void completeTask() override {
            db_timeseries_->writeStatisticInstValuesInTimeRange(
                starting_picoseconds_,
                ending_picoseconds_,
                starting_cycles_,
                ending_cycles_,
                si_values_,
                major_ordering_);
        }

        //Timeseries database object. This will persist all of
        //the report header / metadata and SI raw values that
        //we give it in a database.
        std::shared_ptr<db::ReportTimeseries> db_timeseries_;

        //Aggregated / contiguous SI values flattened into one
        //vector of doubles
        const std::vector<double> si_values_;

        //Row-major or column-major ordering of SI values
        const db::MajorOrdering major_ordering_;

        //Timestamps for the blob we are writing to the DB
        utils::ValidValue<uint64_t> starting_picoseconds_;
        utils::ValidValue<uint64_t> ending_picoseconds_;
        utils::ValidValue<uint64_t> starting_cycles_;
        utils::ValidValue<uint64_t> ending_cycles_;
    };

    //! This class makes a deep copy of SI data values on the
    //! main thread, and is added to the background thread /
    //! worker queue for async DB writes.
    //!
    //! The raw SI values are compressed before writing them
    //! to the database.
    class CompressedStatInstValuesWriter : public simdb::WorkerTask
    {
    public:
        CompressedStatInstValuesWriter(
                             const std::shared_ptr<db::ReportTimeseries> & db_timeseries,
                             const std::vector<double> & values,
                             const db::MajorOrdering major_ordering,
                             const uint64_t starting_picoseconds,
                             const uint64_t ending_picoseconds,
                             const uint64_t starting_cycles,
                             const uint64_t ending_cycles) :
            db_timeseries_(db_timeseries),
            si_values_(values),
            major_ordering_(major_ordering),
            starting_picoseconds_(starting_picoseconds),
            ending_picoseconds_(ending_picoseconds),
            starting_cycles_(starting_cycles),
            ending_cycles_(ending_cycles)
        {}

        //! Assign a callback to be called once the compression
        //! is complete. This will tell you the raw (uncompressed)
        //! number of bytes that went into the compression library,
        //! and the number of bytes that resulted after compression.
        //!
        //! Note that this callback will be called *before* the
        //! compressed data is physically written to the database.
        typedef uint32_t RawNumBytes;
        typedef uint32_t CompressedNumBytes;
        typedef std::function<void(RawNumBytes, CompressedNumBytes)> CompressionCallback;

        void setPostCompressionCallback(CompressionCallback cb) {
            post_compression_callback_ = cb;
        }

    private:
        //WorkerTask implementation. Called on a background thread.
        void completeTask() override;

        //Timeseries database object. This will persist all of
        //the report header / metadata and SI raw values that
        //we give it in a database.
        std::shared_ptr<db::ReportTimeseries> db_timeseries_;

        //Aggregated / contiguous SI values flattened into one
        //vector of doubles. This is given to us in *row-major*
        //format. If this is the equivalent CSV:
        //
        //  SI1   SI2    SI3   SI4
        //  ---   ---   ----   ---
        //  1.3    78   4000   3.4
        //  1.5    79   4007   3.4
        //
        //Then this vector will initially be given to us as:
        //
        //  [1.3, 1.5, 78, 79, 4000, 4007, 3.4, 3.4]
        //
        //We will run this vector as-is through zlib to get
        //the compressed bytes that will then be written to
        //the database.
        const std::vector<double> si_values_;

        //Row-major or column-major ordering of SI values
        const db::MajorOrdering major_ordering_;

        //Time values for our SI blob(s).
        const uint64_t starting_picoseconds_;
        const uint64_t ending_picoseconds_;
        const uint64_t starting_cycles_;
        const uint64_t ending_cycles_;

        //Optional user callback that will be invoked after
        //compressing their data, letting them know how many
        //raw bytes went into the compression library, and
        //how many compressed bytes came out of it.
        utils::ValidValue<CompressionCallback> post_compression_callback_;
    };

    //! Package up the current SI blob values, and add a new
    //! worker task to the background thread's processing queue.
    void queueStatInstValuesOnWorker_(
        const std::vector<double> & values,
        const uint64_t starting_picoseconds,
        const uint64_t ending_picoseconds,
        const uint64_t starting_cycles,
        const uint64_t ending_cycles)
    {
        if (values.empty()) {
            return;
        }

        const db::MajorOrdering major_ordering =
            uncompressed_si_buffer_->getMajorOrdering();

        std::unique_ptr<StatInstValuesWriter> async_writer(
            new StatInstValuesWriter(
                db_timeseries_,
                values,
                major_ordering,
                starting_picoseconds,
                ending_picoseconds,
                starting_cycles,
                ending_cycles));

        task_queue_->addWorkerTask(std::move(async_writer));
    }

    //! Package up the current SI blob values for asynchronous
    //! compression. Put this potentially expensive task on the
    //! worker thread.
    void queueCompressionEnabledStatInstValuesOnWorker_(
        const std::vector<double> & values,
        const uint64_t starting_picoseconds,
        const uint64_t ending_picoseconds,
        const uint64_t starting_cycles,
        const uint64_t ending_cycles)
    {
        if (values.empty()) {
            return;
        }

        const db::MajorOrdering major_ordering =
            compressed_si_buffer_->getMajorOrdering();

        std::unique_ptr<CompressedStatInstValuesWriter> async_compressor(
            new CompressedStatInstValuesWriter(
                db_timeseries_,
                values,
                major_ordering,
                starting_picoseconds,
                ending_picoseconds,
                starting_cycles,
                ending_cycles));

        async_compressor->setPostCompressionCallback([&]
            (const uint32_t num_bytes_in, const uint32_t num_bytes_out)
        {
            postCompressionNotification_(num_bytes_in, num_bytes_out);
        });

        task_queue_->addWorkerTask(std::move(async_compressor));
    }

    //! Post-compression callback that will let us know how
    //! much we are gaining by using compression. We can use
    //! this information to tweak the compression API calls
    //! during simulation if needed.
    void postCompressionNotification_(
        const uint32_t num_bytes_pre_compression,
        const uint32_t num_bytes_post_compression)
    {
        std::lock_guard<std::mutex> guard(mutex_);

        total_num_bytes_sent_for_compression_ += num_bytes_pre_compression;
        total_num_bytes_after_compression_ += num_bytes_post_compression;

        //TODO: This functionality is not complete yet, but we should be
        //able to determine (in the sim loop) if we can get away with asking
        //zlib to switch to Z_BEST_COMPRESSION if we're easily keeping up with
        //the rate of incoming SI data, or Z_BEST_SPEED if we're not really
        //benefitting much from attempting SI compression. In the worst case,
        //we also need the ability to pull the plug on compression entirely
        //and switch to Z_NO_COMPRESSION if we're struggling to keep up with
        //the simulation.
        //
        //Note that the worker thread object would have to be changed a little
        //bit to emit notifications when it is consuming work at a slower rate
        //than it is receiving work packets. Before doing that, it may be useful
        //to just add a stdout message during the final saveReports() call
        //that says something like:
        //
        //     Database thread has 17 things left it needs to do...
        //     COMPLETE - The worker thread took 1.38 seconds to flush
        //     the task queue at the end of simulation.
        //
        //If the database thread is *not* keeping up with the simulation, then
        //this printout would show that we are left with more and more things
        //that we still have to write to the database when we try running
        //longer and longer simulations.
        if (false) {
            const double si_compression_pct =
                1 - (static_cast<double>(total_num_bytes_after_compression_.getValue()) /
                     static_cast<double>(total_num_bytes_sent_for_compression_.getValue()));

            (void) si_compression_pct;

            //Look at the current running "si_compression_pct" and decide
            //what to do: go for more compression, go for more speed, turn
            //off compression, re-enable compression, or stay the course.
        }
    }

    //! This callback is registered with the AsyncTaskEval to
    //! let us know when a synchronous flush is being forced.
    //! This gives us a chance to push any buffered data out
    //! of the SIValuesBuffer objects into the worker queue.
    void notifyTaskQueueAboutToFlush() override final {
        pushBufferedDataToTaskQueue_();
    }

    //! Push any buffered SI values that are pending compression
    //! into the task queue. This is called during synchronization
    //! points like simulation pause/stop.
    void pushBufferedDataToTaskQueue_() {
        pushUncompressedBufferedDataToTaskQueue_();
        pushCompressedBufferedDataToTaskQueue_();
    }

    //! Push any buffered data from the uncompressed SI values
    //! containers.
    void pushUncompressedBufferedDataToTaskQueue_() {
        if (uncompressed_si_buffer_ == nullptr) {
            return;
        }
        if (uncompressed_si_buffer_->buffersAreEmpty()) {
            return;
        }

        const std::vector<double> & values =
            uncompressed_si_buffer_->getBufferedSIValues();

        uint64_t starting_picoseconds = 0;
        uint64_t ending_picoseconds = 0;
        uint64_t starting_cycles = 0;
        uint64_t ending_cycles = 0;

        uncompressed_si_buffer_->getBeginningAndEndingTimestampsForBufferedSIs(
            starting_picoseconds,
            ending_picoseconds,
            starting_cycles,
            ending_cycles);

        //Add these values to the task queue for asynchronous processing
        queueStatInstValuesOnWorker_(
            values,
            starting_picoseconds,
            ending_picoseconds,
            starting_cycles,
            ending_cycles);

        //Reset the buffer for uncompressed SI's during each report
        //update. Pass in FALSE so the buffer is not initialized with
        //nan's (unnecessary performance hit to do so).
        uncompressed_si_buffer_->resetSIBuffers(false);
    }

    //! Push any buffered data from the compressed SI values
    //! containers.
    void pushCompressedBufferedDataToTaskQueue_() {
        if (compressed_si_buffer_ == nullptr) {
            return;
        }
        if (compressed_si_buffer_->buffersAreEmpty()) {
            return;
        }

        //If there is any pending SI data for the compressor,
        //hand it over for async processing on the task thread.
        const std::vector<double> & uncompressed_values =
            compressed_si_buffer_->getBufferedSIValues();

        uint64_t starting_picoseconds = 0;
        uint64_t ending_picoseconds = 0;
        uint64_t starting_cycles = 0;
        uint64_t ending_cycles = 0;

        compressed_si_buffer_->getBeginningAndEndingTimestampsForBufferedSIs(
            starting_picoseconds,
            ending_picoseconds,
            starting_cycles,
            ending_cycles);

        queueCompressionEnabledStatInstValuesOnWorker_(
            uncompressed_values,
            starting_picoseconds,
            ending_picoseconds,
            starting_cycles,
            ending_cycles);

        //Reset the buffer internals. Pass in FALSE so that
        //it knows not to initialize the SI values to nan.
        compressed_si_buffer_->resetSIBuffers(false);
    }

    //! Shared worker thread object. We will give DB writes to
    //! this "task queue" to handle for us in the background.
    simdb::AsyncTaskEval * task_queue_ = nullptr;

    //! Shared database which holds all SI values. This object
    //! is shared with the app::Simulation.
    simdb::ObjectManager * sim_db_ = nullptr;

    //! Wrapper around the timeseries database table(s).
    std::shared_ptr<db::ReportTimeseries> db_timeseries_;

    //! SI values are aggregated into one vector<double> with
    //! the help of this object. This makes DB writes easier.
    std::unique_ptr<statistics::ReportStatisticsAggregator> si_aggregator_;

    //! SI values buffer which contains uncompressed statistics values.
    std::unique_ptr<statistics::SIValuesBuffer> uncompressed_si_buffer_;

    //! SI values buffer which contains SI values that will be sent
    //! for async compression when it becomes full.
    std::unique_ptr<statistics::SIValuesBuffer> compressed_si_buffer_;

    //! Keep a running tally on the number of bytes that went into
    //! the compressor, and the number of bytes that resulted after
    //! compression. We'll use this info to tweak the size of the
    //! compression buffer during simulation if needed.
    utils::ValidValue<uint64_t> total_num_bytes_sent_for_compression_;
    utils::ValidValue<uint64_t> total_num_bytes_after_compression_;

    //! Mutex to protect the compression-related variables. These
    //! are accessed on the worker thread when we are asynchronously
    //! told how the latest SI blob compression went.
    mutable std::mutex mutex_;

    //! Report from which we collect all SI values and header info
    const Report & report_;

    //! The simulation's root clock. Used in order to get the current
    //! "time values" when we are asked to write the SI blobs into the
    //! database.
    const Clock & root_clk_;

    //! ID of the root-level report node in the database
    simdb::DatabaseID root_report_node_id_ = 0;
};

} // namespace async
} // namespace sparta
