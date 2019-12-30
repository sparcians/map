// <SQLiteConnProxy> -*- C++ -*-

#ifndef __SIMDB_SQLITE_CONN_PROXY_H__
#define __SIMDB_SQLITE_CONN_PROXY_H__

#include "simdb/DbConnProxy.hpp"

#include <memory>

namespace simdb {

/*!
 * \brief Proxy object which forwards database commands
 * to the physical database APIs. ObjectManager does not
 * provide direct access to the database handle; going
 * through this proxy prevents misuse of database commands,
 * and throws if it sees a command that is disallowed.
 */
class SQLiteConnProxy : public DbConnProxy
{
public:
    //! SQLite database files have "*.db" file extension.
    const char * getDatabaseFileExtension() const override {
        return ".db";
    }

    //! SQLite has some schema restrictions related to
    //! column dimensionality. This validate method gets
    //! called when a schema is given to an ObjectManager
    //! to use with a SQLite connection.
    void validateSchema(const Schema & schema) const override;

    //! Turn a Schema object into an actual database connection.
    void realizeSchema(
        const Schema & schema,
        const ObjectManager & obj_mgr) override;

    //! Try to open a connection to an existing database file
    bool connectToExistingDatabase(const std::string & db_file) override;

    //! Get the full database filename being used. This includes
    //! the database path, stem, and extension. Returns empty if
    //! no connected is open.
    std::string getDatabaseFullFilename() const override;

    //! Execute the provided statement against the database
    //! connection. This will validate the command, and throw
    //! if this command is disallowed.
    void eval(const std::string & command) const;

    //! Execute a SELECT statement against the database
    //! connection. The provided callback will be invoked
    //! once for each record found. Example:
    //!
    //!    struct CallbackHandler {
    //!        int handle(int argc, char ** argv, char ** col_names) {
    //!            ...
    //!            return 0;
    //!        }
    //!    };
    //!
    //!    CallbackHandler handler;
    //!
    //!    db_proxy->evalSelect(
    //!        "SELECT First,Last FROM Customers",
    //!        +[](void * handler, int argc, char ** argv, char ** col_names) {
    //!            return (CallbackHandler*)(handler)->handle(argc, argv, col_names);
    //!        },
    //!        &handler);
    //!
    //! See TransactionUtils.h for more details about the callback
    //! arguments that are passed to your SELECT handler.
    void evalSelect(
        const std::string & command,
        int (*callback)(void *, int, char **, char **),
        void * callback_obj) const;

    //! Check for nullptr database handle.
    bool isValid() const override;

    //! Override the default way ObjectManager gets the table
    //! names. It defaults to whatever Table objects were in the
    //! Schema object it was originally given, but we may want to
    //! override it so we can make some tables private/unqueryable
    //! for internal use only.
    void getTableNames(
        std::unordered_set<std::string> & table_names) override;

    //! SQLite gets a performance boost by grouping statements
    //! in BEGIN TRANSACTION / COMMIT TRANSACTION pairs.
    bool supportsAtomicTransactions() const override {
        return true;
    }

    //! Issue BEGIN TRANSACTION
    void beginAtomicTransaction() const override;

    //! Issue COMMIT TRANSACTION
    void commitAtomicTransaction() const override;

    //! Take the object constraints we are given, and delete
    //! any records from the given table which match those
    //! constraints.
    void performDeletion(
        const std::string & table_name,
        const ColumnValues & where_clauses) const override;

    //! Take the object constraints we are given, and update
    //! all records from the given table which match those
    //! constraints.
    //!
    //! Returns the total number of updated records.
    size_t performUpdate(
        const std::string & table_name,
        const ColumnValues & col_values,
        const ColumnValues & where_clauses) const override;

    //! Provide an object/record factory for the given table.
    AnySizeObjectFactory getObjectFactoryForTable(
        const std::string & table_name) const override;

    //! Respond when our ObjectFactory is invoked. Create a new
    //! object with the provided arguments.
    DatabaseID createObject(
        const std::string & table_name,
        const ColumnValues & values);

    SQLiteConnProxy();
    ~SQLiteConnProxy() override;

private:
    //! This proxy is intended to be used directly with
    //! the core SimDB classes.
    friend class ObjectManager;
    friend class ObjectRef;
    friend class ObjectQuery;
    friend class TableRef;

    //! First-time database file open.
    std::string openDbFile_(
        const std::string & db_dir,
        const std::string & db_file,
        const bool open_file) override;

    //! Create a prepared statement for the provided command.
    //! The specific pointer type of the output void** is tied
    //! to the database tech being used. This is intended to
    //! be implementation detail, so this method is accessible
    //! to friends only.
    void prepareStatement_(
        const std::string & command,
        void ** statement) const override;

    //! Lookup optimization via ObjectQuery is enabled only
    //! for SQLite SimDB today. This method will be removed
    //! in the future.
    bool supportsObjectQuery_() const override final {
        return true;
    }

    //! Rest of the implementation is hidden from view.
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace simdb

#endif
