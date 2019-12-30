// <HDF5ConnProxy> -*- C++ -*-

#ifndef __SIMDB_HDF5_CONN_PROXY_H__
#define __SIMDB_HDF5_CONN_PROXY_H__

#include "simdb/DbConnProxy.hpp"

#include <memory>

namespace simdb {

/*!
 * \class HDF5ConnProxy
 *
 * \brief Proxy object which forwards database commands
 * to the physical database APIs. ObjectManager does not
 * provide direct access to the database handle; going
 * through this proxy prevents misuse of database commands,
 * and throws if it sees a command that is disallowed.
 */
class HDF5ConnProxy : public DbConnProxy
{
public:
    //! \brief HDF5 database files have "*.h5" file extension
    //!
    //! \return Hard-coded ".h5" extension
    const char * getDatabaseFileExtension() const override {
        return ".h5";
    }

    //! \brief This validate method gets called when a schema is
    //! given to an ObjectManager to use with an HDF5 connection.
    //!
    //! \param schema Schema object to validate
    void validateSchema(const Schema & schema) const override;

    //! \brief Turn a Schema object into an actual database
    //! connection
    //!
    //! \param schema Schema object to realize
    //!
    //! \param obj_mgr SimDB ObjectManager that this DbConnProxy
    //! object belongs to
    void realizeSchema(
        const Schema & schema,
        const ObjectManager & obj_mgr) override;

    //! \brief Try to open a connection to an existing database file
    //!
    //! \param db_file Name of the HDF5 file
    //!
    //! \return Return true on successful connection, false otherwise
    bool connectToExistingDatabase(const std::string & db_file) override;

    //! \brief Get the full database filename being used. This
    //! includes the database path, stem, and extension. Returns
    //! empty if no connection is open.
    //!
    //! \return Full filename, such as "/tmp/abcd-1234.h5"
    std::string getDatabaseFullFilename() const override;

    //! \return Returns true if this database is open and ready to
    //! take read and write requests, false otherwise
    bool isValid() const override;

    //! \brief HDF5 does not support or need atomic transactions
    //!
    //! \return False, hard-coded
    bool supportsAtomicTransactions() const override {
        return false;
    }

    //! \brief Not implemented. Do not call - this will throw.
    void performDeletion(
        const std::string &,
        const ColumnValues &) const override
    {
        throw DBException("Not implemented");
    }

    //! \brief Not implemented. Do not call - this will throw.
    size_t performUpdate(
        const std::string &,
        const ColumnValues &,
        const ColumnValues &) const override
    {
        throw DBException("Not implemented");
    }

    //! \brief For tables that only have fixed-size columns,
    //! we can read the raw bytes for the requested record
    //! properties directly. The HDF5 library will give us the
    //! byte offset to the column (prop_name) in question,
    //! and the DatabaseID is just a linear offset into the
    //! table itself.
    //!
    //!    struct MyFoo {
    //!         int16_t x;
    //!         int16_t y;
    //!    };
    //!
    //! "Give me prop_name 'y' for element #14"
    //!
    //!    --> "Go to the element offset 13, and offset
    //!         further another 2 bytes"
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
    //! \param num_bytes Number of bytes of preallocated memory
    //! the dest_ptr is pointing to
    //!
    //! \return Number of bytes read
    size_t readRawBytes(
        const std::string & table_name,
        const std::string & prop_name,
        const DatabaseID db_id,
        void * dest_ptr,
        const size_t num_bytes) const override;

    //! \brief Provide an object/record factory for the given table
    //!
    //! \param table_name Name of the table we want to create objects
    //! for (table rows)
    //!
    //! \return Factory suitable for creating records for the given
    //! table
    AnySizeObjectFactory getObjectFactoryForTable(
        const std::string & table_name) const override;

    //! \brief Provide an object/record factory for the given table.
    //! This returns a factory that can only work with tables of
    //! fixed-size column data types (POD's).
    //!
    //! \note Currently, HDF5 SimDB only supports scalar/matrix POD's.
    //! The AnySizeObjectFactory objects returned above and the
    //! FixedSizeObjectFactory objects returned here do the same
    //! thing at the moment. Though working with FixedSizeObjectFactory
    //! should be preferred for fixed-size schema tables since it is
    //! considerably faster than the AnySizeObjectFactory equivalent.
    //!
    //! \param table_name Name of the table we want to create objects
    //! for (table rows)
    //!
    //! \return Factory suitable for creating fixed-size records for
    //! the given table
    FixedSizeObjectFactory getFixedSizeObjectFactoryForTable(
        const std::string & table_name) const override;

    //! \brief Respond when our AnySizeObjectFactory is invoked.
    //! Create a new object with the provided column values.
    //!
    //! \param table_name Name of the table we want to create an
    //! object for (table row)
    //!
    //! \param values Vector of objects which are holding the
    //! column names and their values for the new record
    //!
    //! \return Database ID of the newly created record
    DatabaseID createObject(
        const std::string & table_name,
        const ColumnValues & values);

    //! \brief Respond when our FixedSizeObjectFactory is invoked.
    //! Create a new object with the provided raw bytes. Since the
    //! table is fixed in its column(s) width, the raw bytes array
    //! passed in contains a fixed, known number of bytes that the
    //! HDF5 library can read from. This byte array has all of the
    //! new record's column value(s) all packed together.
    //!
    //! \param table_name Name of the table we want to create an
    //! object for (table row)
    //!
    //! \param raw_bytes_ptr Flat byte array containing all of the
    //! columns' values for the new record
    //!
    //! \return Database ID of the newly created record
    DatabaseID createFixedSizeObject(
        const std::string & table_name,
        const void * raw_bytes_ptr,
        const size_t num_raw_bytes);

    HDF5ConnProxy();
    ~HDF5ConnProxy() override;

private:
    //! First-time database file open.
    std::string openDbFile_(
        const std::string & db_dir,
        const std::string & db_file,
        const bool open_file) override;

    //! Not implemented.
    void prepareStatement_(const std::string &,
                           void **) const override
    {
        throw DBException("Not implemented");
    }

    //! Until HDF5ConnProxy supports ObjectQuery, calls to
    //! ObjectManager::findObject() will reroute to this
    //! method.
    bool hasObjectImpl_(
        const std::string & table_name,
        const DatabaseID db_id) const override;

    //! Rest of the implementation is hidden from view. This
    //! is to reduce unnecessary includes and to hide the
    //! specific database tech being used.
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace simdb

#endif
