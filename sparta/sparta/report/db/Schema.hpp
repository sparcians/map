// <Schema> -*- C++ -*-

#pragma once

#include "simdb/schema/Schema.hpp"

#include <cstdint>

namespace sparta {
namespace db {

//! Build a SimDB schema object that can hold all report artifacts
//! and StatisticInstance values for SPARTA simulators. This schema
//! can be given to a simdb::ObjectManager to instantiate a physical
//! database connection.
void buildSimulationDatabaseSchema(simdb::Schema & schema);

//! Configure the default TableSummaries object for SPARTA simulation
//! databases. This will provide default implementations for common
//! summary calculations like min/max/average, and possibly others.
void configureDatabaseTableSummaries(simdb::TableSummaries & summaries);

//! \brief Enum specifying whether blobs' data values
//! are stored in row-major or column-major format
enum class MajorOrdering : int32_t
{
    ROW_MAJOR,
    COLUMN_MAJOR
};

} // namespace db
} // namespace sparta

