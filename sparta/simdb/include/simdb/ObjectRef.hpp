// <ObjectRef> -*- C++ -*-

#pragma once

#include "simdb_fwd.hpp"

#include "simdb/schema/ColumnTypedefs.hpp"
#include "simdb/ObjectManager.hpp"

#include <string>
#include <vector>
#include <cstring>

namespace simdb {

/*!
 * \brief Wrapper around a record (row) in a
 * database table. Used for writing and reading
 * record properties (column values).
 */
class ObjectRef
{
public:
    //! Construct an ObjectRef for a SimDB table record. Pass
    //! in the ObjectManager it belongs to, the name of the
    //! table this record belongs to, and the unique database
    //! ID for this record (primary key in the given table).
    //!
    //! Typically, you should get all ObjectRef's
    //! from an ObjectManager that you created first,
    //! or from a TableRef that you got from such an
    //! ObjectManager, instead of making an ObjectRef
    //! yourself manually.
    ObjectRef(const ObjectManager & obj_mgr,
              const std::string & table_name,
              const DatabaseID db_id) :
        obj_mgr_(obj_mgr),
        table_name_(table_name),
        db_id_(db_id)
    {}

    //! Returns the ObjectManager passed to our constructor
    const ObjectManager & getObjectManager() const;

    //! Get this record's unique database ID. It is unique
    //! to the table it lives in, not necessarily globally
    //! unique across all database tables.
    DatabaseID getId() const;

    //! PROPERTY SETTERS ---------------------------------
    void setPropertyInt8(
        const std::string & prop_name,
        const int8_t prop_value);

    void setPropertyUInt8(
        const std::string & prop_name,
        const uint8_t prop_value);

    void setPropertyInt16(
        const std::string & prop_name,
        const int16_t prop_value);

    void setPropertyUInt16(
        const std::string & prop_name,
        const uint16_t prop_value);

    void setPropertyInt32(
        const std::string & prop_name,
        const int32_t prop_value);

    void setPropertyUInt32(
        const std::string & prop_name,
        const uint32_t prop_value);

    void setPropertyInt64(
        const std::string & prop_name,
        const int64_t prop_value);

    void setPropertyUInt64(
        const std::string & prop_name,
        const uint64_t prop_value);

    void setPropertyString(
        const std::string & prop_name,
        const std::string & prop_value);

    void setPropertyChar(
        const std::string & prop_name,
        const char prop_value);

    void setPropertyFloat(
        const std::string & prop_name,
        const float prop_value);

    void setPropertyDouble(
        const std::string & prop_name,
        const double prop_value);

    void setPropertyBlob(
        const std::string & prop_name,
        const Blob & prop_value);

    //! PROPERTY GETTERS ---------------------------------
    int8_t getPropertyInt8(
        const std::string & prop_name) const;

    uint8_t getPropertyUInt8(
        const std::string & prop_name) const;

    int16_t getPropertyInt16(
        const std::string & prop_name) const;

    uint16_t getPropertyUInt16(
        const std::string & prop_name) const;

    int32_t getPropertyInt32(
        const std::string & prop_name) const;

    uint32_t getPropertyUInt32(
        const std::string & prop_name) const;

    int64_t getPropertyInt64(
        const std::string & prop_name) const;

    uint64_t getPropertyUInt64(
        const std::string & prop_name) const;

    std::string getPropertyString(
        const std::string & prop_name) const;

    char getPropertyChar(
        const std::string & prop_name) const;

    float getPropertyFloat(
        const std::string & prop_name) const;

    double getPropertyDouble(
        const std::string & prop_name) const;

    //Note here that blobs are returned in a vector
    //of bytes that you create and own at the call
    //site:
    //
    //  std::vector<double> record_values;
    //  obj_ref->getPropertyBlob("RawDataValues", record_values);
    //
    //The ObjectRef will only copy the bytes into
    //the vector you provide; it will not cache
    //anything under the hood. Your vector will
    //be resized to exactly fit the record blob,
    //which could of course be 0 elements.
    template <typename BlobDataT>
    void getPropertyBlob(
        const std::string & prop_name,
        std::vector<BlobDataT> & prop_bytes) const;

private:
    //Called at the beginning of getPropertyBlob()
    //  - not inlined in the template code below
    void prepGetPropertyBlob_(
        const std::string & prop_name,
        void ** statement,
        Blob & blob_descriptor) const;

    //Called at the end of getPropertyBlob()
    //  - not inlined in the template code below
    void finalizeGetPropertyBlob_(
        void * statement) const;

    const ObjectManager & obj_mgr_;
    const std::string table_name_;
    const DatabaseID db_id_;
};

//! Inlined template code for getPropertyBlob()
//!   (non-template / common code is in the .cpp file)
template <typename BlobDataT>
void ObjectRef::getPropertyBlob(const std::string & prop_name,
                                std::vector<BlobDataT> & prop_value) const
{
    obj_mgr_.safeTransaction([&]() {
        //Get the blob void* and number of bytes
        Blob blob_descriptor;
        void * statement = nullptr;
        prepGetPropertyBlob_(prop_name, &statement, blob_descriptor);

        const void * blob_ptr = blob_descriptor.data_ptr;
        const size_t blob_num_bytes = blob_descriptor.num_bytes;
        if (blob_num_bytes > 0) {
            prop_value.resize(blob_num_bytes / sizeof(BlobDataT));
            void * dest = &prop_value[0];
            memcpy(dest, blob_ptr, blob_num_bytes);
        } else {
            prop_value.clear();
            //Just to be extra safe, let's shrink the output
            //property value vector. We may choose not to do
            //this for performance reasons later on, but for
            //now we can play it safe.
            prop_value.shrink_to_fit();
        }

        //Destroy the prepared statement - we are done with it.
        finalizeGetPropertyBlob_(statement);
    });
}

} // namespace simdb
