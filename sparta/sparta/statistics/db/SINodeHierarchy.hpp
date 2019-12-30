// <SINodeHierarchy> -*- C++ -*-

#ifndef __SPARTA_SI_NODE_HIERARCHY_H__
#define __SPARTA_SI_NODE_HIERARCHY_H__

#include <memory>

#include "simdb_fwd.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"

namespace simdb {
class ObjectManager;
}  // namespace simdb

namespace sparta {

class Report;

namespace db {
    class ReportTimeseries;
}

namespace statistics {

/*!
 * \brief This class serializes a sparta::Report's entire
 * SI tree (node names, parent nodes / child nodes / etc.)
 * into the SINodeHierarchy table in the database object
 * you provide.
 *
 * TODO: This is specifically for timeseries reports.
 * All other report formats go through ReportNodeHierarchy
 * to get their report/SI trees written to a different
 * table. It would be easier to work with a schema that
 * can put timeseries and non-timeseries report hierarchies
 * and metadata into the same set of tables, but for now
 * they are separate.
 */
class SINodeHierarchy
{
public:
    //! Construct with the sparta::Report we are serializing,
    //! and the database ReportTimeseries object we are writing
    //! all report information into.
    SINodeHierarchy(
        db::ReportTimeseries & db_timeseries,
        const Report & report);

    //! Write out all report/subreport/SI hierarchy metadata
    //! for this report into the provided database. Returns
    //! the database ID corresponding to the root-level Report
    //! node in this hierarchy.
    simdb::DatabaseID serializeHierarchy(simdb::ObjectManager & obj_mgr);

private:
    class Impl;

    std::shared_ptr<Impl> impl_;
};

} // namespace statistics
} // namespace sparta

#endif
