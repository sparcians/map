// <ObjectFactory> -*- C++ -*-

#pragma once

#include "simdb/schema/DatabaseTypedefs.hpp"
#include "simdb/schema/ColumnValue.hpp"
#include "simdb_fwd.hpp"

#include <functional>
#include <string>
#include <vector>

namespace simdb {

//! SimDB implementations can register "object factories"
//! for any of their tables. These routines perform table
//! inserts. The return value is the created record's ID.
//! This ID should be unique for the given table the new
//! record lives in.
using AnySizeObjectFactory = std::function<
    DatabaseID(DbConnProxy * db_proxy,
               const std::string & table_name,
               const ColumnValues & obj_values)>;

//! The fixed-size object factory is intended to be a
//! more performant factory implementation for tables
//! that only contain fixed-size columns, i.e. PODs.
using FixedSizeObjectFactory = std::function<
    DatabaseID(DbConnProxy * db_proxy,
               const std::string & table_name,
               const void * raw_bytes_ptr,
               const size_t num_raw_bytes)>;

}

