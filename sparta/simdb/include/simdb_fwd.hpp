// <simdb_fwd> -*- C++ -*-

#pragma once

#include "simdb/schema/DatabaseTypedefs.hpp"

/*!
 * \brief Forward declarations of SimDB classes.
 * Declared/defined here so downstream projects
 * can freely include it in any of their header
 * files without worrying about impacting compile
 * times.
 */

namespace simdb {

//! Forward declarations   --------------------------
class AsyncTaskController;
class AsyncTaskEval;
class Column;
class ColumnValueBase;
class DatabaseNamespace;
class DatabaseRoot;
class DbConnProxy;
class ObjectProxy;
class ObjectQuery;
class ObjectManager;
class ObjectRef;
class ResultIter;
class Schema;
class SQLiteConnProxy;
class Table;
class TableProxy;
class TableRef;
class TableSummaries;
class TimerThread;

template <typename>
class ConcurrentQueue;

}

