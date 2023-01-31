// <ReportTimeseries> -*- C++ -*-

#include "sparta/report/db/ReportTimeseries.hpp"

#include <boost/lexical_cast.hpp>
#include <cassert>
#include <sqlite3.h>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <string>
#include <utility>
#include <zlib.h>

#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/report/db/Schema.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/utils/BlobHelpers.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
//SQLite-specific headers
#include "simdb/impl/sqlite/TransactionUtils.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/Errors.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"

namespace sparta {
namespace db {

//! Template helper for getting a particular property value
//! from a database record (row). The onFound() method will
//! be called by sqlite for every record that matches the
//! query you run.
template <typename PropertyDataT>
class MultiSelectPropertyQuery {
public:
    int onFound(int argc, char ** argv, char **) {
        assert(argc == 1);

        std::vector<PropertyDataT> & prop_values = property_values_;
        // TODO swap for our own
        prop_values.emplace_back(boost::lexical_cast<PropertyDataT>(argv[0]));

        //Property is valid, return SQLITE_OK for success
        return SQLITE_OK;
    }

    bool found() const {
        return !property_values_.empty();
    }

    const std::vector<PropertyDataT> & getPropertyValues() const {
        assert(found());
        return property_values_;
    }

private:
    std::vector<PropertyDataT> property_values_;
};

ReportTimeseries::ReportTimeseries(std::unique_ptr<simdb::ObjectRef> obj_ref) :
    obj_ref_(std::move(obj_ref))
{
    //Try to find a ReportHeader record whose TimeseriesID is
    //*our* database ID.
    std::unique_ptr<simdb::ObjectRef> our_header_obj =
        obj_ref_->getObjectManager().findObject("ReportHeader", obj_ref_->getId());

    //There is currently no reason why a timeseries object would
    //exist without any header record to go with it.
    assert(our_header_obj != nullptr);
    header_.reset(new ReportHeader(std::move(our_header_obj)));
}

ReportTimeseries::ReportTimeseries(const simdb::ObjectManager & obj_mgr)
{
    //Create a brand new timeseries object in the database
    std::unique_ptr<simdb::TableRef> timeseries_tbl = obj_mgr.getTable("Timeseries");
    obj_ref_ = timeseries_tbl->createObject();

    //And create a new header object to go with it
    header_.reset(new ReportHeader(obj_ref_->getObjectManager()));
    header_->setOwningTimeseries(*this);
}

uint64_t ReportTimeseries::getId() const
{
    return obj_ref_->getId();
}

ReportHeader & ReportTimeseries::getHeader()
{
    return *header_;
}

void LOCAL_ReportTimeseries_writeStatisticInstValuesInTimeRange(
    const uint64_t beginning_picoseconds,
    const uint64_t ending_picoseconds,
    const uint64_t beginning_cycle,
    const uint64_t ending_cycle,
    const simdb::Blob & blob_descriptor,
    const size_t num_si_values_in_blob,
    const bool si_values_blob_was_compressed,
    const db::MajorOrdering major_ordering,
    const simdb::DatabaseID timeseries_id,
    const simdb::ObjectManager & obj_mgr)
{
    assert(ending_picoseconds >= beginning_picoseconds);
    assert(ending_cycle >= beginning_cycle);

    //We currently do not support indexed chunks at time values
    //that are larger than int64 max. This is due to SQLite not
    //supporting uint64_t out of the box. More investigation
    //needs to go into how we can index chunks against uint64_t's.
    constexpr uint64_t max_time_t = std::numeric_limits<int64_t>::max();
    if (ending_picoseconds > max_time_t || ending_cycle > max_time_t) {
        throw simdb::DBException("Simulation database does not currently accept SI ")
            << "chunks at time values larger than int64_t max. "
            << "This error occured at " << ending_picoseconds
            << " simulated picoseconds (" << ending_cycle << " root clock cycles).";
    }

    obj_mgr.safeTransaction([&]() {
        //Create a new chunk record
        std::unique_ptr<simdb::TableRef> chunk_tbl = obj_mgr.getTable("TimeseriesChunk");
        std::unique_ptr<simdb::ObjectRef> chunk_ref = chunk_tbl->createObject();

        //Tell that record which timeseries it belongs to (foreign key)
        chunk_ref->setPropertyInt32("TimeseriesID", timeseries_id);

        //When we are *not* using SI compression, we write each report update
        //into the database in its own chunk. In that case, StartPS==EndPS
        //and StartCycle==EndCycle
        //
        //However, if we *are* using SI compression, then multiple report
        //updates will be buffered in memory for a certain amount of time.
        //When it is time to go to the database, the start/end ps/cycle
        //values may look like this:
        //
        //   Update #1    Occurred at start = 100ps / 600 cycles
        //   Update #2    Occurred at start = 200ps / 1200 cycles
        //   Update #3    Occurred at start = 300ps / 1800 cycles
        //       **send to the database now**
        //          -> beginning_picoseconds = 100
        //          -> ending_picoseconds = 300
        //          -> beginning_cycle = 600
        //          -> ending_cycle = 1800
        chunk_ref->setPropertyUInt64("StartPS", beginning_picoseconds);
        chunk_ref->setPropertyUInt64("EndPS", ending_picoseconds);
        chunk_ref->setPropertyUInt64("StartCycle", beginning_cycle);
        chunk_ref->setPropertyUInt64("EndCycle", ending_cycle);

        //Create a new SI blob record
        std::unique_ptr<simdb::TableRef> chunk_values_tbl = obj_mgr.getTable("StatInstValues");
        std::unique_ptr<simdb::ObjectRef> chunk_values_ref = chunk_values_tbl->createObject();

        //Write the SI values as a raw blob to this record
        chunk_values_ref->setPropertyBlob("RawBytes", blob_descriptor);

        //Tell this blob how many double SI values are in this
        //blob. If compression was used for this SI blob, we will
        //need this bit of information later when we go to inflate
        //the blob back into a vector<double>.
        chunk_values_ref->setPropertyInt32("NumPts", num_si_values_in_blob);

        //Tell this blob if it was compressed or not. If so, it will
        //need to send the raw char's through zlib when these SI's
        //are read back out from the database.
        chunk_values_ref->setPropertyInt32(
            "WasCompressed",
            si_values_blob_was_compressed ? 1 : 0);
        chunk_values_ref->setPropertyInt32(
            "MajorOrdering", (uint32_t)major_ordering);

        //Tell the SI blob record which TimeseriesChunk it belongs
        //to (foreign key)
        chunk_values_ref->setPropertyInt32("TimeseriesChunkID", chunk_ref->getId());
    });
}

void ReportTimeseries::writeStatisticInstValuesAtTimeT(
    const uint64_t current_picoseconds,
    const uint64_t current_cycle,
    const std::vector<double> & si_values,
    const db::MajorOrdering major_ordering)
{
    writeStatisticInstValuesInTimeRange(
        current_picoseconds,
        current_picoseconds,
        current_cycle,
        current_cycle,
        si_values,
        major_ordering);
}

void ReportTimeseries::writeCompressedStatisticInstValuesAtTimeT(
    const uint64_t current_picoseconds,
    const uint64_t current_cycle,
    const std::vector<char> & compressed_si_values,
    const db::MajorOrdering major_ordering,
    const uint32_t original_num_si_values)
{
    writeCompressedStatisticInstValuesInTimeRange(
        current_picoseconds,
        current_picoseconds,
        current_cycle,
        current_cycle,
        compressed_si_values,
        major_ordering,
        original_num_si_values);
}

void ReportTimeseries::writeStatisticInstValuesInTimeRange(
    const uint64_t beginning_picoseconds,
    const uint64_t ending_picoseconds,
    const uint64_t beginning_cycle,
    const uint64_t ending_cycle,
    const std::vector<double> & si_values,
    const db::MajorOrdering major_ordering)
{
    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &si_values[0];
    blob_descriptor.num_bytes = si_values.size() * sizeof(double);

    const size_t num_si_values_in_blob = si_values.size();
    const bool si_values_blob_was_compressed = false;

    const simdb::DatabaseID timeseries_id = getId();
    const simdb::ObjectManager & obj_mgr = obj_ref_->getObjectManager();

    LOCAL_ReportTimeseries_writeStatisticInstValuesInTimeRange(
        beginning_picoseconds,
        ending_picoseconds,
        beginning_cycle,
        ending_cycle,
        blob_descriptor,
        num_si_values_in_blob,
        si_values_blob_was_compressed,
        major_ordering,
        timeseries_id,
        obj_mgr);
}

void ReportTimeseries::writeCompressedStatisticInstValuesInTimeRange(
    const uint64_t beginning_picoseconds,
    const uint64_t ending_picoseconds,
    const uint64_t beginning_cycle,
    const uint64_t ending_cycle,
    const std::vector<char> & compressed_si_values,
    const db::MajorOrdering major_ordering,
    const uint32_t original_num_si_values)
{
    if (compressed_si_values.empty()) {
        return;
    }

    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &compressed_si_values[0];
    blob_descriptor.num_bytes = compressed_si_values.size();

    const size_t num_si_values_in_blob = original_num_si_values;
    const bool si_values_blob_was_compressed = true;

    const simdb::DatabaseID timeseries_id = getId();
    const simdb::ObjectManager & obj_mgr = obj_ref_->getObjectManager();

    LOCAL_ReportTimeseries_writeStatisticInstValuesInTimeRange(
        beginning_picoseconds,
        ending_picoseconds,
        beginning_cycle,
        ending_cycle,
        blob_descriptor,
        num_si_values_in_blob,
        si_values_blob_was_compressed,
        major_ordering,
        timeseries_id,
        obj_mgr);
}

//! Local helper method to get a range of SI values. Whether the
//! range is between simulated picoseconds, clock cycles, etc.
//! doesn't matter to us here, since the string command that is
//! passed in is all we need.
//!
//! This query returns a vector of blobs, where each blob (vector
//! element) contains all the SI values at one report update.
void LOCAL_getStatisticInstValuesFromPreparedSqlCommand(
    const simdb::ObjectManager & obj_mgr,
    const std::string & command,
    std::vector<std::vector<double>> & si_values,
    const uint32_t num_stat_insts)
{
    const simdb::SQLiteConnProxy * db_proxy =
        dynamic_cast<const simdb::SQLiteConnProxy*>(obj_mgr.getDbConn());

    if (!db_proxy) {
        std::cout << "  [simdb] ReportTimeseries object can only "
                  << "be used with SQLite databases" << std::endl;
        return;
    }

    obj_mgr.safeTransaction([&]() {
        //Create a callback handler that will collect all
        //database ID's for the chunks we are asking for
        MultiSelectPropertyQuery<simdb::DatabaseID> query;

        //Run the SELECT statement
        eval_sql_select(db_proxy,
                        command,
                        +[](void * callback_obj, int argc, char ** argv, char ** col_names) {
                            return static_cast<MultiSelectPropertyQuery<simdb::DatabaseID>*>(callback_obj)->
                                onFound(argc, argv, col_names);
                        },
                        &query);

        //Did we find any? It is not an error if 'No, there are
        //no chunks found'. Asking for data that falls outside
        //our range is not invalid, we just return nothing.
        if (query.found()) {
            const auto & chunk_ids = query.getPropertyValues();
            si_values.reserve(chunk_ids.size());

            std::vector<std::unique_ptr<simdb::ObjectRef>> chunk_refs;
            obj_mgr.findObjects("StatInstValues", chunk_ids, chunk_refs);
            assert(chunk_refs.size() == chunk_ids.size());

            for (const auto & chunk_ref : chunk_refs) {
                assert(chunk_ref != nullptr);

                const bool was_compressed = chunk_ref->getPropertyInt32("WasCompressed");
                const db::MajorOrdering major_ordering =
                    (db::MajorOrdering)chunk_ref->getPropertyInt32("MajorOrdering");
                const bool needs_transpose =
                    (major_ordering == db::MajorOrdering::COLUMN_MAJOR);

                //If compression was used, get the raw char bytes back from the
                //database, inflate them using zlib, and put the final values
                //into a new vector at the back of 'si_values'.
                if (was_compressed) {
                    std::vector<char> compressed_blob;
                    chunk_ref->getPropertyBlob("RawBytes", compressed_blob);
                    assert(!compressed_blob.empty());

                    const size_t num_si_values = chunk_ref->getPropertyInt32("NumPts");
                    assert(num_si_values > 0);

                    //Re-inflate the compressed SI blob
                    z_stream infstream;
                    infstream.zalloc = Z_NULL;
                    infstream.zfree = Z_NULL;
                    infstream.opaque = Z_NULL;

                    //Setup the source stream
                    infstream.avail_in = (uInt)(compressed_blob.size());
                    infstream.next_in = (Bytef*)(&compressed_blob[0]);

                    //Setup the destination stream
                    std::vector<double> interleaved_sis;
                    interleaved_sis.resize(num_si_values);
                    infstream.avail_out = (uInt)(interleaved_sis.size() * sizeof(double));
                    infstream.next_out = (Bytef*)(&interleaved_sis[0]);

                    //Inflate it!
                    inflateInit(&infstream);
                    inflate(&infstream, Z_FINISH);
                    inflateEnd(&infstream);

                    //If this SI chunk was written in row-major format, simply rearrange
                    //the single vector<double> into vector<vector<double>> like so:
                    //
                    //     blob from database = [3, 5, 3, 4, 4, 8, 9, 7, 1]
                    //     num SI's per update = 3
                    //     output SI's = [[3, 5, 3], [4, 4, 8], [9, 7, 1]]
                    if (!needs_transpose) {
                        assert(interleaved_sis.size() % num_stat_insts == 0);
                        const uint32_t num_report_updates_here =
                            interleaved_sis.size() / num_stat_insts;

                        for (uint32_t relative_update_idx = 0;
                             relative_update_idx < num_report_updates_here;
                             ++relative_update_idx)
                        {
                            si_values.emplace_back(num_stat_insts);
                            std::vector<double> & original_si_values = si_values.back();
                            memcpy(&original_si_values[0],
                                   &interleaved_sis[relative_update_idx * num_stat_insts],
                                   original_si_values.size() * sizeof(double));
                        }
                        continue;
                    }

                    //SI blobs that are in column-major order are a little different.
                    //We're not done yet - we still have to rearrange the SI
                    //values. Instead of being interleaved like this:
                    //
                    //  [SI_1a, SI_1b, SI_1c, SI_2a, SI_2b, SI_2c]
                    //   -------------------  -------------------
                    //           (1)                   (2)         <- organized by SI
                    //   (three report updates, where SI values are
                    //          next to their previous value)
                    //
                    //They need to be back in their original order like this:
                    //
                    //  [SI_1a, SI_2a, SI_1b, SI_2b, SI_1c, SI_2c]
                    //
                    //   ------------  ------------  ------------
                    //       (1)            (2)           (3)      <- organized by
                    //    (three report updates, where SI values        report update
                    //     are next to the SI's that are adjacent
                    //  to them in the physical tree when traversed
                    //                  depth-first)
                    assert(interleaved_sis.size() % num_stat_insts == 0);
                    const uint32_t num_report_updates_here = interleaved_sis.size() / num_stat_insts;

                    for (uint32_t relative_update_idx = 0;
                         relative_update_idx < num_report_updates_here;
                         ++relative_update_idx)
                    {
                        si_values.emplace_back(num_stat_insts);
                        std::vector<double> & original_si_values = si_values.back();

                        //Perform the column-major-to-row-major transpose. This is the
                        //last step to get back the original SI values from a compressed,
                        //column-major ordered dataset.
                        uint32_t read_idx = relative_update_idx;
                        for (uint32_t write_idx = 0; write_idx < num_stat_insts; ++write_idx) {
                            original_si_values[write_idx] = interleaved_sis[read_idx];
                            read_idx += num_report_updates_here;
                        }
                    }
                }

                //No compression for this blob - just ask for the blob raw bytes
                //and deinterleave them into their original SI ordering.
                else {
                    std::vector<double> interleaved_sis;
                    chunk_ref->getPropertyBlob("RawBytes", interleaved_sis);
                    assert(interleaved_sis.size() % num_stat_insts == 0);
                    const uint32_t num_report_updates_here =
                        interleaved_sis.size() / num_stat_insts;

                    for (uint32_t relative_update_idx = 0;
                         relative_update_idx < num_report_updates_here;
                         ++relative_update_idx)
                    {
                        si_values.emplace_back(std::vector<double>());
                        std::vector<double> & original_si_values = si_values.back();
                        original_si_values.resize(num_stat_insts);

                        if (!needs_transpose) {
                            memcpy(&original_si_values[0],
                                   &interleaved_sis[relative_update_idx * num_stat_insts],
                                   original_si_values.size() * sizeof(double));
                            continue;
                        }

                        //Continuing with the example from above, we might now
                        //have a original_si_values vector sized to 2 elements
                        //ready to go:
                        //
                        //   original_si_values = [---, ---]
                        uint32_t read_idx = relative_update_idx;
                        for (uint32_t write_idx = 0; write_idx < num_stat_insts; /*see below*/) {
                            original_si_values[write_idx] = interleaved_sis[read_idx];

                            //Advance the read and write indices
                            read_idx += num_report_updates_here;
                            ++write_idx;
                        }
                    }
                }
            }
        }

        //Clear and shrink the results just to be safe
        else {
            si_values.clear();
            si_values.shrink_to_fit();
        }
    });
}

//! Retrieve all SI data value chunks between "time points" A and B:
//!     - between simulated time values A and B (picoseconds)
void ReportTimeseries::getStatisticInstValuesBetweenSimulatedPicoseconds(
    const uint64_t start_picoseconds,
    const uint64_t end_picoseconds,
    std::vector<std::vector<double>> & si_values)
{
    const auto & obj_mgr = obj_ref_->getObjectManager();
    const std::string table_name = obj_mgr.getQualifiedTableName(
        "TimeseriesChunk", "Stats");

    std::ostringstream oss;
    oss << " SELECT Id FROM " << table_name << " WHERE "
        << start_picoseconds << " <= StartPS AND "
        << end_picoseconds << " >= EndPS AND "
        << "TimeseriesID == " << getId();

    const std::string command = oss.str();

    //TODO: This piece of metadata is needed to decompress SI data.
    //Find another way to decompress blobs without requiring this.
    const uint32_t num_stat_insts =
        getHeader().getObjectRef().getPropertyInt32("NumStatInsts");

    LOCAL_getStatisticInstValuesFromPreparedSqlCommand(
        obj_mgr, command, si_values, num_stat_insts);
}

//! Retrieve all SI data value chunks between "time points" A and B:
//!     - between root clock cycles A and B
void ReportTimeseries::getStatisticInstValuesBetweenRootClockCycles(
    const uint64_t start_cycle,
    const uint64_t end_cycle,
    std::vector<std::vector<double>> & si_values)
{
    const auto & obj_mgr = obj_ref_->getObjectManager();
    const std::string table_name = obj_mgr.getQualifiedTableName(
        "TimeseriesChunk", "Stats");

    std::ostringstream oss;
    oss << " SELECT Id FROM " << table_name << " WHERE "
        << start_cycle << " <= StartCycle AND "
        << end_cycle << " >= EndCycle AND "
        << "TimeseriesID == " << getId();

    const std::string command = oss.str();

    //TODO: This piece of metadata is needed to decompress SI data.
    //Find another way to decompress blobs without requiring this.
    const uint32_t num_stat_insts =
        getHeader().getObjectRef().getPropertyInt32("NumStatInsts");

    LOCAL_getStatisticInstValuesFromPreparedSqlCommand(
        obj_mgr, command, si_values, num_stat_insts);
}

//! Implementation class for RangeIterator. This class allows
//! us to incrementally process SI blobs across an arbitrarily
//! large dataset without needing to bring the whole dataset
//! into memory.
class ReportTimeseries::RangeIterator::Impl
{
public:
    explicit Impl(ReportTimeseries & db_timeseries) :
        db_timeseries_(db_timeseries)
    {}

    //! Setup the database cursor at the beginning of
    //! a specified range in simulated picoseconds.
    bool positionRangeAroundSimulatedPicoseconds(
        const uint64_t start_picoseconds,
        const uint64_t end_picoseconds)
    {
        const auto & obj_mgr = db_timeseries_.getHeader().
            getObjectRef().getObjectManager();

        const std::string table_name = obj_mgr.getQualifiedTableName(
            "TimeseriesChunk", "Stats");

        std::ostringstream oss;
        oss << " SELECT Id FROM " << table_name << " WHERE "
            << start_picoseconds << " <= StartPS AND "
            << end_picoseconds << " >= EndPS AND "
            << "TimeseriesID == " << db_timeseries_.getId();

        const std::string command = oss.str();
        return runQueryAndPositionRangeIterator_(command);
    }

    //! Setup the database cursor at the beginning of
    //! a specified range in root clock cycles.
    bool positionRangeAroundRootClockCycles(
        const uint64_t start_cycle,
        const uint64_t end_cycle)
    {
        const auto & obj_mgr = db_timeseries_.getHeader().
            getObjectRef().getObjectManager();

        const std::string table_name = obj_mgr.getQualifiedTableName(
            "TimeseriesChunk", "Stats");

        std::ostringstream oss;
        oss << " SELECT Id FROM " << table_name << " WHERE "
            << start_cycle << " <= StartCycle AND "
            << end_cycle << " >= EndCycle AND "
            << "TimeseriesID == " << db_timeseries_.getId();

        const std::string command = oss.str();
        return runQueryAndPositionRangeIterator_(command);
    }

    //! Advance the iterator to the next set of values. Return
    //! true if successful.
    bool getNext()
    {
        if (iterator_ == nullptr) {
            return false;
        }

        ++current_deserialized_vector_idx_;
        if (current_deserialized_vector_idx_ >= deserialized_si_values_.size()) {
            //! We've exhausted the blob we had in memory. Now we
            //! need to advance the DB iterator, which will bring
            //! in the next SI blob. Note this is not necessarily
            //! one SI *row* (update) - a blob can contain many
            //! SI rows.
            const bool valid_advance = iterator_->getNext();
            if (valid_advance) {
                //! We have a new blob with one or more SI rows in
                //! it. Deserialize it and reset our read index
                //! back to zero. Say this blob has three rows'
                //! worth of SI data in it. Our "read index" goes
                //! from 0, to 1, to 2, and then back to zero since
                //! we'll have used up this three-row blob.
                deserializeCurrentBlob_();
                current_deserialized_vector_idx_ = 0;
                return true;
            } else {
                //! No more blobs, or the DB iterator failed for
                //! some other reason.
                current_deserialized_vector_idx_ =
                    std::numeric_limits<uint32_t>::max();
                return false;
            }
        }
        return true;
    }

    //! Get a pointer to the current SI range's data values.
    const double * getCurrentSliceDataValuesPtr() const
    {
        if (current_deserialized_vector_idx_ >= deserialized_si_values_.size()) {
            return nullptr;
        }

        const std::vector<double> & values =
            deserialized_si_values_[current_deserialized_vector_idx_];

        return !values.empty() ? &values[0] : nullptr;
    }

    //! Get the number of SI data points in the current slice.
    //! This is how many points you can read off of the returned
    //! pointer from getCurrentSliceDataValuesPtr()
    size_t getCurrentSliceNumDataValues() const
    {
        if (current_deserialized_vector_idx_ >= deserialized_si_values_.size()) {
            return 0;
        }

        const std::vector<double> & values =
            deserialized_si_values_[current_deserialized_vector_idx_];

        return values.size();
    }

private:
    //! Setup the database cursor at the beginning of
    //! a specified range. The input string is a SQL
    //! statement we've already computed.
    bool runQueryAndPositionRangeIterator_(const std::string & command)
    {
        const simdb::ObjectRef & header_ref = db_timeseries_.getHeader().getObjectRef();
        const simdb::ObjectManager & obj_mgr = header_ref.getObjectManager();

        const simdb::SQLiteConnProxy * db_proxy =
            dynamic_cast<const simdb::SQLiteConnProxy*>(obj_mgr.getDbConn());

        if (!db_proxy) {
            std::cout << "  [simdb] ReportTimeseries object can only "
                      << "be used with SQLite databases" << std::endl;
            return false;
        }

        num_stat_insts_ = header_ref.getPropertyInt32("NumStatInsts");

        struct FindHelper {
            explicit FindHelper(std::vector<simdb::DatabaseID> & chunk_ids) :
                chunk_ids(chunk_ids)
            {}

            int onFound(int argc, char ** argv, char ** col_names) {
                assert(argc == 1);
                assert(strcmp(col_names[0], "Id") == 0);
                chunk_ids.emplace_back(atoi(argv[0]));
                return SQLITE_OK;
            }

            std::vector<simdb::DatabaseID> & chunk_ids;
        };

        //! We run two queries to set up this database cursor:
        //!  - First query finds all chunk database IDs we need
        //!  - The second uses ObjectQuery to give us a cursor
        //!    at the beginning of the StatInstValues rows that
        //!    we are looking for (but it does so without reading
        //!    any of those rows' blobs into memory).
        FindHelper finder(chunk_ids_);
        eval_sql_select(db_proxy, command,
                        +[](void * caller, int argc, char ** argv, char ** col_names) {
                            return static_cast<FindHelper*>(caller)->onFound(
                                argc, argv, col_names);
                        },
                        &finder);

        iterator_.reset();
        if (chunk_ids_.empty()) {
            return false;
        }

        //SELECT RawBytes,NumPts,WasCompressed,MajorOrdering
        //FROM StatInstValues
        //WHERE TimeseriesChunkID IN (12,15,88,106)
        //                           **************
        //                             chunk_ids_
        //
        simdb::ObjectQuery query(obj_mgr, "StatInstValues");

        query.addConstraints("TimeseriesChunkID",
                             simdb::constraints::in_set,
                             chunk_ids_);

        query.writeResultIterationsTo("RawBytes", &raw_bytes_,
                                      "NumPts", &num_pts_,
                                      "WasCompressed", &was_compressed_,
                                      "MajorOrdering", &major_ordering_);

        iterator_ = query.executeQuery();
        return (iterator_ != nullptr);
    }

    //! SI blobs are stored in one of a number of configurations.
    //! These include compression on or off, and row-major vs.
    //! column-major SI ordering. This method puts SI blobs
    //! back into a "human readable" vector of doubles: the
    //! row-major raw double values that originally came from
    //! the simulation/report.
    void deserializeCurrentBlob_()
    {
        deserialized_si_values_.clear();
        std::vector<std::vector<double>> & si_values = deserialized_si_values_;
        const bool needs_transpose =
            (major_ordering_ == (uint32_t)db::MajorOrdering::COLUMN_MAJOR);

        //If compression was used, get the raw char bytes back from the
        //database, inflate them using zlib, and put the final values
        //into a new vector at the back of 'si_values'.
        if (was_compressed_) {
            std::vector<char> & compressed_blob = raw_bytes_;
            const size_t num_si_values = num_pts_;
            assert(num_si_values > 0);

            //Re-inflate the compressed SI blob
            z_stream infstream;
            infstream.zalloc = Z_NULL;
            infstream.zfree = Z_NULL;
            infstream.opaque = Z_NULL;

            //Setup the source stream
            infstream.avail_in = (uInt)(compressed_blob.size());
            infstream.next_in = (Bytef*)(&compressed_blob[0]);

            //Setup the destination stream
            std::vector<double> interleaved_sis;
            interleaved_sis.resize(num_si_values);
            infstream.avail_out = (uInt)(interleaved_sis.size() * sizeof(double));
            infstream.next_out = (Bytef*)(&interleaved_sis[0]);

            //Inflate it!
            inflateInit(&infstream);
            inflate(&infstream, Z_FINISH);
            inflateEnd(&infstream);

            //If this SI chunk was written in row-major format, simply rearrange
            //the single vector<double> into vector<vector<double>> like so:
            //
            //     blob from database = [3, 5, 3, 4, 4, 8, 9, 7, 1]
            //     num SI's per update = 3
            //     output SI's = [[3, 5, 3], [4, 4, 8], [9, 7, 1]]
            if (!needs_transpose) {
                assert(interleaved_sis.size() % num_stat_insts_ == 0);
                const uint32_t num_report_updates_here =
                    interleaved_sis.size() / num_stat_insts_;

                for (uint32_t relative_update_idx = 0;
                     relative_update_idx < num_report_updates_here;
                     ++relative_update_idx)
                {
                    si_values.emplace_back(num_stat_insts_);
                    std::vector<double> & original_si_values = si_values.back();
                    memcpy(&original_si_values[0],
                           &interleaved_sis[relative_update_idx * num_stat_insts_],
                           original_si_values.size() * sizeof(double));
                }
                //! Nothing left to do for blobs in row-major order...
                return;
            }
            //!...but column-major ordering is a little different...

            //We're not done yet - we still have to rearrange the SI
            //values. Instead of being interleaved like this:
            //
            //  [SI_1a, SI_1b, SI_1c, SI_2a, SI_2b, SI_2c]
            //   -------------------  -------------------
            //           (1)                   (2)         <- organized by SI
            //   (three report updates, where SI values are
            //          next to their previous value)
            //
            //They need to be back in their original order like this:
            //
            //  [SI_1a, SI_2a, SI_1b, SI_2b, SI_1c, SI_2c]
            //
            //   ------------  ------------  ------------
            //       (1)            (2)           (3)      <- organized by
            //    (three report updates, where SI values        report update
            //     are next to the SI's that are adjacent
            //  to them in the physical tree when traversed
            //                  depth-first)
            assert(interleaved_sis.size() % num_stat_insts_ == 0);
            const uint32_t num_report_updates_here =
                interleaved_sis.size() / num_stat_insts_;

            for (uint32_t relative_update_idx = 0;
                 relative_update_idx < num_report_updates_here;
                 ++relative_update_idx)
            {
                si_values.emplace_back(num_stat_insts_);
                std::vector<double> & original_si_values = si_values.back();

                //Continuing with the example from above, we might now
                //have a original_si_values vector sized to 2 elements
                //ready to go:
                //
                //   original_si_values = [---, ---]
                uint32_t read_idx = relative_update_idx;
                for (uint32_t write_idx = 0; write_idx < num_stat_insts_; ++write_idx) {
                    original_si_values[write_idx] = interleaved_sis[read_idx];
                    read_idx += num_report_updates_here;
                }
            }
        }

        //No compression for this blob - just ask for the blob raw bytes
        //and deinterleave them into their original SI ordering.
        else {
            const simdb::VectorAlias<double> interleaved_sis(raw_bytes_);
            assert(interleaved_sis.size() % num_stat_insts_ == 0);
            const uint32_t num_report_updates_here =
                interleaved_sis.size() / num_stat_insts_;

            for (uint32_t relative_update_idx = 0;
                 relative_update_idx < num_report_updates_here;
                 ++relative_update_idx)
            {
                si_values.emplace_back(num_stat_insts_);
                std::vector<double> & original_si_values = si_values.back();

                if (!needs_transpose) {
                    memcpy(&original_si_values[0],
                           &interleaved_sis[relative_update_idx * num_stat_insts_],
                           original_si_values.size() * sizeof(double));
                    continue;
                }

                //Continuing with the example from above, we might now
                //have a original_si_values vector sized to 2 elements
                //ready to go:
                //
                //   original_si_values = [---, ---]
                uint32_t read_idx = relative_update_idx;
                for (uint32_t write_idx = 0; write_idx < num_stat_insts_; ++write_idx) {
                    original_si_values[write_idx] = interleaved_sis[read_idx];
                    read_idx += num_report_updates_here;
                }
            }
        }
    }

    ReportTimeseries & db_timeseries_;
    std::vector<simdb::DatabaseID> chunk_ids_;
    std::unique_ptr<simdb::ResultIter> iterator_;

    std::vector<char> raw_bytes_;
    std::vector<std::vector<double>> deserialized_si_values_;

    int32_t num_pts_ = 0;
    uint32_t num_stat_insts_ = 0;
    int32_t major_ordering_ = (int32_t)db::MajorOrdering::ROW_MAJOR;
    uint32_t current_deserialized_vector_idx_ = 0;

    //In C++, the "was compressed or not" variable would be a
    //bool, but sqlite does not support boolean data types.
    //It is stored in the database as an unsigned int, so this
    //variable here is an int32_t to match what the database
    //query will return.
    int32_t was_compressed_ = true;
};

ReportTimeseries::RangeIterator::RangeIterator(ReportTimeseries & db_timeseries) :
    impl_(new ReportTimeseries::RangeIterator::Impl(db_timeseries))
{
}

void ReportTimeseries::RangeIterator::positionRangeAroundSimulatedPicoseconds(
    const uint64_t start_picoseconds,
    const uint64_t end_picoseconds)
{
    impl_->positionRangeAroundSimulatedPicoseconds(
        start_picoseconds, end_picoseconds);
}

void ReportTimeseries::RangeIterator::positionRangeAroundRootClockCycles(
    const uint64_t start_cycle,
    const uint64_t end_cycle)
{
    impl_->positionRangeAroundRootClockCycles(
        start_cycle, end_cycle);
}

bool ReportTimeseries::RangeIterator::getNext()
{
    return impl_->getNext();
}

const double * ReportTimeseries::RangeIterator::getCurrentSliceDataValuesPtr() const
{
    return impl_->getCurrentSliceDataValuesPtr();
}

size_t ReportTimeseries::RangeIterator::getCurrentSliceNumDataValues() const
{
    return impl_->getCurrentSliceNumDataValues();
}

} // namespace db
} // namespace sparta
