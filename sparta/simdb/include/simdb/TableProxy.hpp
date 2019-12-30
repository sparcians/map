// <TableProxy> -*- C++ -*-

#ifndef __SIMDB_TABLE_PROXY_H__
#define __SIMDB_TABLE_PROXY_H__

#include "simdb/schema/DatabaseRoot.hpp"
#include "simdb/TableRef.hpp"
#include "simdb_fwd.hpp"

#include <memory>
#include <string>

namespace simdb {

/*!
 * \class TableProxy
 *
 * \brief When SimDB is used with table access triggers,
 * there may be times when a table in the database is not
 * in a writable state. Instead of returning null TableRef
 * objects to signal that the table is not available, we
 * instead return TableProxy objects which are never null.
 * When the table connection is available, it can be accessed
 * through this proxy.
 */
class TableProxy
{
public:
    //! Construct with the table name and ObjectManager
    //! this table belongs to.
    TableProxy(const std::string & table_name,
               const ObjectManager & obj_mgr,
               DatabaseNamespace * db_namespace) :
        table_name_(table_name),
        obj_mgr_(obj_mgr),
        db_namespace_(db_namespace)
    {}

    //! Check if this proxy is in a writable state.
    bool isWritable() const {
        refreshAccess_();
        return table_ref_ != nullptr;
    }

    //! Get the underlying TableRef from this proxy. This
    //! will return null without warnings or exceptions if
    //! the table is requested when it is unavailable.
    TableRef * getTable() const {
        refreshAccess_();
        return table_ref_.get();
    }

    //! Method called when the table becomes accessible.
    void grantAccess() {
        if (!table_ref_) {
            table_ref_ = obj_mgr_.getTable_(table_name_);
        }
    }

    //! Method called when the table becomes inaccessible.
    void revokeAccess() {
        table_ref_.reset();
    }

private:
    void refreshAccess_() const {
        if (db_namespace_) {
            db_namespace_->getDatabase();
        }
    }

    std::string table_name_;
    const ObjectManager & obj_mgr_;
    mutable DatabaseNamespace * db_namespace_ = nullptr;
    std::unique_ptr<TableRef> table_ref_;
    friend class DatabaseRoot;
};

} // namespace simdb

#endif
