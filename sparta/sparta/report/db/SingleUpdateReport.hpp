// <SingleUpdateReport> -*- C++ -*-

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "simdb_fwd.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"

namespace simdb {
class ObjectManager;
}  // namespace simdb

namespace sparta {
namespace db {

/*!
 * \brief Wrapper around SimDB tables that are dedicated
 * to report/SI persistence of single-update, non-timeseries
 * report formats.
 *
 * \note All methods in this class are performed synchronously.
 * Typically, you will only call the "write*()" method once
 * for this object, and synchronous operation yields basically
 * the same performance as asynchronous. However, if you have
 * many of these reports to write in a SimDB, consider using
 * this class together with AsyncNonTimeseriesReport for better
 * overall performanace across all of the single-update report
 * records.
 */
class SingleUpdateReport
{
public:
    //! Construct with an ObjectRef to an existing record
    //! in the SingleUpdateStatInstValues table.
    explicit SingleUpdateReport(
        std::unique_ptr<simdb::ObjectRef> obj_ref);

    //! Add a new entry in the SingleUpdateStatInstValues
    //! table. This record will be added to the given SimDB
    //! you pass in, and the database ID corresponding to
    //! the root-level Report node (typically retrieved
    //! from ReportNodeHierarchy).
    SingleUpdateReport(
        const simdb::ObjectManager & obj_mgr,
        const simdb::DatabaseID root_report_node_id);

    //! Get this report record's database ID.
    int getId() const;

    //! Write the given SI values to this database record.
    //! No compression will be performed.
    void writeStatisticInstValues(
        const std::vector<double> & si_values);

    //! Write the *already compressed* SI values into
    //! the database record.
    void writeCompressedStatisticInstValues(
        const std::vector<char> & compressed_si_values,
        const uint32_t original_num_si_values);

    //! Retrieve the SI values for this report. As this
    //! is for single-update reports only, there are no
    //! start/end time points like you see with the read
    //! API's for the ReportTimeseries class.
    void getStatisticInstValues(
        std::vector<double> & si_values);

private:
    std::unique_ptr<simdb::ObjectRef> obj_ref_;
    simdb::DatabaseID root_report_node_id_ = 0;
};

} // namespace db
} // namespace sparta

