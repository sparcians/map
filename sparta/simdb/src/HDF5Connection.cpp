// <HDF5Connection> -*- C++ -*-

#include "simdb/ObjectManager.hpp"
#include "simdb/schema/Schema.hpp"
#include "simdb/schema/ColumnMetaStructs.hpp"
#include "simdb/Errors.hpp"

//HDF5-specific headers
#include "simdb/impl/hdf5/HDF5ConnProxy.hpp"
#include "simdb/impl/hdf5/DataTypeUtils.hpp"
#include "simdb/impl/hdf5/Resources.hpp"
#include "hdf5.h"

//Standard headers
#include <cstring>
#include <iostream>
#include <map>
#include <numeric>
#include <unordered_map>

#define HDF5_MAX_NAME 1024

namespace simdb {

/*!
 * \class HDF5DatasetIds
 *
 * \brief Class which holds onto HDF5 identifiers,
 * and closes them / their associated resource from
 * the destructor. Basically a smart pointer for
 * HDF5 ids.
 */
class HDF5DatasetIds
{
public:
    //! \brief Construct with HDF5 dataset and data
    //! type IDs
    //!
    //! \param dset Dataset ID. H5Dclose() will be
    //! called on this identifier on destruction.
    //!
    //! \param dtype Data type ID. H5Tclose() will
    //! be called on this identifier on destruction.
    HDF5DatasetIds(const hid_t dset,
                   const hid_t dtype) :
        dset_(dset),
        dtype_(dtype)
    {}

    HDF5DatasetIds(const HDF5DatasetIds &) = delete;
    HDF5DatasetIds & operator=(const HDF5DatasetIds &) = delete;

    hid_t getDatasetId() const {
        return dset_;
    }

    hid_t getDataTypeId() const {
        return dtype_;
    }

private:
    H5DResource dset_;
    H5TResource dtype_;
};

/*!
 * \class HDF5FileScanner
 *
 * \brief Given a handle to an HDF5 file, scan the file
 * contents for all tables (datasets) and columns (dataset
 * elements/fields). This is used to recreate the original
 * HDF5 schema when connectToExistingDatabase() is called
 * on an HDF5 file outside of a running simulation.
 */
class HDF5FileScanner
{
public:
    //! \brief Reconstruct HDF5 dataset / data type objects
    //! from an existing HDF5 database file
    //!
    //! \param hfile File ID obtained via H5Fcreate() or
    //! H5Fopen()
    //!
    //! \return Reconstructed schema object containing the
    //! same tables (datasets) and columns (member fields) as
    //! found in the file.
    Schema scanSchema(const hid_t hfile)
    {
        Schema schema;
        H5GResource grp = H5Gopen(hfile, "/", H5P_DEFAULT);
        scanGroup_(grp, schema);
        return schema;
    }

    //! \brief Get a mapping of the table names found in the
    //! file, to their associated dataset and data type IDs.
    //!
    //! \return Map of table names to an HDF5DatasetIDs object,
    //! which can be used to get the dataset (table) ID and
    //! the compound/struct data type ID of that table.
    const std::map<std::string, std::shared_ptr<HDF5DatasetIds>> & getDatasetIds()
    {
        return dataset_ids_;
    }

private:
    //! \brief Parse dataset information about an HDF5 group
    void scanGroup_(const hid_t gid, Schema & schema)
    {
        char memb_name[HDF5_MAX_NAME];

        hsize_t nobj;
        H5Gget_num_objs(gid, &nobj);
        for (size_t idx = 0; idx < nobj; ++idx) {
            auto len = H5Gget_objname_by_idx(
                gid, idx, memb_name, HDF5_MAX_NAME);

            const std::string memb_name_string(memb_name, len);

            auto otype = H5Gget_objtype_by_idx(gid, idx);
            if (otype == H5G_DATASET) {
                auto & table = schema.addTable(memb_name_string);
                auto dsid = H5Dopen(gid, memb_name, H5P_DEFAULT);
                scanDataset_(dsid, table);
            }
        }
    }

    //! \brief Parse data type information about an HDF5 dataset
    void scanDataset_(const hid_t dsid, Table & table)
    {
        const hid_t tid = H5Dget_type(dsid);
        const H5T_class_t t_class = H5Tget_class(tid);
        if (t_class == H5T_COMPOUND) {
            scanCompoundDatatype_(tid, table);
            dataset_ids_[table.getName()] = std::make_shared<HDF5DatasetIds>(dsid, tid);
        } else {
            H5Dclose(dsid);
            H5Tclose(tid);
        }
    }

    //! \brief Parse field information about an HDF5 compound
    //! data type
    void scanCompoundDatatype_(const hid_t tid, Table & table)
    {
        const int nfields = H5Tget_nmembers(tid);
        for (int idx = 0; idx < nfields; ++idx) {
            const std::string memb_name(reinterpret_cast<const char*>(
                H5Tget_member_name(tid, idx)));

            H5TResource ftype = H5Tget_member_type(tid, idx);
            table.addColumn(memb_name, getPODColumnDTypeFromHDF5(ftype));
        }
    }

    std::map<std::string, std::shared_ptr<HDF5DatasetIds>> dataset_ids_;
};

/*!
 * \class HDF5Dataset
 *
 * \brief Handle requests from the HDF5ConnProxy class
 * to create and populate HDF5 datasets from SimDB schemas
 * and record raw values.
 */
class HDF5Dataset
{
public:
    //! \brief Construct a dataset with the desired table name
    explicit HDF5Dataset(const std::string & table_name) :
        table_name_(table_name)
    {
        if (table_name_.empty()) {
            throw DBException("Empty table name given to HDF5 dataset");
        }
    }

    //! \brief There are TableRef APIs which instantiate
    //! records using non-contiguous record values paired
    //! up with their column names, as well as APIs which
    //! accept column values *only*. The latter is more
    //! performant, but you can only use it when your table /
    //! dataset has nothing but fixed-size POD's for its
    //! columns / fields.
    //!
    //! This method here lets us know if we can "interpret
    //! the table records as structs". If yes, we can ask
    //! our Column objects directly what their byte offset
    //! is. Non-contiguous records use Columns that do not
    //! have a known byte offset, since they do not have
    //! the same reference point like POD fields in a C
    //! struct do.
    void interpretAsStruct()
    {
        is_struct_table_ = true;
    }

    //! \brief As the schema is getting realized, this
    //! method gets called once for each table column.
    void addColumnToDataset(const Column & col)
    {
        columns_.emplace_back(col);
    }

    //! \brief Set up the variables needed in order to
    //! read or write an existing HDF5 file.
    void recreateDatasetFromFile(const std::shared_ptr<HDF5DatasetIds> & ids,
                                 const std::string & table_name)
    {
        if (ids == nullptr) {
            return;
        }
        dataset_ids_ = ids;
        dataset_name_ = table_name;

        constexpr hsize_t ndims = 2;
        static hsize_t memspace_dims[ndims] = {1, 1};
        memspace_ = H5Screate_simple(ndims, memspace_dims, nullptr);

        H5SResource space = H5Dget_space(dataset_ids_->getDatasetId());
        num_writes_ = H5Sget_select_npoints(space);
    }

    //! \brief Given an HDF5 file ID and table name, turn
    //! our schema metadata into a realized, ready-to-go
    //! HDF5 Dataset. We'll use the dataset later to write
    //! raw bytes into the HDF5 file through the C library.
    void createDatasetInFile(const hid_t hfile,
                             const std::string & table_name,
                             CompressionType compression)
    {
        static const hsize_t ndims = 1;
        hsize_t dims[ndims] = {0};
        static hsize_t maxdims[ndims] = {H5S_UNLIMITED};
        H5SResource filespace = H5Screate_simple(ndims, dims, maxdims);

        H5PResource plist = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_layout(plist, H5D_CHUNKED);

        if (compression != CompressionType::NONE &&
            !H5Zfilter_avail(H5Z_FILTER_DEFLATE))
        {
            std::cout << "  [simdb] HDF5 compression requested, "
                      << "but gzip is not available" << std::endl;
            compression = CompressionType::NONE;
        }

        //The HDF5 library lets you set the compression
        //level on a scale from 0-9:
        //
        //              0 -- 1 -- 2 -- 3 -- 4 -- 5 -- 6 -- 7 -- 8 -- 9
        //Compression: (none) ... (some) ........................ (max)
        //Speed:           (fast) .... (slower) ............... (slowest)
        switch (compression) {
            case CompressionType::NONE: {
                H5Pset_deflate(plist, 0);
                break;
            }
            case CompressionType::DEFAULT_COMPRESSION: {
                H5Pset_deflate(plist, 5);
                break;
            }
            case CompressionType::BEST_COMPRESSION_RATIO: {
                H5Pset_deflate(plist, 9);
                break;
            }
            case CompressionType::BEST_COMPRESSION_SPEED: {
                H5Pset_deflate(plist, 1);
                break;
            }
        }

        //TODO: It is not really possible to determine a one-size-
        //fits-all chunk size that is performant for every use case.
        //This should eventually be surfaced as a tunable parameter.
        hsize_t chunkdims[ndims] = {1000};
        H5Pset_chunk(plist, ndims, chunkdims);

        //Build the compound data type field by field. We first
        //need to calculate the total number of bytes in one of
        //these compound elements (structs). This is the data
        //size in bytes that will be written to the HDF5 file
        //during each call to the writeRawBytes() method.
        const size_t record_num_bytes = getOneRecordNumBytes_();
        auto dtid = H5Tcreate(H5T_COMPOUND, record_num_bytes);

        //The dtid variable is the data type ID for the compound
        //data type (struct, or one record/row in our table).
        addColumnsToCompoundDataType_(dtid);

        auto dset = H5Dcreate(
            hfile, table_name.c_str(), dtid, filespace,
            H5P_DEFAULT, plist, H5P_DEFAULT);

        if (dset < 0) {
            throw DBException("Unable to create dataset for HDF5 table: ")
                << table_name;
        }
        dataset_ids_ = std::make_shared<HDF5DatasetIds>(dset, dtid);
        dataset_name_ = table_name;

        static hsize_t memspace_dims[ndims] = {1};
        memspace_ = H5Screate_simple(ndims, memspace_dims, nullptr);
    }

    //! \brief This dataset object currently only supports fixed-
    //! size columns (POD's) so we are free to write raw memory
    //! into the HDF5 library directly. This method is called when
    //! the TableRef we are associated with gets a request to
    //! create a record with provided column values.
    //!
    //! \return The total number of records we have written into
    //! this specific dataset.
    size_t writeRawBytes(const void * raw_bytes_ptr,
                         const size_t num_raw_bytes)
    {
        const hid_t dset = dataset_ids_->getDatasetId();
        if (dset == 0) {
            throw DBException(
                "Method cannot be called. Dataset does not exist.");
        }

        hsize_t dims[2] = {num_writes_ + 1, 1};
        H5Dset_extent(dset, dims);

        H5SResource filespace = H5Dget_space(dset);
        hsize_t start[2] = {num_writes_, 0};
        hsize_t count[2] = {1, 1};
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET,
                            start, nullptr,
                            count, nullptr);

        const hid_t dtype = dataset_ids_->getDataTypeId();

        //The HDF5ConnProxy class currently only supports writing
        //fixed-size records (either a single fixed-size POD type,
        //or a struct whose fields are all fixed-size POD types).
        if (H5Tget_size(dtype) != num_raw_bytes) {
            auto err = DBException("Invalid call to write HDF5 data. Attempt to ")
                << "write " << num_raw_bytes << " bytes' worth of raw data into "
                << "an HDF5 dataset that is " << H5Tget_size(dtype) << " bytes "
                << "in size.";
            if (!dataset_name_.empty()) {
                err << " Occurred for dataset '" << dataset_name_ << "'.";
            }
            err << "\n";
            throw (err);
        }

        H5Dwrite(dset, dtype, memspace_, filespace, H5P_DEFAULT, raw_bytes_ptr);

        ++num_writes_;
        return num_writes_;
    }

    //! \brief Read raw data out of the HDF5 file belonging
    //! to this dataset
    //!
    //! \param prop_name Column / field name of the requested value
    //!
    //! \param db_id Unique database ID of the record in this dataset
    //! (table). Equivalent to SQL's rowid.
    //!
    //! \param dest_ptr Preallocated memory the raw bytes from the
    //! database should be written into
    //!
    //! \param num_bytes Number of bytes of preallocated memory
    //! the dest_ptr parameter is pointing to
    //!
    //! \return Number of bytes read
    size_t readRawBytes(
        const std::string & prop_name,
        const DatabaseID db_id,
        void * dest_ptr,
        const size_t num_bytes) const
    {
        if (db_id <= 0) {
            return 0;
        }

        const hid_t dset = dataset_ids_->getDatasetId();
        const hid_t dtype = dataset_ids_->getDataTypeId();

        const int field_idx = H5Tget_member_index(dtype, prop_name.c_str());
        if (field_idx < 0) {
            std::cout << "  [simdb] Property named '"
                      << prop_name << "' not found in HDF5 dataset '"
                      << table_name_ << "'" << std::endl;
            return 0;
        }

        H5TResource field_dtype = H5Tget_member_type(dtype, field_idx);
        if (H5Tget_size(field_dtype) != num_bytes) {
            std::cout << "  [simdb] Incorrect number of bytes "
                      << "requested from HDF5 dataset '"
                      << table_name_ << "'" << std::endl;
            return 0;
        }

        H5SResource filespace = H5Dget_space(dset);
        H5Sselect_all(filespace);

        const hsize_t start[2] = {static_cast<hsize_t>(db_id - 1), 0};
        const hsize_t count[2] = {1, 1};
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET,
                            start, nullptr,
                            count, nullptr);

        const size_t compound_size = H5Tget_size(dtype);
        if (compound_size == 0) {
            return 0;
        }

        const hsize_t memdims[2] = {1, 1};
        H5SResource memspace = H5Screate_simple(1, memdims, nullptr);

        const size_t field_offset = H5Tget_member_offset(dtype, field_idx);
        assert(field_offset < compound_size);

        std::vector<char> raw_chars(compound_size);
        H5Dread(dset, dtype, memspace, filespace,
                H5P_DEFAULT, &raw_chars[0]);

        memcpy(dest_ptr, &raw_chars[field_offset], num_bytes);
        return num_bytes;
    }

    //! \brief Return the number of elements in this dataset.
    //! This can be called on an "active" HDF5 connection
    //! during simulation, or on an "inactive" connection
    //! outside of a simulation.
    size_t getNumElements() const
    {
        return num_writes_;
    }

private:
    //! Calculate the number of bytes in one row of
    //! this dataset table.
    size_t getOneRecordNumBytes_() const
    {
        if (is_struct_table_) {
            return getOneRecordNumBytesForStructTable_();
        } else {
            return getOneRecordNumBytesForNonContiguousTable_();
        }
    }

    //! Calculate the number of bytes in one row of
    //! this dataset table. Here, a "struct table" is
    //! one which was defined in the schema using the
    //! addTable() / addField() methods. Tables defined
    //! like this are free to use the more performant
    //! TableRef::createObjectFromStruct() API, which
    //! takes a literal struct of POD's and writes
    //! the struct to file directly just reading bytes
    //! from the caller's struct:
    //!
    //!    struct Foo {
    //!        //field 1
    //!        //...
    //!        //field 100
    //!    };
    //!    Foo f;
    //!
    //!    sim_db_->getTable("Foo")->createObjectFromStruct(f);
    size_t getOneRecordNumBytesForStructTable_() const
    {
        assert(is_struct_table_);

        const auto & col = columns_.back();

        size_t byte_offset = col.getByteOffset();

        byte_offset += getFixedNumBytesForColumnDType(
            col.getDataType(),
            col.getDimensions());

        return byte_offset;
    }

    //! Calculate the number of bytes in one row of
    //! this dataset table. Here, a "non-contiguous"
    //! table is one which was defined in the schema
    //! using the addTable() / addColumn() methods.
    //! Tables built with these APIs must use the
    //! TableRef::createObject(), createObjectWithArgs(),
    //! and/or createObjectWithVals() APIs.
    size_t getOneRecordNumBytesForNonContiguousTable_() const
    {
        assert(!is_struct_table_);

        size_t record_num_bytes = 0;
        for (const auto & col : columns_) {
            record_num_bytes += getFixedNumBytesForColumnDType(
                col.getDataType(),
                columns_.back().getDimensions());
        }

        return record_num_bytes;
    }

    //! Append a field to this dataset with the given ID.
    void addColumnsToCompoundDataType_(
        const hid_t compound_dtid)
    {
        if (is_struct_table_) {
            addColumnsToCompoundDataTypeForStructTable_(compound_dtid);
        } else {
            addColumnsToCompoundDataTypeForNonContiguousTable_(compound_dtid);
        }
    }

    //! Append a field to this dataset with the given ID,
    //! for datasets that are populated using literal C
    //! structs as the input data source.
    void addColumnsToCompoundDataTypeForStructTable_(
        const hid_t compound_dtid)
    {
        assert(is_struct_table_);

        for (const auto & col : columns_) {
            const size_t el_offset = col.getByteOffset();
            auto field_dtype = getScopedDTypeForHDF5(col);

            H5Tinsert(compound_dtid, col.getName().c_str(),
                      el_offset, field_dtype.getDataTypeId());
        }
    }

    //! Append a field to this dataset with the given ID,
    //! for datasets that are populated using separate
    //! variables, even if those variables are all fixed-
    //! size data types.
    void addColumnsToCompoundDataTypeForNonContiguousTable_(
        const hid_t compound_dtid)
    {
        assert(!is_struct_table_);

        size_t el_offset = 0;
        for (const auto & col : columns_) {
            auto field_dtype = getScopedDTypeForHDF5(col);

            H5Tinsert(compound_dtid, col.getName().c_str(),
                      el_offset, field_dtype.getDataTypeId());

            el_offset += getFixedNumBytesForColumnDType(col.getDataType());
        }
    }

    H5SResource memspace_;
    size_t num_writes_ = 0;
    std::vector<Column> columns_;
    bool is_struct_table_ = false;
    std::string table_name_;
    std::shared_ptr<HDF5DatasetIds> dataset_ids_;
    std::string dataset_name_;
};

//! \brief Utility method which looks for a file relative
//! to the working directory. Tries to find the file with
//! and without the provided directory.
//!
//! \param db_dir Directory path to the HDF5 file
//!
//! \param db_file HDF5 file name, including the ".h5"
//! extension
//!
//! \return Returns the full filename if the file was found,
//! returns an empty string if not.
std::string resolveDbFilename(
    const std::string & db_dir,
    const std::string & db_file)
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

/*!
 * \class HDF5ConnProxy::Impl
 *
 * \brief HDF5 implementation class for SimDB
 */
class HDF5ConnProxy::Impl
{
public:
    //! \brief During database schema creation, tables
    //! may be created in a "non-contiguous data" way,
    //! like so:
    //!
    //!     simdb::Schema schema;
    //!
    //!     schema.addTable("MyNonContig")
    //!         .addColumn("X", dt::string_t)
    //!         .addColumn("Y", dt::int64_t);
    //!
    //! This is the table format that SQLiteConnProxy
    //! uses, but HDF5 tables can have individual columns
    //! put together to be physically contiguous, like a
    //! C struct. Imagine the following two call sites
    //! that want to write this simple struct into a
    //! table:
    //!
    //!     struct MyContig {
    //!         int16_t A = rand();
    //!         int16_t B = rand();
    //!         float C = (float)rand() / 10000 * 3.14;
    //!     };
    //!
    //! The SQLite-esque way of doing it would be to
    //! define the table as non-contiguous, and write
    //! the values into the TableRef APIs in separate
    //! variables, like this:
    //!
    //!     schema.addTable("MyContig")
    //!         .addColumn("A", dt::int16_t)
    //!         .addColumn("B", dt::int16_t)
    //!         .addColumn("C", dt::float_t);
    //!
    //!     void writeRow(ObjectManager & db, const MyContig & mc)
    //!     {
    //!         db.getTable("MyContig")->createObjectWithArgs(
    //!             "A", mc.A, "B", mc.B, "C", mc.C);
    //!     }
    //!
    //! Another way to do the same thing (though with
    //! better performance) would be to define the table
    //! like it is a C struct with fields:
    //!
    //!     schema.addTable("MyContig")
    //!         .addField("A", dt::int16_t, FOFFSET(MyContig,A))
    //!         .addField("B", dt::int16_t, FOFFSET(MyContig,B))
    //!         .addField("C", dt::float_t, FOFFSET(MyContig,C));
    //!
    //! This "struct table" could be written to like this:
    //!
    //!     void writeRow(ObjectManager & db, const MyContig & mc)
    //!     {
    //!         db.getTable("MyContig")->createObjectWithVals(
    //!             mc.A, mc.B, mc.C);
    //!     }
    //!
    //! The lack of "A"/"B"/"C" specifiers like you see in the first
    //! createObjectWithArgs() call is allowed because C structs of
    //! POD's are a fixed number of bytes, and you can't rearrange
    //! their fields from one binary write to the next, so we can
    //! get a performance boost by writing the POD values directly,
    //! without any column names to go with the values at the call
    //! site. One last API which we could also use looks like this:
    //!
    //!     void writeRow(ObjectManager & db, const MyContig & mc)
    //!     {
    //!         db.getTable("MyContig")->createObjectFromStruct(mc);
    //!     }
    //!
    //! The TableRef::createObjectFromStruct() API should only be
    //! called for tables that were defined using the addField()
    //! API to build struct tables.
    //!
    //! Unlike SQLite, we can support all three of these use cases
    //! for HDF5. This method here lets the schema creation classes
    //! tell us which tables can be interpreted as C structures.
    //!
    //! \param struct_tables Set of table names referring to schema
    //! tables that were constructed with the addField() API's to
    //! signify contiguous, fixed-size fields like you would have
    //! in a struct of POD's.
    void setStructTables(std::unordered_set<std::string> && struct_tables)
    {
        struct_tables_ = std::move(struct_tables);
    }

    //! \brief Turn a Schema object into an actual database
    //! connection
    //!
    //! \param schema Schema object to realize
    //!
    //! \param obj_mgr SimDB ObjectManager that this DbConnProxy
    //! object belongs to
    void realizeSchema(
        const Schema & schema,
        const ObjectManager &)
    {
        static const std::map<
            std::string,
            std::shared_ptr<HDF5DatasetIds>
        > empty_table_ids;

        realizeSchema_(schema, empty_table_ids);
    }

    //! \return The HDF5 file identifier. Similar to FILE*.
    hid_t getFileId() const
    {
        return hfile_;
    }

    //! \brief Get the full database filename being used. This
    //! includes the database path, stem, and extension. Returns
    //! empty if no connection is open.
    //!
    //! \return Full filename, such as "/tmp/abcd-1234.h5"
    const std::string & getDatabaseFullFilename() const
    {
        return db_full_filename_;
    }

    //! \brief Try to open a connection to an existing database file
    //!
    //! \param db_file HDF5 file name, including the ".h5" extension
    //!
    //! \return Return true on successful connection, false otherwise
    bool connectToExistingDatabase(const std::string & db_file)
    {
        if (openDbFile(".", db_file, false).empty() || !hfile_.good()) {
            return false;
        }

        HDF5FileScanner scanner;
        const Schema schema = scanner.scanSchema(hfile_);
        const auto & dataset_ids = scanner.getDatasetIds();
        realizeSchema_(schema, dataset_ids);

        return true;
    }

    //! \return Returns true if this database is open and ready to
    //! take read and write requests, false otherwise
    bool isValid() const
    {
        return hfile_.good();
    }

    //! \brief Given a table (dataset) and a column (field) for
    //! a particular record (element index / linear offset), read
    //! one field's value from the database
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
        const size_t num_bytes) const
    {
        auto iter = datasets_.find(table_name);
        if (iter != datasets_.end()) {
            return iter->second->readRawBytes(
                prop_name, db_id, dest_ptr, num_bytes);
        }
        return 0;
    }

    //! \brief First-time database file open
    //!
    //! \param db_dir Directory path to the HDF5 file
    //!
    //! \param db_file HDF5 file name, including the ".h5"
    //! extension
    //!
    //! \param create_file Flag which tells us if we should
    //! create a brand new file, or open an existing file
    //!
    //! \return The full filename of the opened HDF5 file
    std::string openDbFile(
        const std::string & db_dir,
        const std::string & db_file,
        const bool create_file)
    {
        db_full_filename_ = resolveDbFilename(db_dir, db_file);

        bool open_existing = true;
        if (db_full_filename_.empty()) {
            if (create_file) {
                db_full_filename_ = db_dir + "/" + db_file;
                open_existing = false;
            } else {
                throw DBException("Could not find database file: '")
                    << db_dir << "/" << db_file;
            }
        }

        if (open_existing) {
            hfile_ = H5Fopen(db_full_filename_.c_str(),
                             H5F_ACC_RDWR,
                             H5P_DEFAULT);
        } else {
            hfile_ = H5Fcreate(db_full_filename_.c_str(),
                               H5F_ACC_TRUNC,
                               H5P_DEFAULT,
                               H5P_DEFAULT);
        }

        assert(!db_full_filename_.empty());
        return db_full_filename_;
    }

    //! \brief We maintain our own unique IDs for rows written
    //! into HDF5 tables, and they are zero-based, incrementing
    //! by one with each write into a particular table. We can
    //! answer the question "hasObject()" by comparing the ID
    //! given to us with the total number of elements in the
    //! dataset.
    //!
    //! \param table_name Table we are looking into for the
    //! database ID in question
    //!
    //! \param db_id Unique database ID for the given table
    //!
    //! \return True if this database ID was found in the given
    //! table, false otherwise
    bool hasObject(
        const std::string & table_name,
        const DatabaseID db_id) const
    {
        auto dset_iter = datasets_.find(table_name);
        if (dset_iter == datasets_.end()) {
            return false;
        }

        const size_t nelems = dset_iter->second->getNumElements();
        return (static_cast<size_t>(db_id) > 0 &&
                static_cast<size_t>(db_id) <= nelems);
    }

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
    //! \param raw_bytes Flat byte array containing all of the
    //! columns' values for the new record
    //!
    //! \return Database ID of the newly created record
    DatabaseID createFixedSizeObject(
        const std::string & table_name,
        const void * raw_bytes_ptr,
        const size_t num_raw_bytes)
    {
        auto dset_iter = datasets_.find(table_name);
        if (dset_iter == datasets_.end()) {
            throw DBException("Could not find table '") << table_name << "'";
        }

        if (raw_bytes_ptr == nullptr) {
            throw DBException("Cannot create a fixed-sized "
                              "HDF5 object with no data");
        }

        return dset_iter->second->writeRawBytes(raw_bytes_ptr, num_raw_bytes);
    }

private:
    //! Turn schema/table/row metadata into a realized HDF5 file,
    //! complete with the dataset objects we'll need in order to
    //! write simulation data.
    void realizeSchema_(
        const Schema & schema,
        const std::map<std::string,                     //Table name
                       std::shared_ptr<HDF5DatasetIds>  //Dataset IDs
                       > & dset_ids)
    {
        for (const auto & table : schema) {
            std::unique_ptr<HDF5Dataset> dset(new HDF5Dataset(table.getName()));
            if (struct_tables_.find(table.getName()) != struct_tables_.end()) {
                dset->interpretAsStruct();
            }

            for (const auto & column : table) {
                dset->addColumnToDataset(*column);
            }

            auto ids_iter = dset_ids.find(table.getName());
            if (ids_iter == dset_ids.end()) {
                dset->createDatasetInFile(getFileId(),
                                          table.getName(),
                                          table.getCompression());
            } else if (ids_iter->second != nullptr) {
                dset->recreateDatasetFromFile(ids_iter->second, table.getName());
            }

            datasets_[table.getName()] = std::move(dset);
        }
    }

    std::unordered_map<std::string, std::unique_ptr<HDF5Dataset>> datasets_;
    std::unordered_set<std::string> struct_tables_;
    H5FResource hfile_;
    std::string db_full_filename_;
};

HDF5ConnProxy::HDF5ConnProxy() : impl_(new HDF5ConnProxy::Impl)
{
}

HDF5ConnProxy::~HDF5ConnProxy()
{
}

//! \brief This validate method gets called when a schema is
//! given to an ObjectManager to use with an HDF5 connection.
//!
//! \param schema Schema object to validate
void HDF5ConnProxy::validateSchema(const Schema & schema) const
{
    std::unordered_set<std::string> struct_tables;

    for (const auto & tbl : schema) {
        const auto num_columns = std::distance(tbl.begin(), tbl.end());
        if (num_columns == 0) {
            continue;
        }

        bool first_col_evaluated = false;
        bool first_col_is_struct_field = false;

        for (const auto & col : tbl) {
            const size_t prod = std::accumulate(
                col->getDimensions().begin(),
                col->getDimensions().end(),
                1, std::multiplies<size_t>());

            if (prod == 0) {
                throw DBException("Invalid dimensions encountered in ")
                    << "HDF5 schema (0 is not allowed). Found in table "
                    << tbl.getName() << ":" << getColumnDTypeStr(*col);
            } else if (col->getDataType() == ColumnDataType::blob_t) {
                throw DBException("Invalid data type encountered in ")
                    << "HDF5 schema. Blob data types are not supported. "
                    << "Found in table " << tbl.getName() << ":"
                    << getColumnDTypeStr(*col);
            } else if (col->getDataType() == ColumnDataType::string_t) {
                throw DBException("Invalid data type encountered in ")
                    << "HDF5 schema. String data types are not supported. "
                    << "Found in table " << tbl.getName() << ":"
                    << getColumnDTypeStr(*col);
            }

            if (first_col_evaluated) {
                if (col->hasByteOffset() != first_col_is_struct_field) {
                    throw DBException("Table encountered which has column(s) ")
                        << "set as a field of a struct, and column(s) which are "
                        << "not defined as part of a struct";
                }
            } else {
                first_col_is_struct_field = col->hasByteOffset();
                first_col_evaluated = true;
            }
        }

        if (first_col_is_struct_field) {
            struct_tables.insert(tbl.getName());
        }
    }

    impl_->setStructTables(std::move(struct_tables));
}

void HDF5ConnProxy::realizeSchema(
    const Schema & schema,
    const ObjectManager & obj_mgr)
{
    impl_->realizeSchema(schema, obj_mgr);
}

bool HDF5ConnProxy::connectToExistingDatabase(const std::string & db_file)
{
    return impl_->connectToExistingDatabase(db_file);
}

std::string HDF5ConnProxy::getDatabaseFullFilename() const
{
    return impl_->getDatabaseFullFilename();
}

bool HDF5ConnProxy::isValid() const
{
    return impl_->isValid();
}

std::string HDF5ConnProxy::openDbFile_(
    const std::string & db_dir,
    const std::string & db_file,
    const bool create_file)
{
    return impl_->openDbFile(db_dir, db_file, create_file);
}

bool HDF5ConnProxy::hasObjectImpl_(
    const std::string & table_name,
    const DatabaseID db_id) const
{
    return impl_->hasObject(table_name, db_id);
}

size_t HDF5ConnProxy::readRawBytes(
    const std::string & table_name,
    const std::string & prop_name,
    const DatabaseID db_id,
    void * dest_ptr,
    const size_t num_bytes) const
{
    return impl_->readRawBytes(
        table_name, prop_name, db_id, dest_ptr, num_bytes);
}

AnySizeObjectFactory HDF5ConnProxy::getObjectFactoryForTable(
    const std::string &) const
{
    return +[](DbConnProxy * db_proxy,
               const std::string & table_name,
               const ColumnValues & values)
    {
        //Take the incoming column values, put them into
        //a contiguous vector of raw bytes, and call the
        //"fixed-size" factory method to create the object
        //with these column values.
        std::vector<char> raw_bytes;

        for (const auto & col : values) {
            const size_t cur_num_bytes = raw_bytes.size();

            const size_t elm_num_bytes =
                getFixedNumBytesForColumnDType(col.getDataType());

            const size_t new_num_bytes = cur_num_bytes + elm_num_bytes;

            raw_bytes.resize(new_num_bytes);
            memcpy(&raw_bytes[cur_num_bytes],
                   col.getDataPtr(),
                   elm_num_bytes);
        }

        return static_cast<HDF5ConnProxy*>(db_proxy)->
            createFixedSizeObject(table_name, &raw_bytes[0], raw_bytes.size());
    };
}

FixedSizeObjectFactory HDF5ConnProxy::getFixedSizeObjectFactoryForTable(
    const std::string &) const
{
    return +[](DbConnProxy * db_proxy,
               const std::string & table_name,
               const void * raw_bytes_ptr,
               const size_t num_raw_bytes)
    {
        return static_cast<HDF5ConnProxy*>(db_proxy)->
            createFixedSizeObject(table_name, raw_bytes_ptr, num_raw_bytes);
    };
}

DatabaseID HDF5ConnProxy::createObject(
    const std::string &,
    const ColumnValues &)
{
    //Until the HDF5 SimDB implementation supports column
    //data types that are variable in length (such as strings),
    //this method should never be getting called.
    throw DBException("Not implemented");
}

DatabaseID HDF5ConnProxy::createFixedSizeObject(
    const std::string & table_name,
    const void * raw_bytes_ptr,
    const size_t num_raw_bytes)
{
    return impl_->createFixedSizeObject(table_name, raw_bytes_ptr, num_raw_bytes);
}

} // namespace simdb
