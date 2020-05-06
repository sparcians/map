// <ColumnValueContainer> -*- C++ -*-

#pragma once

#include "simdb/schema/ColumnValue.hpp"
#include "simdb_fwd.hpp"

#include <string>
#include <type_traits>

namespace simdb {

/*!
 * \brief This container holds onto column values and an
 * enumeration which gives the column data type.
 * Values are accessible via the ColumnValue's
 * getAs<T>() method.
 */
class ColumnValueContainer
{
public:
    //! Add a POD column value
    template <typename ColumnT>
    typename std::enable_if<
        std::is_fundamental<ColumnT>::value,
    ColumnValueBase*>::type
    add(const char * col_name,
        const ColumnT col_val)
    {
        const void * valptr = getColumnDataValuePtr_(col_val);
        if (valptr) {
            ColumnValue<ColumnT> col(col_name, valptr);
            col_values_.emplace_back(col);
            return &col_values_.back();
        }
        return nullptr;
    }

    //! Add a string value (as string literal)
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    ColumnValueBase*>::type
    add(const char * col_name,
        ColumnT col_val)
    {
        const void * valptr = getColumnDataValuePtr_(col_val);
        if (valptr) {
            ColumnValue<ColumnT> col(col_name, valptr);
            col_values_.emplace_back(col);
            return &col_values_.back();
        }
        return nullptr;
    }

    //! Add a string value (as std::string)
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, std::string>::value,
    ColumnValueBase*>::type
    add(const char * col_name,
        const ColumnT & col_val)
    {
        return add(col_name, col_val.c_str());
    }

    //! Add a blob value (as std::vector<T>, where T is a scalar POD)
    template <typename ColumnT>
    typename std::enable_if<
        is_container<ColumnT>::value and is_contiguous<ColumnT>::value,
    ColumnValueBase*>::type
    add(const char * col_name,
        const ColumnT & col_val)
    {
        const void * valptr = getColumnDataValuePtr_(col_val);
        if (valptr) {
            ColumnValue<ColumnT> col(col_name, valptr);
            col_values_.emplace_back(col);
            return &col_values_.back();
        }
        return nullptr;
    }

    //! Add a blob value (as simdb::Blob)
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, Blob>::value,
    ColumnValueBase*>::type
    add(const char * col_name,
        const ColumnT & col_val)
    {
        const void * valptr = getColumnDataValuePtr_(col_val);
        if (valptr) {
            ColumnValue<ColumnT> col(col_name, valptr);
            col_values_.emplace_back(col);
            return &col_values_.back();
        }
        return nullptr;
    }

    //! Add a set of values in an initialization list
    template <typename ColumnT>
    ColumnValueBase * add(const char * col_name,
                          const std::initializer_list<ColumnT> & col_vals)
    {
        std::vector<const void*> valptrs;
        for (const auto val : col_vals) {
            const void * valptr = getColumnDataValuePtr_(val);
            if (valptr) {
                valptrs.emplace_back(valptr);
            }
        }

        if (!valptrs.empty()) {
            ColumnValue<ColumnT> col(col_name, std::move(valptrs));
            col_values_.emplace_back(col);
            return &col_values_.back();
        }
        return nullptr;
    }

    //! Get the underlying ColumnValue objects. Access the
    //! column values using the ColumnValue::getAs<T>()
    //! method.
    const ColumnValues & getValues() const {
        return col_values_;
    }

    //! Ask if there are any column values in this container.
    bool empty() const {
        return col_values_.empty();
    }

    //! Clear all column value objects in this container.
    void clear() {
        for (const auto & col : col_values_) {
            if (col.getDataType() == ColumnDataType::blob_t) {
                const void * valptr = col.getValuePtr_();
                if (valptr) {
                    delete static_cast<const Blob*>(valptr);
                }
            }
        }

        col_values_.clear();
        held_8bit_int_values_.clear();
        held_16bit_int_values_.clear();
        held_32bit_int_values_.clear();
        held_64bit_int_values_.clear();
        held_float_values_.clear();
        held_double_values_.clear();
    }

private:
    //! Return a void* to 8-bit integer column values
    template <typename ColumnT>
    typename std::enable_if<
        sizeof(ColumnT) == sizeof(int8_t) and
        std::is_integral<ColumnT>::value and
        not std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_8bit_int_values_.emplace_back(static_cast<int8_t>(col_val));
        return &held_8bit_int_values_.back();
    }

    //! Return a void* to 16-bit integer column values
    template <typename ColumnT>
    typename std::enable_if<
        sizeof(ColumnT) == sizeof(int16_t) and
        std::is_integral<ColumnT>::value and
        not std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_16bit_int_values_.emplace_back((int16_t)col_val);
        return &held_16bit_int_values_.back();
    }

    //! Return a void* to 32-bit integer column values
    template <typename ColumnT>
    typename std::enable_if<
        sizeof(ColumnT) == sizeof(int32_t) and
        std::is_integral<ColumnT>::value and
        not std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_32bit_int_values_.emplace_back((int32_t)col_val);
        return &held_32bit_int_values_.back();
    }

    //! Return a void* to 64-bit integer column values
    template <typename ColumnT>
    typename std::enable_if<
        sizeof(ColumnT) == sizeof(int64_t) and
        std::is_integral<ColumnT>::value and
        not std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_64bit_int_values_.emplace_back((int64_t)col_val);
        return &held_64bit_int_values_.back();
    }

    //! Return a void* to std::string column values
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, std::string>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT & col_val) {
        return col_val.c_str();
    }

    //! Return a void* to string literal column values
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    const void*>::type
    getColumnDataValuePtr_(ColumnT col_val) {
        return col_val;
    }

    //! Return a void* to float column values
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, float>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_float_values_.emplace_back(col_val);
        return &held_float_values_.back();
    }

    //! Return a void* to double column values
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, double>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT col_val) {
        held_double_values_.emplace_back(col_val);
        return &held_double_values_.back();
    }

    //! Return a void* to data values held in contiguous
    //! containers. For these column data types, we need
    //! both the void* to the actual data, and the number
    //! of bytes' worth of data values, so we return a
    //! new'd Blob descriptor. We'll delete it later.
    template <typename ColumnT>
    typename std::enable_if<
        is_container<ColumnT>::value and
        is_contiguous<ColumnT>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT & col_val) {
        if (col_val.empty()) {
            return nullptr;
        }

        constexpr auto nbytes =
            sizeof(typename is_contiguous<ColumnT>::value_type);

        Blob * blob_descriptor = new Blob;
        blob_descriptor->data_ptr = &col_val[0];
        blob_descriptor->num_bytes = col_val.size() * nbytes;
        return blob_descriptor;
    }

    //! Return a void* to data values held in contiguous containers.
    //! This is similar to the method above but is specifically for
    //! data passed in as simdb::Blob's, as opposed to STL containers.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, Blob>::value,
    const void*>::type
    getColumnDataValuePtr_(const ColumnT & col_val) {
        if (col_val.data_ptr == nullptr || col_val.num_bytes == 0) {
            return nullptr;
        }

        Blob * blob_descriptor = new Blob;
        blob_descriptor->data_ptr = col_val.data_ptr;
        blob_descriptor->num_bytes = col_val.num_bytes;
        return blob_descriptor;
    }

    //! Vector of objects holding onto column values. All values
    //! are in this data structure as a void* pointing somewhere
    //! else, along with the data type enumeration that can be
    //! used to cast that void* to the right type.
    ColumnValues col_values_;

    //! When we are given POD column values, we copy them into
    //! deques so they remain in scope while we hand out void*
    //! to the outside world (such as TableRef, or DbConnProxy
    //! subclasses).
    std::deque<int8_t> held_8bit_int_values_;
    std::deque<int16_t> held_16bit_int_values_;
    std::deque<int32_t> held_32bit_int_values_;
    std::deque<int64_t> held_64bit_int_values_;
    std::deque<float> held_float_values_;
    std::deque<double> held_double_values_;
};

}

