// <DbConnProxy> -*- C++ -*-

#ifndef __SIMDB_CONN_PROXY_H__
#define __SIMDB_CONN_PROXY_H__

#include "simdb/schema/ColumnValue.hpp"
#include "simdb/ObjectFactory.hpp"
#include "simdb_fwd.hpp"

#include <memory>
#include <string>

namespace simdb {

/*!
 * \brief Interface class for database connections. Subclasses
 * are responsible for turning a Schema object into a fully
 * realized database with an open connection (SQL, HDF5, etc.)
 * and for issuing commands (INSERT, SELECT, ...) against that
 * database.
 */
class DbConnProxy
{
public:
    virtual ~DbConnProxy() = default;

    //! Return a file extension to be used when creating database
    //! files using auto-generated file names.
    virtual const char * getDatabaseFileExtension() const = 0;

    //! Inspect and validate this schema before it gets instantiated
    //! by an ObjectManager.
    virtual void validateSchema(const Schema & schema) const = 0;

    //! Turn a Schema object into an actual database connection.
    virtual void realizeSchema(
        const Schema & schema,
        const ObjectManager & obj_mgr) = 0;

    //! By default, ObjectManager will assume that the Table's
    //! that were in the Schema object given to it at the start
    //! of simulation are all in the physical database. Optionally
    //! override this method to change how table names are figured
    //! out.
    virtual void getTableNames(std::unordered_set<std::string> & table_names) {
        (void) table_names;
    }

    //! Try to open a connection to an existing database file.
    virtual bool connectToExistingDatabase(const std::string & db_file) = 0;

    //! Get the full database filename being used. This includes
    //! the database path, stem, and extension. Returns empty if
    //! no connection is open.
    virtual std::string getDatabaseFullFilename() const = 0;

    //! Is this connection still alive and well?
    virtual bool isValid() const = 0;

    //! Tell ObjectManager whether or not your database should
    //! have larger writes/reads enclosed inside calls to the
    //! beginAtomicTransaction() / commitAtomicTransaction()
    //! methods.
    virtual bool supportsAtomicTransactions() const = 0;

    //! If supportsAtomicTransactions() returns true, calls to
    //! this method will occur at the start of atomic writes/
    //! reads.
    virtual void beginAtomicTransaction() const {}

    //! If supportsAtomicTransactions() returns true, calls to
    //! this method will occur at the end of atomic writes/
    //! reads.
    virtual void commitAtomicTransaction() const {}

    //! When a call into the TableRef class is made to delete one
    //! or more records that match a certain set of constraints,
    //! this method will end up getting invoked. In SQL, this
    //! is equivalent to something like this:
    //!
    //!    DELETE FROM Accounts WHERE PendingDelete=1
    //!
    virtual void performDeletion(
        const std::string & table_name,
        const ColumnValues & where_clauses) const = 0;

    //! When a call into the TableRef class is made to update the
    //! values of one or more records that match a certain set of
    //! constraints, this method will end up getting invoked. In
    //! SQL, this is equivalent to something like this:
    //!
    //!    UPDATE Accounts SET PendingDelete=1
    //!    WHERE Balance=0 AND LastUseDays>365
    //!
    //! Returns the total number of updated records.
    virtual size_t performUpdate(
        const std::string & table_name,
        const ColumnValues & col_values,
        const ColumnValues & where_clauses) const = 0;

    //! \brief Some database implementations may have tables
    //! whose columns are all held in contiguous memory / on disk
    //! and those implementations may have a more performant
    //! way to read the requested data. This is also for sub-
    //! classes which do not support ObjectQuery / indexed
    //! queries.
    //!
    //! \param table_name Name of the table containing data
    //! we want to read
    //!
    //! \param prop_name Name of the specific property (column /
    //! field) in that table being read
    //!
    //! \param db_id Unique database ID for the requested
    //! record. Equivalent to rowid in SQL.
    //!
    //! \param dest_ptr Preallocated memory the raw bytes from
    //! the database should be written into
    //!
    //! \param num_bytes Number of bytes of preallocated
    //! memory the dest_ptr is pointing to
    //!
    //! \return Number of bytes read
    virtual size_t readRawBytes(
        const std::string & table_name,
        const std::string & prop_name,
        const DatabaseID db_id,
        void * dest_ptr,
        const size_t num_bytes) const
    {
        (void) table_name;
        (void) prop_name;
        (void) db_id;
        (void) dest_ptr;
        (void) num_bytes;
        throw DBException("Not implemented");
    }

    //! Provide an object/record factory for the given table.
    virtual AnySizeObjectFactory getObjectFactoryForTable(
        const std::string & table_name) const = 0;

    //! Tables which only contain fixed-sized columns may
    //! be able to implement a more performant object factory
    //! than the returned callback from getObjectFactoryForTable()
    //!
    //! The ObjectManager knows which tables only contain
    //! fixed-sized columns. For these tables, it will call
    //! this getFixedSizeObjectFactoryForTable() method *once*,
    //! and if this method was not overridden, ObjectManager
    //! will fall back on calling getObjectFactoryForTable()
    //! for all further calls to its getTable() method for
    //! the requested table.
    virtual FixedSizeObjectFactory getFixedSizeObjectFactoryForTable(
        const std::string & table_name) const
    {
        (void) table_name;
        return FixedSizeObjectFactory();
    }

private:
    //! This proxy is intended to be used directly with
    //! the core SimDB classes.
    friend class ObjectManager;
    friend class ObjectRef;
    friend class ObjectQuery;

    //! First-time database file open.
    virtual std::string openDbFile_(
        const std::string & db_dir,
        const std::string & db_file,
        const bool open_file) = 0;

    //! Create a prepared statement for the provided command.
    //! The specific pointer type of the output void** is tied
    //! to the database tech being used. This is intended to
    //! be implementation detail, so this method is accessible
    //! to friends only.
    virtual void prepareStatement_(
        const std::string & command,
        void ** statement) const = 0;

    //! Leveraging the ObjectQuery utility for fast lookups
    //! may not be an option for some SimDB implementations.
    //! This utility is only available for SimDB's SQLite
    //! implementation at the moment.
    //!
    //! Developer note: This method goes hand in hand with
    //! the prepareStatement_() method above. Both of these
    //! methods will ultimately be combined into a more
    //! implementation-agnostic pure virtual public method
    //! in the DbConnProxy class. Until at least one more
    //! non-SQL implementation finds a performant way to
    //! leverage single-constraint and multi-constraint
    //! indexing like SQLite provides out of the box, it
    //! is not very obvious what that public API should
    //! look like. The ObjectQuery internals are hard
    //! coded to work with SQLite only today.
    virtual bool supportsObjectQuery_() const {
        return false;
    }

    //! For DbConnProxy subclasses that do not support
    //! ObjectQuery, the ObjectManager::findObject()
    //! method calls will reroute to this method.
    //! The virtual hasObjectImpl_() method must be
    //! overridden, even if that subclass override
    //! has to return FALSE.
    //!
    //! The reason why hasObject_() and hasObjectImpl_()
    //! are split up into two methods like this is that
    //! ObjectManager needs to be sure that the subclass
    //! actually attempted to find the requested record.
    //! If hasObject_() was a virtual method that returned
    //! false by default, ObjectManager would not know
    //! if the attempt was even made (since FALSE is a
    //! valid answer to the question, "do you have a
    //! record with this ID in this table?")
    bool hasObject_(
        const std::string & table_name,
        const DatabaseID db_id) const
    {
        assert(!supportsObjectQuery_());
        return hasObjectImpl_(table_name, db_id);
    }

    //! For DbConnProxy subclasses that do not support
    //! ObjectQuery, this method must be overridden to
    //! allow ObjectManager::findObject() to find any
    //! database object/record by its table name and
    //! its database ID.
    virtual bool hasObjectImpl_(
        const std::string & table_name,
        const DatabaseID db_id) const
    {
        (void) table_name;
        (void) db_id;
        throw DBException("Not implemented");
    }
};

} // namespace simdb

#endif
