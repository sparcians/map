// <SQLiteConnection> -*- C++ -*-

#include "simdb/ObjectManager.hpp"
#include "simdb/utils/MathUtils.hpp"
#include "simdb/Errors.hpp"

//SQLite-specific headers
#include "simdb/impl/sqlite/Schema.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/impl/sqlite/TransactionUtils.hpp"
#include <sqlite3.h>

//Standard headers
#include <fstream>
#include <numeric>

namespace simdb {

//! Local utility to turn any 8, 16, or 32 bit integer
//! column value into an int32_t
int getColumnValueAsInt32(const ColumnValueBase & col)
{
    using dt = ColumnDataType;

    switch (col.getDataType()) {
        case dt::char_t: {
            const auto val = col.getAs<char>();
            return static_cast<int>(val);
        }
        case dt::int8_t: {
            const auto val = col.getAs<int8_t>();
            return static_cast<int>(val);
        }
        case dt::uint8_t: {
            const auto val = col.getAs<uint8_t>();
            return static_cast<int>(val);
        }
        case dt::int16_t: {
            const auto val = col.getAs<int16_t>();
            return static_cast<int>(val);
        }
        case dt::uint16_t: {
            const auto val = col.getAs<uint16_t>();
            return static_cast<int>(val);
        }
        case dt::int32_t: {
            const auto val = col.getAs<int32_t>();
            return static_cast<int>(val);
        }
        case dt::uint32_t: {
            const auto val = col.getAs<uint32_t>();
            return static_cast<int>(val);
        }
        default:
            throw DBException("Invalid call to getColumnValueAsInt32() ")
                << "- the ColumnValueBase object passed in has a value "
                << "that cannot be cast to 32-bit int.";
    }
}

//! Local utility to turn any 64 bit integer column value
//! into an int64_t
sqlite3_int64 getColumnValueAsInt64(const ColumnValueBase & col)
{
    using dt = ColumnDataType;

    switch (col.getDataType()) {
        case dt::int64_t: {
            const auto val = col.getAs<int64_t>();
            return static_cast<sqlite3_int64>(val);
        }
        case dt::uint64_t: {
            const auto val = col.getAs<uint64_t>();
            return static_cast<sqlite3_int64>(val);
        }
        default:
            throw DBException("Invalid call to getColumnValueAsInt64() ")
                << "- the ColumnValueBase object passed in has a value "
                << "that cannot be cast to 64-bit int.";
    }
}

//! Local utility to turn any floating point column value
//! into a double
double getColumnValueAsDouble(const ColumnValueBase & col)
{
    using dt = ColumnDataType;

    switch (col.getDataType()) {
        case dt::float_t: {
            const auto val = col.getAs<float>();
            return static_cast<double>(val);
        }
        case dt::double_t: {
            const auto val = col.getAs<double>();
            return static_cast<double>(val);
        }
        default:
            throw DBException("Invalid call to getColumnValueAsDouble() ")
                << "- the ColumnValueBase object passed in has a value "
                << "that cannot be cast to double.";
    }
}

//! Local helper method used by INSERT and UPDATE code in
//! SQLiteConnProxy code below.
void LOCAL_finalizeInsertOrUpdateStatement(
    sqlite3_stmt * prepared_stmt,
    const ColumnValues & col_values)
{
    int rc = SQLITE_OK;
    auto check_sql = [&rc]() {
        if (rc != SQLITE_OK) {
            throw DBException("An error was encountered while ")
               << "a TableRef object was writing to the database. "
                << "The sqlite error code was " << rc << ".";
        }
    };
    auto check_done = [&rc]() {
        if (rc != SQLITE_OK && rc != SQLITE_DONE) {
            throw DBException("An error was encountered while ")
                << "a TableRef object was writing to the database. "
                << "The sqlite error code was " << rc << ".";
       }
    };

    //Scoped object which finalizes a SQLite statement
    struct OnCreateOrUpdateExit {
        OnCreateOrUpdateExit(sqlite3_stmt * prepared_stmt) :
            stmt_(prepared_stmt)
        {}
        ~OnCreateOrUpdateExit() {
            if (stmt_) {
                sqlite3_finalize(stmt_);
            }
        }
    private:
        sqlite3_stmt * stmt_ = nullptr;
    };

    OnCreateOrUpdateExit scoped_exit(prepared_stmt);
    (void) scoped_exit;

    using dt = ColumnDataType;

    //Bind the TableRef's column values to the prepared statement.
    for (int idx = 0; idx < (int)col_values.size(); ++idx) {
        const int sql_col_idx = idx + 1;
        const ColumnValueBase & col = col_values[idx];

        switch (col.getDataType()) {
            case dt::fkey_t:
            case dt::char_t:
            case dt::int8_t:
            case dt::uint8_t:
            case dt::int16_t:
            case dt::uint16_t:
            case dt::int32_t:
            case dt::uint32_t: {
                const int val = getColumnValueAsInt32(col);
                rc = sqlite3_bind_int(prepared_stmt, sql_col_idx, val);
                break;
            }

            case dt::int64_t:
            case dt::uint64_t: {
                const sqlite3_int64 val = getColumnValueAsInt64(col);
                rc = sqlite3_bind_int64(prepared_stmt, sql_col_idx, val);
                break;
            }

            case dt::float_t:
            case dt::double_t: {
                const double val = getColumnValueAsDouble(col);
                rc = sqlite3_bind_double(prepared_stmt, sql_col_idx, val);
                break;
            }

            case dt::string_t: {
                const char * val = col.getAs<const char*>();
                rc = sqlite3_bind_text(prepared_stmt, sql_col_idx, val, -1, 0);
                break;
            }

            case ColumnDataType::blob_t: {
                const Blob blob_descriptor = col.getAs<Blob>();
                rc = sqlite3_bind_blob(prepared_stmt, sql_col_idx,
                                       blob_descriptor.data_ptr,
                                       (int)blob_descriptor.num_bytes,
                                       0);
                break;
            }

            default:
                throw DBException("Unrecognized column data type encountered");
        }
        check_sql();
    }

    rc = sqlite3_step(prepared_stmt);
    check_done();
}

//Loop over a Table's columns one by one, and create
//a SQL statement that can be used with CREATE TABLE.
//Column names, data types, and value defaults are
//used here. Example SQL might look like this:
//
//  First TEXT, Last TEXT, Age INT, Balance FLOAT DEFAULT 50.00
//                                                -------------
//                                           (default $50.00 balance!)
//
std::string getColumnsSqlCommand(const Table & table)
{
    std::ostringstream oss;
    for (const auto & column : table) {
        oss << column->getName() << " " << column->getDataType();
        if (column->hasDefaultValue()) {
            oss << " DEFAULT " << column->getDefaultValueAsString();
        }
        oss << ", ";
    }
    std::string command = oss.str();

    //Trim the trailing ", "
    if (command.back() == ' ') { command.pop_back(); }
    if (command.back() == ',') { command.pop_back(); }

    return command;
}

//! Execute a SQL statement against an open database connection.
//! The optional callback arguments are only used for SELECT
//! statements (eval_sql_select).
//!
//! Here is the documentation from the SQLite library regarding
//! the callback arguments:
//!
//! user_callback
//!   - An optional callback that is invoked once for each row of
//!     any query results produced by the SQL statements
//!
//! callback_obj
//!   - First argument to 'user_callback'. It is the 'this' pointer
//!     of the object that implements the callback function.
//!
//! For example:
//!
//!    struct MyCallbackHandler {
//!        int callback(int argc, char ** argv, char ** col_names) {
//!            std::cout << "I just found a SELECT match. Here are the values:\n";
//!            for (int col_idx = 0; col_idx < argc; ++col_idx) {
//!                std::cout << "\tColumn '" << col_names[col_idx]
//!                          << "' has value '" << argv[col_idx] << "'\n";
//!            }
//!            return SQLITE_OK;
//!        }
//!    };
//!
//!    MyCallbackHandler callback_obj;
//!
//!    //Assuming we have a sqlite3* connection "db_conn"
//!    LOCAL_eval_sql(db_conn,
//!                   "SELECT * FROM Customers WHERE Last='Smith'",
//!                   +[](void * callback_obj, int argc, char ** argv, char ** col_names) {
//!                       return static_cast<MyCallbackHandler*>(callback_obj)->callback(
//!                           argc, argv, col_names);
//!                   },
//!                   &callback_obj);
void LOCAL_eval_sql(sqlite3 * db_conn,
                    const std::string & command,
                    sqlite_select_callback user_callback = nullptr,
                    void * callback_obj = nullptr)
{
    char * err = 0;
    const int res = sqlite3_exec(db_conn,
                                 command.c_str(),
                                 user_callback,
                                 callback_obj,
                                 &err);
    if (res != SQLITE_OK) {
        switch (res) {
            case SQLITE_BUSY:
                throw SqlFileLockedException();
            case SQLITE_LOCKED:
                throw SqlTableLockedException();
            default:
                break;
        }

        std::string err_str;
        if (err) {
            //If our char* has an error message in it,
            //we will add it to the exception.
            err_str = err;
            sqlite3_free(err);
        } else {
            //Otherwise, just add the SQLite error code.
            //Users can look up the meaning of the code
            //in sqlite3.h
            std::ostringstream oss;
            oss << res << " (see sqlite3.h for error code definitions)";
            err_str = oss.str();
        }
        err_str += " (failed SQL command was '" + command + "')";
        throw DBException(err_str);
    }
}

//! Execute a SQL statement on an ObjectManager's connection proxy.
void eval_sql(const SQLiteConnProxy * db_proxy, const std::string & command)
{
    if (db_proxy) {
        db_proxy->eval(command);
    }
}

//! Execute a SELECT SQL statement on an open database connection
void eval_sql_select(const SQLiteConnProxy * db_proxy,
                     const std::string & command,
                     sqlite_select_callback select_callback,
                     void * callback_obj)
{
    if (db_proxy) {
        db_proxy->evalSelect(command, select_callback, callback_obj);
    }
}

//! Callback which gets invoked during SELECT queries that involve
//! floating point comparisons with a supplied tolerance.
void isWithinTolerance(sqlite3_context * context,
                       int, sqlite3_value ** argv)
{
    const double column_value = sqlite3_value_double(argv[0]);
    const double target_value = sqlite3_value_double(argv[1]);
    const double tolerance = sqlite3_value_double(argv[2]);

    if (utils::approximatelyEqual(column_value, target_value, tolerance)) {
        sqlite3_result_int(context, 1);
    } else {
        sqlite3_result_int(context, 0);
    }
}

//! Implementation class for the SQL database connection.
class SQLiteConnProxy::Impl
{
public:
    std::string openDbFile(const std::string & db_dir,
                           const std::string & db_file,
                           const bool create_file)
    {
        db_full_filename_ = resolveDbFilename_(db_dir, db_file);
        if (db_full_filename_.empty()) {
            if (create_file) {
                db_full_filename_ = db_dir + "/" + db_file;
            } else {
                throw DBException("Could not find database file: '")
                    << db_dir << "/" << db_file;
            }
        }

        const int db_open_flags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
        sqlite3 * sqlite_conn = nullptr;
        auto err_code = sqlite3_open_v2(db_full_filename_.c_str(),
                                        &sqlite_conn, db_open_flags, 0);

        //Inability to even open the database file may mean that
        //we don't have write permissions in this directory or
        //something like that. We should throw until we understand
        //better how else we can get bad file opens.
        if (err_code != SQLITE_OK) {
            throw DBException(
                "Unable to connect to the database file: ") << db_file;
        }

        //SQLite isn't the only implementation that SimDB supports.
        //The sqlite3_open_v2() function can still return a non-null
        //handle for a file that is NOT even SQLite. Let's try to make
        //a simple database query to verify the file is actually SQLite.
        if (!validateConnectionIsSQLite_(sqlite_conn)) {
            sqlite3_close(sqlite_conn);
            sqlite_conn = nullptr;
        }

        db_conn_ = sqlite_conn;
        if (db_conn_) {
            sqlite3_create_function(db_conn_, "withinTol", 3,
                                    SQLITE_UTF8, nullptr,
                                    &isWithinTolerance,
                                    nullptr, nullptr);
        }
        return (db_conn_ != nullptr ? db_full_filename_ : "");
    }

    void validateSchema(const Schema & schema) const
    {
        auto get_dims_str = [](const std::vector<size_t> & dims) {
            if (dims.empty()) {
                return std::string();
            }

            std::ostringstream oss;
            oss << "{";
            if (dims.size() == 1) {
                oss << dims[0];
            } else {
                for (size_t idx = 0; idx < dims.size() - 1; ++idx) {
                    oss << dims[idx] << ",";
                }
                oss << dims.back();
            }
            oss << "}";
            return oss.str();
        };

        std::ostringstream oss;

        for (const auto & table : schema) {
            for (const auto & column : table) {
                const std::vector<size_t> & dims = column->getDimensions();
                const size_t dims_product = std::accumulate(
                    dims.begin(), dims.end(), 1,
                    std::multiplies<size_t>());

                if (dims_product > 1) {
                    oss << "  [simdb] SQLite schema error: Table '"
                        << table.getName() << "', Column '" << column->getName()
                        << "' has data type " << column->getDataType() << get_dims_str(dims)
                        << ". Non-scalar ints/floats/strings are not supported by "
                        << "SQLite. Use a blob column data type instead.\n\n";
                } else if (dims_product == 0) {
                    oss << "  [simdb] SQLite schema error: Table '"
                        << table.getName() << "', Column '" << column->getName()
                        << "' has data type " << column->getDataType() << ", but "
                        << "its dimensions are " << get_dims_str(dims) << ". The "
                        << "dimensions vector should not have any zeros in it.\n\n";
                }
            }
        }

        std::string err = oss.str();
        if (!err.empty()) {
            err = "SQLite could not validate the schema. "
                 "These errors were produced:\n\n" + err;
            throw DBException(err);
        }
    }

    void realizeSchema(const Schema & schema,
                       const ObjectManager & obj_mgr)
    {
        obj_mgr.safeTransaction([&]() {
            for (const auto & table : schema) {
                //First create the table and its columns
                std::ostringstream oss;
                oss << "CREATE TABLE " << table.getName() << "(";

                //All tables have an auto-incrementing primary key
                oss << "Id INTEGER PRIMARY KEY AUTOINCREMENT";
                if (!table.hasColumns()) {
                    //A table without any columns would be somewhat
                    //odd, but that's what the user's schema specified.
                    //It is not invalid SQL, so we do not throw.
                    oss << ");";
                } else {
                    //Fill in the rest of the CREATE TABLE command:
                    //CREATE TABLE Id INTEGER PRIMARY KEY AUTOINCREMENT First TEXT, ...
                    //                                                  ---------------
                    oss << ", " << getColumnsSqlCommand(table) << ");";
                }

                //Create the table in the database
                evalSql(oss.str());

                //Now create any table indexes, for example:
                //    CREATE INDEX customer_fullname ON Customers (First,Last)
                //    CREATE INDEX county_population ON Counties (CountyName,Population)
                //    ...
                makeIndexesForTable_(table);
            }
        });
    }

    bool connectToExistingDatabase(const std::string & db_file)
    {
        return (!openDbFile(".", db_file, false).empty());
    }

    std::string getDatabaseFullFilename() const
    {
        return db_full_filename_;
    }

    bool isValid() const
    {
        return (db_conn_ != nullptr);
    }

    void getTableNames(
        std::unordered_set<std::string> & table_names)
    {
        //Helper object that will get called once for each
        //matching record in the SELECT statement.
        struct TableNames {
            TableNames(std::unordered_set<std::string> & names) :
                tbl_names(names)
            {}

            int addTableName(int argc, char ** argv, char **) {
                //We got another table name. Add it to the set.
                //*BUT* skip over any tables that are prefixed
                //with "sqlite_". Those are all reserved for the
                //library, and aren't really ours.
                assert(argc == 1);
                if (std::string(argv[0]).find("sqlite_") != 0) {
                    tbl_names.insert(argv[0]);
                }
               return 0;
            }
            std::unordered_set<std::string> & tbl_names;
        };

        TableNames select_callback_obj(table_names);

        //The TableNames object above has a reference to the table_names
        //output argument. Running the following SELECT statement will
        //populate this variable.
        evalSqlSelect("SELECT name FROM sqlite_master WHERE type='table'",
                      +[](void * callback_obj, int argc, char ** argv, char ** col_names) {
                          return static_cast<TableNames*>(callback_obj)->
                              addTableName(argc, argv, col_names);
                      },
                      &select_callback_obj);
    }

    void evalSql(const std::string & command) const
    {
        eval_(command, nullptr, nullptr);
    }

    void evalSqlSelect(const std::string & command,
                       int (*callback)(void *, int, char **, char **),
                       void * callback_obj) const
    {
        eval_(command, callback, callback_obj);
    }

    void prepareStatement(const std::string & command,
                          sqlite3_stmt ** statement) const
    {
        if (db_conn_ == nullptr) {
            return;
        }
        if (statement == nullptr) {
            return;
        }
        if (sqlite3_prepare_v2(db_conn_, command.c_str(), -1, statement, 0)) {
            throw DBException("Malformed SQL command: '") << command << "'";
        }
    }

    void openBlob(const std::string & table_name,
                  const std::string & column_name,
                  const int row_id,
                  sqlite3_blob ** blob) const
    {
        if (db_conn_ == nullptr) {
            return;
        }
        if (blob == nullptr) {
            return;
        }
        if (sqlite3_blob_open(db_conn_, "main", table_name.c_str(),
                              column_name.c_str(), row_id, 1, blob))
        {
            throw DBException("Error encountered while opening database blob. Occurred in ")
                << "table '" << table_name << "', column '" << column_name << "'.";
        }
    }

    int getLastActionNumRecordChanges() const
    {
        if (db_conn_ == nullptr) {
            return 0;
        }
        return sqlite3_changes(db_conn_);
    }

    DatabaseID createObject(const std::string & table_name,
                            const ColumnValues & values)
    {
        if (values.empty()) {
            evalSql("INSERT INTO " + table_name + " DEFAULT VALUES");
        } else {
            const std::string command =
                prepareSqlInsertStatement_(table_name, values);
            sqlite3_stmt * prepared_stmt = nullptr;
            prepareStatement(command, &prepared_stmt);
            assert(prepared_stmt != nullptr);

            //Execute the prepared statement
            LOCAL_finalizeInsertOrUpdateStatement(prepared_stmt, values);
        }
        return getLastInsertRowId_();
    }

    ~Impl()
    {
        if (db_conn_) {
            sqlite3_close(db_conn_);
        }
    }

private:
    //See if there is an existing file by the name <dir/file>
    //and return it. If not, return just <file> if it exists.
    //Return "" if no such file could be found.
    std::string resolveDbFilename_(
        const std::string & db_dir,
        const std::string & db_file) const
    {
        std::ifstream fin(db_dir + "/" + db_file);
        if (fin) {
            return db_dir + "/" + db_file;
        }

        fin.open(db_file);
        if (fin) {
            return db_file;
        }
        return "";
    }

    //For the CREATE INDEX statements, this helper makes a comma-
    //separated string of Column names like "First,Last"
    std::string makePropertyIndexesStr_(const Column & column)
    {
        std::ostringstream oss;
        for (const auto & indexed_property : column.getIndexedProperties()) {
            oss << indexed_property->getName() << ",";
        }
        std::string indexes_str = oss.str();
        if (indexes_str.back() == ',') {
            indexes_str.pop_back();
        }
        return indexes_str;
    }

    //Execute index creation statements like:
    //
    //    "CREATE INDEX Customers_Last ON Customers(Last)"
    //        ^^ indexes Customers table by Last column only
    //
    //    "CREATE INDEX Customers_Last ON Customers(First,Last)"
    //        ^^ multi-column index on the Customers table by First+Last columns
    //
    void makeIndexesForColumnInTable_(const Table & table,
                                      const Column & column)
    {
        std::ostringstream oss;
        oss << " CREATE INDEX " << table.getName() << "_" << column.getName()
            << " ON " << table.getName()
            << " (" << makePropertyIndexesStr_(column) << ")";
        evalSql(oss.str());
    }

    //Create indexes for a given Table, depending on how the
    //user set up the Column indexes (indexed by itself, vs.
    //indexed together with other columns)
    void makeIndexesForTable_(const Table & table)
    {
        if (!table.hasColumns()) {
            return;
        }

        for (const auto & column : table) {
            if (column->isIndexed()) {
                makeIndexesForColumnInTable_(table, *column);
            }
        }
    }

    //Attempt to run an SQL command against our open connection.
    //A file may have been given to us that was actually a different
    //database format, such as HDF5.
    bool validateConnectionIsSQLite_(sqlite3 * db_conn) const
    {
        struct FindHelper {
            bool did_find_any = false;
        };

        FindHelper finder;
        const std::string command =
            "SELECT name FROM sqlite_master WHERE type='table'";

        try {
            LOCAL_eval_sql(db_conn, command,
                          +[](void * callback_obj, int, char **, char **) {
                            static_cast<FindHelper*>(callback_obj)->did_find_any = true;
                            return SQLITE_OK;
                          },
                          &finder);
            return true;
        } catch (...) {
        }
        return false;
    }

    //All SQL commands (both reads and writes) end up here. The only
    //difference between a read and a write is if the two callback
    //inputs are null or not.
    void eval_(const std::string & command,
               int (*callback)(void *, int, char **, char **),
               void * callback_obj) const
    {
        //This proxy is intended to be used for safety checks
        //to ensure no disallowed statements are executed against
        //the database, such as "DROP TABLE Timeseries".
        //
        //Statement verification should go here as needed before
        //calling the LOCAL_eval_sql function.
        LOCAL_eval_sql(db_conn_, command, callback, callback_obj);
    }

    //Put together an INSERT statement for this table's
    //current column values.
    std::string prepareSqlInsertStatement_(
        const std::string & table_name,
        const ColumnValues & col_values) const
    {
        //Build the prepared SQL statement. This will put
        //placeholders ("?") for all the column values,
        //which we'll bind to shortly.
        //
        //The resulting SQL command looks something like this:
        //
        //   INSERT INTO Customers values (?,?,?)
        //
        std::string stmt;
        std::ostringstream oss;
        oss << "INSERT INTO " << table_name << " (";
        if (col_values.size() == 1) {
            oss << col_values[0].getColumnName() << ")";
        } else {
            for (size_t idx = 0; idx < col_values.size() - 1; ++idx) {
                oss << col_values[idx].getColumnName() << ",";
            }
            oss << col_values.back().getColumnName() << ")";
        }

        oss << " values (";
        if (col_values.size() == 1) {
            oss << "?";
        } else {
            for (size_t idx = 0; idx < col_values.size() - 1; ++idx) {
                oss << "?,";
            }
            oss << "?";
        }
        oss << ")";
        stmt = oss.str();
        return stmt;
    }

    //Return the database ID of the last record INSERT
    int getLastInsertRowId_() const
    {
        if (db_conn_ == nullptr) {
            return 0;
        }
        return sqlite3_last_insert_rowid(db_conn_);
    }

    //Physical database connection
    sqlite3 * db_conn_ = nullptr;

    //Filename of the database in use
    std::string db_full_filename_;
};

SQLiteConnProxy::SQLiteConnProxy() : impl_(new SQLiteConnProxy::Impl)
{
}

void SQLiteConnProxy::validateSchema(const Schema & schema) const
{
    impl_->validateSchema(schema);
}

void SQLiteConnProxy::realizeSchema(
    const Schema & schema,
    const ObjectManager & obj_mgr)
{
    impl_->realizeSchema(schema, obj_mgr);
}

bool SQLiteConnProxy::connectToExistingDatabase(const std::string & db_file)
{
    return impl_->connectToExistingDatabase(db_file);
}

std::string SQLiteConnProxy::getDatabaseFullFilename() const
{
    return impl_->getDatabaseFullFilename();
}

bool SQLiteConnProxy::isValid() const
{
    return impl_->isValid();
}

void SQLiteConnProxy::getTableNames(
    std::unordered_set<std::string> & table_names)
{
    impl_->getTableNames(table_names);
}

void SQLiteConnProxy::beginAtomicTransaction() const
{
    impl_->evalSql("BEGIN TRANSACTION");
}

void SQLiteConnProxy::commitAtomicTransaction() const
{
    impl_->evalSql("COMMIT TRANSACTION");
}

void stringifyColumnValue(const ColumnValueBase & col,
                          std::ostream & os)
{
    using dt = ColumnDataType;

    auto print_char_if_has_set_constraint = [&](const char ch) {
        if (!col.hasConstraint()) {
            return;
        }

        switch (col.getConstraint()) {
            case constraints::in_set:
            case constraints::not_in_set: {
                os << ch;
                break;
            }
            default:
                break;
        }
    };

    print_char_if_has_set_constraint('(');

    auto print_col_value_at_idx = [&](const size_t idx) {
        switch (col.getDataType()) {
            case dt::char_t: {
                os << stringify(col.getAs<char>(idx));
                break;
            }
            case dt::int8_t: {
                os << stringify(col.getAs<int8_t>(idx));
                break;
            }
            case dt::uint8_t: {
                os << stringify(col.getAs<uint8_t>(idx));
                break;
            }
            case dt::int16_t: {
                os << stringify(col.getAs<int16_t>(idx));
                break;
            }
            case dt::uint16_t: {
                os << stringify(col.getAs<uint16_t>(idx));
                break;
            }
            case dt::int32_t: {
                os << stringify(col.getAs<int32_t>(idx));
                break;
            }
            case dt::uint32_t: {
                os << stringify(col.getAs<uint32_t>(idx));
                break;
            }
            case dt::int64_t: {
                os << stringify(col.getAs<int64_t>(idx));
                break;
            }
            case dt::uint64_t: {
                os << stringify(col.getAs<uint64_t>(idx));
                break;
            }
            case dt::float_t: {
                os << stringify(col.getAs<float>(idx));
                break;
            }
            case dt::double_t: {
                os << stringify(col.getAs<double>(idx));
                break;
            }
            case dt::string_t: {
                os << stringify(col.getAs<std::string>(idx));
                break;
            }
            case dt::fkey_t: {
                os << stringify(col.getAs<DatabaseID>(idx));
                break;
            }
            default:
                throw DBException("ColumnValueBase cannot be stringified");
        }
    };

    if (col.getNumValues() == 1) {
        print_col_value_at_idx(0);
    } else {
        for (size_t idx = 0; idx < col.getNumValues() - 1; ++idx) {
            print_col_value_at_idx(idx);
            os << ",";
        }
        print_col_value_at_idx(col.getNumValues() - 1);
    }

    print_char_if_has_set_constraint(')');
}

//! Turn a ColumnValueBase object's value into a clause
//! that looks something like this:
//!
//!     WHERE LastName='Smith'
//!
//! This is used when building constrained UPDATE and
//! DELETE statements.
std::string createWhereClause(const ColumnValueBase & col)
{
    std::ostringstream oss;
    oss << col.getColumnName() << col.getConstraint();
    stringifyColumnValue(col, oss);
    return oss.str();
}

void SQLiteConnProxy::performDeletion(
    const std::string & table_name,
    const ColumnValues & where_clauses) const
{
    std::ostringstream oss;
    oss << "DELETE FROM " << table_name;
    if (!where_clauses.empty()) {
        oss << " WHERE ";
        if (where_clauses.size() == 1) {
            oss << createWhereClause(where_clauses[0]);
        } else {
            for (size_t idx = 0; idx < where_clauses.size() - 1; ++idx) {
                oss << createWhereClause(where_clauses[idx]) << " AND ";
            }
            oss << createWhereClause(where_clauses.back());
        }
    }

    const std::string command = oss.str();
    impl_->evalSql(command);
}

size_t SQLiteConnProxy::performUpdate(
    const std::string & table_name,
    const ColumnValues & col_values,
    const ColumnValues & where_clauses) const
{
    //Build the prepared SQL statement. This will put
    //placeholders ("?") for all the column values,
    //which we'll bind to shortly.
    //
    //The resulting SQL command looks something like this:
    //
    //   UPDATE Customers SET AccountActive=?
    //   WHERE Name='Smith'
    std::ostringstream oss;
    oss << "UPDATE " << table_name << " SET ";
    if (col_values.size() == 1) {
        oss << col_values[0].getColumnName() << "=?";
    } else {
        for (size_t idx = 0; idx < col_values.size() - 1; ++idx) {
            oss << col_values[idx].getColumnName() << "=?,";
        }
        oss << col_values.back().getColumnName() << "=?";
    }

    std::string command;
    if (where_clauses.empty()) {
        command = oss.str();
    } else {
        oss << " WHERE ";
        if (where_clauses.size() == 1) {
            oss << createWhereClause(where_clauses[0]);
        } else {
            for (size_t idx = 0; idx < where_clauses.size() - 1; ++idx) {
                oss << createWhereClause(where_clauses[idx]) << " AND ";
            }
            oss << createWhereClause(where_clauses.back());
        }
        command = oss.str();
    }

    //Execute the prepared statement
    sqlite3_stmt * prepared_stmt = nullptr;
    impl_->prepareStatement(command, &prepared_stmt);
    assert(prepared_stmt != nullptr);

    LOCAL_finalizeInsertOrUpdateStatement(prepared_stmt, col_values);
    return impl_->getLastActionNumRecordChanges();
}

AnySizeObjectFactory SQLiteConnProxy::getObjectFactoryForTable(
    const std::string &) const
{
    return +[](DbConnProxy * db_proxy,
               const std::string & table_name,
               const ColumnValues & obj_values)
    {
        return static_cast<SQLiteConnProxy*>(db_proxy)->
            createObject(table_name, obj_values);
    };
}

DatabaseID SQLiteConnProxy::createObject(
    const std::string & table_name,
    const ColumnValues & values)
{
    return impl_->createObject(table_name, values);
}

std::string SQLiteConnProxy::openDbFile_(
    const std::string & db_dir,
    const std::string & db_file,
    const bool create_file)
{
    return impl_->openDbFile(db_dir, db_file, create_file);
}

void SQLiteConnProxy::eval(const std::string & command) const
{
    impl_->evalSql(command);
}

void SQLiteConnProxy::evalSelect(
    const std::string & command,
    int (*callback)(void *, int, char **, char **),
    void * callback_obj) const
{
    impl_->evalSqlSelect(command, callback, callback_obj);
}

void SQLiteConnProxy::prepareStatement_(
    const std::string & command,
    void ** statement) const
{
    sqlite3_stmt * stmt = nullptr;
    impl_->prepareStatement(command, &stmt);
    if (stmt) {
        *statement = stmt;
    }
}

SQLiteConnProxy::~SQLiteConnProxy()
{
}

} // namespace simdb
