// <TransactionUtils> -*- C++ -*-

#pragma once

#include "simdb/Errors.hpp"
#include "simdb_fwd.hpp"

//Standard headers
#include <exception>

namespace simdb {

//! \brief SQLite may return error codes when evaluating a SQL
//! statement, i.e. with either "eval_sql()" or "eval_sql_select()"
//! below. We may trap these exceptions and keep retrying the
//! SQL statement until successful, or decide to rethrow the
//! exception:
//!
//!    SQLite gives error code:        SimDB throws:
//!    --------------------------      -----------------------------
//!      SQLITE_BUSY                     simdb::SqlFileLockedException()
//!      SQLITE_LOCKED                   simdb::SqlTableLockedException()

class SqlFileLockedException : public DBAccessException
{
public:
    const char * what() const noexcept override {
        return "The database file is locked";
    }
};

class SqlTableLockedException : public DBAccessException
{
public:
    const char * what() const noexcept override {
        return "A table in the database is locked";
    }
};

//! \brief Evaluate a SQL command on an ObjectManager's connection proxy.
void eval_sql(const SQLiteConnProxy * db_proxy,
              const std::string & command);

//! \brief Optional callback that will be forwarded to sqlite when
//! executing a SELECT command. Use this with eval_sql_select()
//! calls. Your callback will be called once for each matching
//! record. Example usage:
//!
//!   Say we want to execute the following statement:
//!       SELECT (First,Last,Age) FROM Customers WHERE Balance > 1000
//!
//! This statement could return any number of records, or none.
//! We could set up a SELECT callback like this:
//!
//! \code
//!   struct SelectCustomersCallback {
//!       std::vector<std::tuple<std::string, std::string, int>> matches;
//!
//!       int processMatchingRecord(int argc, char ** argv, char ** col_names) {
//!           //Should be 3 columns returned for this statement
//!           assert(argc == 3);
//!
//!           //We might not care about the column names, but they should be:
//!           assert(col_names[0] == std::string("First"));
//!           assert(col_names[1] == std::string("Last"));
//!           assert(col_names[2] == std::string("Age"));
//!
//!           //The most important part... the column values:
//!           std::string first_name = argv[0];
//!           std::string last_name = argv[1];
//!           int age = atoi(argv[2]);
//!           matches.emplace_back(first_name, last_name, age);
//!       }
//!   };
//!
//!   //IMPORTANT - Your object's callback function must return 'int'
//!   //            or it will fail to compile ("int processMatchingRecord...")
//!
//!   //Make one of these callback objects:
//!   SelectCustomersCallback select_cb;
//!
//!   //Say we have an SQLiteConnProxy 'db_proxy' nearby... now call:
//!   eval_sql_select(db_proxy,
//!                   "SELECT (First,Last,Age) FROM Customers WHERE Balance > 1000",
//!                   +[](void * callback_obj, int argc, char ** argv, char ** col_names) {
//!                       return static_cast<SelectCustomersCallback*>(callback_obj)->
//!                           processMatchingRecord(argc, argv, col_names);
//!                   },
//!                   &select_cb);
//!
//!   //Now the 'select_cb.matches' member variable has your results.
//! \endcode
//!
typedef int                 //Return 0 to indicate success (*see note below)
(*sqlite_select_callback)(
    void * caller_ptr,      //Object provided in the 4th argument of eval_sql_select()
    int argc,               //The number of columns in row
    char ** argv,           //An array of strings representing fields in the row
    char ** col_names);     //An array of strings representing column names
    //
    // *If something goes wrong when the SELECT statement calls
    //  your callback, throw an exception. Do not return specific
    //  SQL error codes.

//! \brief Evaluate a SELECT SQL command on an open sqlite connection.
void eval_sql_select(const SQLiteConnProxy * db_proxy,
                     const std::string & command,
                     sqlite_select_callback callback,
                     void * callback_obj);

} // namespace simdb

