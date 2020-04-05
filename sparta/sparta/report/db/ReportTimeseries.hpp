// <ReportTimeseries> -*- C++ -*-

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "simdb/schema/Schema.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb_fwd.hpp"
#include "sparta/report/db/Schema.hpp"
#include "simdb/ObjectManager.hpp"
#include "sparta/report/db/ReportHeader.hpp"

namespace sparta {
namespace db {

class ReportHeader;

/*!
 * Wrapper around a database record (ObjectRef) which provides
 * user-friendly API's to read and write timeseries values and
 * report metadata in the database.
 */
class ReportTimeseries
{
public:
    //! Create a report timeseries wrapper around an
    //! *existing* database record
    explicit ReportTimeseries(std::unique_ptr<simdb::ObjectRef> obj_ref);

    //! Create a new report timeseries
    ReportTimeseries(const simdb::ObjectManager & obj_mgr);

    //! Get the unique database ID for this timeseries.
    //! This ID is unique in the main ReportTimeseries table,
    //! not necessarily globally unique across the entire database.
    uint64_t getId() const;

    //! Timeseries reports typically have some header
    //! information. Access that object with this method
    //! to read or write report metadata.
    ReportHeader & getHeader();

    //! Write SI values at a specific "time point":
    //!     - current simulated time (picoseconds)
    //!     - current root clock cycle
    //!
    //! The database will create an indexed chunk at this "time" value.
    void writeStatisticInstValuesAtTimeT(
        const uint64_t current_picoseconds,
        const uint64_t current_cycle,
        const std::vector<double> & si_values,
        const db::MajorOrdering major_ordering);

    //! Write *compressed* SI values at a specific "time point":
    //!     - current simulated time (picoseconds)
    //!     - current root clock cycle
    //!
    //! The database will create an indexed chunk at this "time" value.
    void writeCompressedStatisticInstValuesAtTimeT(
        const uint64_t current_picoseconds,
        const uint64_t current_cycle,
        const std::vector<char> & compressed_si_values,
        const db::MajorOrdering major_ordering,
        const uint32_t original_num_si_values);

    //! Write SI values between two "time points":
    //!     - simulated time (picoseconds)
    //!     - root clock cycle
    //!
    //! The database will create an indexed chunk for this "time" range.
    void writeStatisticInstValuesInTimeRange(
        const uint64_t starting_picoseconds,
        const uint64_t ending_picoseconds,
        const uint64_t starting_cycle,
        const uint64_t ending_cycle,
        const std::vector<double> & si_values,
        const db::MajorOrdering major_ordering);

    //! Write *compressed* SI values between two "time points":
    //!     - simulated time (picoseconds)
    //!     - root clock cycle
    //!
    //! It is the CALLER'S responsibility to compress this blob.
    //! It will be written to the TimeseriesChunk table and will
    //! be decompressed for you when you later access those SI values.
    //!
    //! The database will create an indexed chunk for this "time" range.
    void writeCompressedStatisticInstValuesInTimeRange(
        const uint64_t beginning_picoseconds,
        const uint64_t ending_picoseconds,
        const uint64_t beginning_cycle,
        const uint64_t ending_cycle,
        const std::vector<char> & compressed_si_values,
        const db::MajorOrdering major_ordering,
        const uint32_t original_num_si_values);

    //! Retrieve all SI data value chunks between "time points" A and B:
    //!     - between simulated time values A and B (picoseconds)
    void getStatisticInstValuesBetweenSimulatedPicoseconds(
        const uint64_t start_picoseconds,
        const uint64_t end_picoseconds,
        std::vector<std::vector<double>> & si_values);

    //! Retrieve all SI data value chunks between "time points" A and B:
    //!     - between root clock cycles A and B
    void getStatisticInstValuesBetweenRootClockCycles(
        const uint64_t start_cycle,
        const uint64_t end_cycle,
        std::vector<std::vector<double>> & si_values);

    //! Retrieve SI data values one "time slice" at a time. This can be
    //! used to incrementally read SI values into memory for processing
    //! in a more performant way than getting all of the values at once
    //! with one of the above APIs.
    class RangeIterator
    {
    public:
        RangeIterator(ReportTimeseries & db_timeseries);

        //! Prepare to retrieve SI values between two simulated
        //! picoseconds. This only sets up the query; call getNext()
        //! to read in the first set of values.
        void positionRangeAroundSimulatedPicoseconds(
            const uint64_t start_picoseconds,
            const uint64_t end_picoseconds);

        //! Prepare to retrieve SI values between two root
        //! clock cycles. This only sets up the query; call getNext()
        //! to read in the first set of values.
        void positionRangeAroundRootClockCycles(
            const uint64_t start_cycle,
            const uint64_t end_cycle);

        //! Advance the iterator to the next set of values. Returns
        //! true if successful, false otherwise (occurs when there
        //! is no more data in this range).
        bool getNext();

        //! Get a pointer to the current SI range's data values.
        const double * getCurrentSliceDataValuesPtr() const;

        //! Get the number of SI data points in the current slice.
        //! This is how many points you can read off of the returned
        //! pointer from getCurrentSliceDataValuesPtr()
        size_t getCurrentSliceNumDataValues() const;

    private:
        class Impl;

        std::shared_ptr<Impl> impl_;
    };

private:
    //! Timeseries database object
    std::unique_ptr<simdb::ObjectRef> obj_ref_;

    //! Report header database object
    std::unique_ptr<ReportHeader> header_;
};

} // namespace db
} // namespace sparta

