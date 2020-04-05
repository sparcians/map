// <ColumnValue> -*- C++ -*-

#pragma once

//! SimDB column values can be numeric, strings, or
//! blobs, and in all cases they can be represented
//! with a data type enumeration, a void* that can
//! be casted to the actual type (int16_t, double,
//! etc.) and the name of the column.

#include "simdb/schema/ColumnMetaStructs.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/Errors.hpp"

#include <iostream>
#include <deque>

namespace simdb {

//! This object holds onto the minimum information needed
//! to get a column's value. It uses a void* to the data
//! and is able to cast it to the correct underlying type.
class ColumnValueBase
{
public:
    virtual ~ColumnValueBase() = default;

    const std::string & getColumnName() const {
        return name_;
    }

    ColumnDataType getDataType() const {
        return dt_;
    }

    const void * getDataPtr() const {
        return getValuePtrAt_(0);
    }

    template <typename T>
    typename std::enable_if<
        !std::is_same<T, Blob>::value and
        !std::is_same<T, std::string>::value and
        !std::is_same<typename std::decay<T>::type, const char*>::value,
    T>::type
    getAs() const {
        if (column_info<T>::data_type() != dt_) {
            throw DBException("Invalid call to ColumnValueBase::getAs<T>() ")
                << "- attempt to cast to invalid data type";
        }
        const void * val = getValuePtrAt_(0);
        return *static_cast<const typename column_info<T>::value_type*>(val);
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<typename std::decay<T>::type, const char*>::value,
    T>::type
    getAs() const {
        if (column_info<T>::data_type() != dt_) {
            throw DBException("Invalid call to ColumnValueBase::getAs<T>() ")
                << "- attempt to cast to invalid data type";
        }
        const void * val = getValuePtrAt_(0);
        return static_cast<typename column_info<T>::value_type>(val);
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
    T>::type
    getAs() const {
        const char * charval = getAs<const char*>();
        return std::string(charval);
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<T, Blob>::value,
    T>::type
    getAs() const {
        if (column_info<T>::data_type() != dt_) {
            throw DBException("Invalid call to ColumnValueBase::getAs<T>() ")
                << "- attempt to cast to invalid data type";
        }

        Blob blob_descriptor;
        const void * val = getValuePtrAt_(0);
        const Blob * my_descriptor = static_cast<const Blob*>(val);
        blob_descriptor.data_ptr = my_descriptor->data_ptr;
        blob_descriptor.num_bytes = my_descriptor->num_bytes;
        return blob_descriptor;
    }

    template <typename T>
    typename std::enable_if<
        !std::is_same<T, Blob>::value and
        !std::is_same<T, std::string>::value and
        !std::is_same<typename std::decay<T>::type, const char*>::value,
    T>::type
    getAs(const size_t idx) const {
        if (column_info<T>::data_type() != dt_) {
            throw DBException("Invalid call to ColumnValueBase::getAs<T>() ")
                << "- attempt to cast to invalid data type";
        }

        using col_type = typename column_info<T>::value_type;
        const void * val = getValuePtrAt_(idx);
        return *static_cast<const col_type*>(val);
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<typename std::decay<T>::type, const char*>::value,
    T>::type
    getAs(const size_t idx) const {
        if (column_info<T>::data_type() != dt_) {
            throw DBException("Invalid call to ColumnValueBase::getAs<T>() ")
                << "- attempt to cast to invalid data type";
        }

        const void * val = getValuePtrAt_(idx);
        return static_cast<typename column_info<T>::value_type>(val);
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
    T>::type
    getAs(const size_t idx) const {
        const char * charval = getAs<const char*>(idx);
        return std::string(charval);
    }

    //! ColumnValue objects might be holding onto a set of
    //! values, for example:
    //!
    //!    UPDATE Accounts SET Active=0
    //!    WHERE LastName IN ('Smith','Thompson')
    //!
    //! Call this method to get the number of column values
    //! this object is holding.
    size_t getNumValues() const {
        return vals_.size();
    }

    //! For ColumnValue objects that are used when building
    //! up a database WHERE clause, tack on the value constraint.
    void setConstraint(const constraints constraint) {
        if (constraint == constraints::INVALID) {
            throw DBException(
                "Cannot call ColumnValue::setConstraint() passing "
                "in constraints::INVALID");
        }
        constraint_ = constraint;
    }

    //! For ColumnValue objects that are used when building
    //! up a database WHERE clause, get the value constraint.
    //! This throws if the setConstraint() method was never
    //! called. Check hasConstraint() before calling this
    //! method if you are unsure.
    constraints getConstraint() const {
        if (constraint_ == constraints::INVALID) {
            throw DBException("ColumnValue::getConstraint() called ")
                << "on an object whose constraint has not been set";
        }
        return constraint_;
    }

    //! See if this ColumnValue has a constraint attached to
    //! it. This applies to ColumnValue objects that are used
    //! when building a database WHERE clause for an UPDATE or
    //! DELETE.
    bool hasConstraint() const {
        return constraint_ != constraints::INVALID;
    }

    ColumnValueBase(const ColumnValueBase &) = default;
    ColumnValueBase(ColumnValueBase &&) = default;

protected:
    ColumnValueBase(const std::string & name,
                    const ColumnDataType dt) :
        name_(name),
        dt_(dt)
    {}

    void setValuePtr_(const void * valptr) {
        vals_.emplace_back(valptr);
    }

    void setValuePtrs_(std::vector<const void*> && valptrs) {
        vals_.insert(vals_.end(), valptrs.begin(), valptrs.end());
    }

private:
    //! Private data access. ColumnValueContainer is
    //! only outside class who can get to this pointer
    //! directly. Everyone else must go through the
    //! getAs<T>() methods.
    inline const void * getValuePtr_() const {
        return getValuePtrAt_(0);
    }
    inline const void * getValuePtrAt_(const size_t idx) const {
        return vals_.at(idx);
    }
    friend class ColumnValueContainer;

    const std::string name_;
    std::vector<const void*> vals_;
    const ColumnDataType dt_;
    constraints constraint_ = constraints::INVALID;
};

//! Lightweight "copy" of table column's value. Identified
//! by its data type and a pointer to its value. Use the
//! data type to cast the pointer accordingly.
using ColumnValues = std::deque<ColumnValueBase>;

//! Base template class for enable_if's below
template <typename ColumnT, typename Enable = void>
class ColumnValue;

//! Integer and floating-point values
template <typename ColumnT>
class ColumnValue<ColumnT,
    typename std::enable_if<std::is_arithmetic<ColumnT>::value>::type>
  : public ColumnValueBase
{
public:
    ColumnValue(const std::string & name,
                const void * valptr) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtr_(valptr);
    }

    ColumnValue(const std::string & name,
                std::vector<const void*> && valptrs) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtrs_(std::move(valptrs));
    }
};

//! String literal values
template <typename ColumnT>
class ColumnValue<ColumnT,
    typename std::enable_if<std::is_same<
        typename std::decay<ColumnT>::type, const char*>::value>::type>
  : public ColumnValueBase
{
public:
    ColumnValue(const std::string & name,
                const void * valptr) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtr_(valptr);
    }

    ColumnValue(const std::string & name,
                std::vector<const void*> && valptrs) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtrs_(std::move(valptrs));
    }
};

//! std::string values
template <typename ColumnT>
class ColumnValue<ColumnT,
    typename std::enable_if<std::is_same<ColumnT, std::string>::value>::type>
  : public ColumnValueBase
{
public:
    ColumnValue(const std::string & name,
                const void * valptr) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtr_(valptr);
    }

    ColumnValue(const std::string & name,
                std::vector<const void*> && valptrs) :
        ColumnValueBase(name, column_info<ColumnT>::data_type())
    {
        setValuePtrs_(std::move(valptrs));
    }
};

//! Blob values
template <typename ColumnT>
class ColumnValue<ColumnT,
    typename std::enable_if<std::is_same<ColumnT, Blob>::value>::type>
  : public ColumnValueBase
{
public:
    ColumnValue(const std::string & name,
                const void * valptr) :
        ColumnValueBase(name, ColumnDataType::blob_t)
    {
        setValuePtr_(valptr);
    }
};

//! STL container values
template <typename ColumnT>
class ColumnValue<ColumnT,
    typename std::enable_if<
        is_container<ColumnT>::value and
        is_contiguous<ColumnT>::value>::type>
  : public ColumnValueBase
{
public:
    ColumnValue(const std::string & name,
                const void * valptr) :
        ColumnValueBase(name, ColumnDataType::blob_t)
    {
        setValuePtr_(valptr);
    }
};

}

