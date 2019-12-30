// <ReportHeader> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_DATABASE_REPORT_HEADER_H__
#define __SPARTA_STATISTICS_DATABASE_REPORT_HEADER_H__

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "simdb_fwd.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"

namespace sparta {
namespace db {

class ReportTimeseries;

/*!
 * \brief Wrapper around a database record (ObjectRef)
 * which provides user-friendly API's to read and write
 * report metadata in the database.
 */
class ReportHeader
{
public:
    //! Create a ReportHeader wrapper around an *existing* database record
    explicit ReportHeader(std::unique_ptr<simdb::ObjectRef> obj_ref);

    //! Create a new ReportHeader object in the database
    ReportHeader(const simdb::ObjectManager & obj_mgr);

    //! Get this record's unique database ID. Returns the
    //! ID of the underlying ObjectRef this ReportHeader
    //! wraps.
    uint64_t getId() const;

    //! Get this record's ObjectRef. This is the same record you
    //! would get if you asked us 'getId()', and gave that ID to
    //! the ObjectManager to get that record for the "ReportHeader"
    //! table.
    const simdb::ObjectRef & getObjectRef() const;
    simdb::ObjectRef & getObjectRef();

    //! Some report headers are standalone database
    //! records, but timeseries reports always have
    //! header metadata that go with them. Calling
    //! this method will make the table-to-table
    //! connection (primary key / foreign key) that
    //! will let you do this:
    //!
    //!     ReportHeader header(...)
    //!     header.setReportStartTime(1500)
    //!
    //!     ReportTimeseries timeseries(...)
    //!     header.setOwningTimeseries(timeseries)
    //!     ...
    //!
    //!     timeseries.getHeader().getReportStartTime()
    //!         ^^^ Returns 1500, which was read from the
    //!             physical database, NOT from any member
    //!             variable in any object.
    void setOwningTimeseries(const ReportTimeseries & ts);

    //! METADATA SETTERS ---------------------------------
    void setReportName(
        const std::string & report_name);

    void setReportStartTime(
        const uint64_t start_time);

    void setReportEndTime(
        const uint64_t end_time);

    void setSourceReportDescDestFile(
        const std::string & fname);

    void setCommaSeparatedSILocations(
        const std::string & si_locations);

    void setSourceReportNumStatInsts(
        const uint32_t num_stat_insts);

    void setStringMetadata(
        const std::string & name,
        const std::string & value);

    //! METADATA GETTERS ---------------------------------
    //!
    //!   - Note that none of these getters return const &,
    //!     since this object is just a wrapper requesting
    //!     data from the database. It does not actually
    //!     store anything in memory.
    std::string getReportName() const;

    uint64_t getReportStartTime() const;

    uint64_t getReportEndTime() const;

    std::string getSourceReportDescDestFile() const;

    std::string getCommaSeparatedSILocations() const;

    std::string getStringMetadata(const std::string & name) const;

    std::map<std::string, std::string> getAllStringMetadata() const;

    std::map<std::string, std::string> getAllHiddenStringMetadata() const;

private:
    std::unique_ptr<simdb::ObjectRef> obj_ref_;
};

} // namespace db
} // namespace sparta

#endif
