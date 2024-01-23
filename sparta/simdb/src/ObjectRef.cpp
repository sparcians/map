// <ObjectRef> -*- C++ -*-

#include "simdb/ObjectRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/utils/CompatUtils.hpp"

//SQLite-specific headers
#include "simdb/impl/sqlite/TransactionUtils.hpp"
#include <sqlite3.h>

namespace simdb {

//! Local helper which sets a record's scalar property.
//! All numeric properties get set through this method.
template <typename PropertyDataT>
typename std::enable_if<
    std::is_fundamental<PropertyDataT>::value,
void>::type
LOCAL_setScalarProperty(const std::string & table_name,
                        const std::string & prop_name,
                        const PropertyDataT prop_value,
                        const DatabaseID db_id,
                        const ObjectManager & obj_mgr)
{
    auto table = obj_mgr.getTable(table_name);

    if (!table->updateRowValues(prop_name.c_str(), prop_value).
                forRecordsWhere("Id", constraints::equal, db_id))
    {
        throw DBException("Unable to write database property '")
            << table_name << "::" << prop_name << "' for record "
            << "with Id " << db_id << ". Error occurred in database "
            << "file '" << obj_mgr.getDatabaseFile() << "'.";
    }
}

//! Local helper which sets a record's scalar property.
//! Specialization for std::string so we can pass it
//! in by const ref.
template <typename PropertyDataT>
typename std::enable_if<
    std::is_same<PropertyDataT, std::string>::value,
void>::type
LOCAL_setScalarProperty(const std::string & table_name,
                        const std::string & prop_name,
                        const PropertyDataT & prop_value,
                        const DatabaseID db_id,
                        const ObjectManager & obj_mgr)
{
    auto table = obj_mgr.getTable(table_name);

    if (!table->updateRowValues(prop_name.c_str(), prop_value).
                forRecordsWhere("Id", constraints::equal, db_id))
    {
        throw DBException("Unable to write database property '")
            << table_name << "::" << prop_name << "' for record "
            << "with Id " << db_id << ". Error occurred in database "
            << "file '" << obj_mgr.getDatabaseFile() << "'.";
    }
}

#define VERIFY_OBJECT_QUERY_SUPPORT \
  if (!obj_mgr_.getDbConn()->supportsObjectQuery_()) { \
      throw DBException("ObjectQuery is not supported."); \
  }

#define HAS_OBJECT_QUERY_SUPPORT \
  obj_mgr_.getDbConn()->supportsObjectQuery_()

//! Local helper which gets a record's scalar property.
template <typename PropertyDataT>
PropertyDataT LOCAL_getScalarProperty(const std::string & table_name,
                                      const std::string & prop_name,
                                      const DatabaseID db_id,
                                      const ObjectManager & obj_mgr)
{
    ObjectQuery query(obj_mgr, table_name);

    PropertyDataT prop_value{};
    query.writeResultIterationsTo(prop_name.c_str(), &prop_value);
    query.addConstraints("Id", constraints::equal, db_id);

    if (!query.executeQuery()->getNext())
    {
        throw DBException("Unable to read database property '")
            << table_name << "::" << prop_name << "' for record "
            << "with Id " << db_id << ". Error occurred in database "
            << "file '" << obj_mgr.getDatabaseFile() << "'.";
    }

    return prop_value;
}

//! Local helper which gets a record's scalar property
//! without using ObjectQuery.
template <typename PropertyDataT>
typename std::enable_if<
    utils::is_pod<PropertyDataT>::value,
PropertyDataT>::type
LOCAL_getScalarPropertyNoObjectQuery(
    const std::string & table_name,
    const std::string & prop_name,
    const DatabaseID db_id,
    const ObjectManager & obj_mgr)
{
    const DbConnProxy * db_proxy = obj_mgr.getDbConn();

    PropertyDataT prop_value;
    const size_t bytes_read = db_proxy->readRawBytes(
        table_name, prop_name, db_id,
        &prop_value, sizeof(PropertyDataT));

    if (bytes_read != sizeof(PropertyDataT)) {
        throw DBException("DbConnProxy::readRawBytes() failed");
    }
    return prop_value;
}

const ObjectManager & ObjectRef::getObjectManager() const
{
    return obj_mgr_;
}

DatabaseID ObjectRef::getId() const
{
    return db_id_;
}

void ObjectRef::setPropertyInt8(
    const std::string & prop_name,
    const int8_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyUInt8(
    const std::string & prop_name,
    const uint8_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyInt16(
    const std::string & prop_name,
    const int16_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyUInt16(
    const std::string & prop_name,
    const uint16_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyInt32(
    const std::string & prop_name,
    const int32_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyUInt32(
    const std::string & prop_name,
    const uint32_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyInt64(
    const std::string & prop_name,
    const int64_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyUInt64(
    const std::string & prop_name,
    const uint64_t prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyString(
    const std::string & prop_name,
    const std::string & prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyChar(
    const std::string & prop_name,
    const char prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyFloat(
    const std::string & prop_name,
    const float prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyDouble(
    const std::string & prop_name,
    const double prop_value)
{
    LOCAL_setScalarProperty(
        table_name_, prop_name, prop_value, db_id_, obj_mgr_);
}

void ObjectRef::setPropertyBlob(
    const std::string & prop_name,
    const Blob & prop_value)
{
    auto table = obj_mgr_.getTable(table_name_);

    if (!table->updateRowValues(prop_name.c_str(), prop_value).
                forRecordsWhere("Id", constraints::equal, db_id_))
    {
        throw DBException("Unable to write database property '")
            << table_name_ << "::" << prop_name << "' for record "
            << "with Id " << db_id_ << ". Error occurred in database "
            << "file '" << obj_mgr_.getDatabaseFile() << "'.";
    }
}

int8_t ObjectRef::getPropertyInt8(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<int8_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<int8_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

uint8_t ObjectRef::getPropertyUInt8(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<uint8_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<uint8_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

int16_t ObjectRef::getPropertyInt16(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<int16_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<int16_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

uint16_t ObjectRef::getPropertyUInt16(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<uint16_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<uint16_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

int32_t ObjectRef::getPropertyInt32(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<int32_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<int32_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

uint32_t ObjectRef::getPropertyUInt32(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<uint32_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<uint32_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

int64_t ObjectRef::getPropertyInt64(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<int64_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<int64_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

uint64_t ObjectRef::getPropertyUInt64(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<uint64_t>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<uint64_t>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

std::string ObjectRef::getPropertyString(const std::string & prop_name) const
{
    VERIFY_OBJECT_QUERY_SUPPORT
    return LOCAL_getScalarProperty<std::string>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

char ObjectRef::getPropertyChar(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<char>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<char>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

float ObjectRef::getPropertyFloat(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<float>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<float>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

double ObjectRef::getPropertyDouble(const std::string & prop_name) const
{
    if (HAS_OBJECT_QUERY_SUPPORT) {
        return LOCAL_getScalarProperty<double>(
            table_name_, prop_name, db_id_, obj_mgr_);
    }
    return LOCAL_getScalarPropertyNoObjectQuery<double>(
        table_name_, prop_name, db_id_, obj_mgr_);
}

void ObjectRef::prepGetPropertyBlob_(
    const std::string & prop_name,
    void ** statement,
    Blob & blob_descriptor) const
{
    VERIFY_OBJECT_QUERY_SUPPORT
    sqlite3_stmt * stmt_retrieve;

    std::ostringstream oss;
    oss << " SELECT " << prop_name << " FROM " << table_name_
        << " WHERE Id = " << db_id_;
    const std::string command = oss.str();

    //Create the prepared statement for this blob
    obj_mgr_.getDbConn()->prepareStatement_(command, statement);
    assert(statement != nullptr);
    stmt_retrieve = static_cast<sqlite3_stmt*>(*statement);

    //Execute the prepared statement
    if (sqlite3_step(stmt_retrieve) != SQLITE_ROW) {
        throw DBException("Error getting property '") << prop_name << "' for SQL table '"
            << table_name_ << "'";
    }

    //Notice the output argument for this function. We aren't
    //actually getting the blob values right here, we are just
    //getting the blob descriptor (void*, and number of bytes).
    blob_descriptor.data_ptr = sqlite3_column_blob(stmt_retrieve, 0);
    blob_descriptor.num_bytes = static_cast<size_t>(sqlite3_column_bytes(stmt_retrieve, 0));
}

void ObjectRef::finalizeGetPropertyBlob_(void * statement) const
{
    //Destroy the prepared statement - we are done with it.
    sqlite3_finalize(static_cast<sqlite3_stmt*>(statement));
}

} // namespace simdb
