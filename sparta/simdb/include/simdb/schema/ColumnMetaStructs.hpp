// <MetaStructs> -*- C++ -*-

#pragma once

/*!
 * \brief Metaprogramming utilities for use
 * with SimDB schemas.
 */

#include "simdb/schema/GeneralMetaStructs.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"
#include "simdb/Errors.hpp"
#include "simdb/utils/CompatUtils.hpp"

#include <string>
#include <type_traits>
#include <vector>
#include <numeric>

namespace simdb {

//! Base template for column_info structs
template <typename ColumnT, typename Enable = void>
struct column_info;

//! int8_t
template <>
struct column_info<int8_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::int8_t;
    }
    using value_type = int8_t;
    static constexpr bool is_fixed_size = utils::is_pod<int8_t>::value;
};

//! uint8_t
template <>
struct column_info<uint8_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::uint8_t;
    }
    using value_type = uint8_t;
    static constexpr bool is_fixed_size = utils::is_pod<uint8_t>::value;
};

//! int16_t
template <>
struct column_info<int16_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::int16_t;
    }
    using value_type = int16_t;
    static constexpr bool is_fixed_size = utils::is_pod<int16_t>::value;
};

//! uint16_t
template <>
struct column_info<uint16_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::uint16_t;
    }
    using value_type = uint16_t;
    static constexpr bool is_fixed_size = utils::is_pod<uint16_t>::value;
};

//! int32_t
template <>
struct column_info<int32_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::int32_t;
    }
    using value_type = int32_t;
    static constexpr bool is_fixed_size = utils::is_pod<int32_t>::value;
};

//! uint32_t
template <>
struct column_info<uint32_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::uint32_t;
    }
    using value_type = uint32_t;
    static constexpr bool is_fixed_size = utils::is_pod<uint32_t>::value;
};

//! int64_t
template <>
struct column_info<int64_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::int64_t;
    }
    using value_type = int64_t;
    static constexpr bool is_fixed_size = utils::is_pod<int64_t>::value;
};

//! uint64_t
template <>
struct column_info<uint64_t> {
    static ColumnDataType data_type() {
        return ColumnDataType::uint64_t;
    }
    using value_type = uint64_t;
    static constexpr bool is_fixed_size = utils::is_pod<uint64_t>::value;
};

//! float
template <>
struct column_info<float> {
    static ColumnDataType data_type() {
        return ColumnDataType::float_t;
    }
    using value_type = float;
    static constexpr bool is_fixed_size = utils::is_pod<float>::value;
};

//! double
template <>
struct column_info<double> {
    static ColumnDataType data_type() {
        return ColumnDataType::double_t;
    }
    using value_type = double;
    static constexpr bool is_fixed_size = utils::is_pod<double>::value;
};

//! string
template <typename ColumnT>
struct column_info<ColumnT, typename std::enable_if<
    std::is_same<ColumnT, std::string>::value or
    std::is_same<typename std::decay<ColumnT>::type, const char*>::value>::type>
{
    static ColumnDataType data_type() {
        return ColumnDataType::string_t;
    }
    using value_type = ColumnT;
    static constexpr bool is_fixed_size = utils::is_pod<std::string>::value;
};

//! char
template <>
struct column_info<char> {
    static ColumnDataType data_type() {
        return ColumnDataType::char_t;
    }
    using value_type = char;
    static constexpr bool is_fixed_size = utils::is_pod<char>::value;
};

//! Vectors of raw bytes are stored as blobs (void* / opaque)
template <typename ColumnT>
struct column_info<ColumnT, typename std::enable_if<
    is_container<ColumnT>::value>::type>
{
    static ColumnDataType data_type() {
        return ColumnDataType::blob_t;
    }
    using value_type = typename is_container<ColumnT>::value_type;
    static constexpr bool is_fixed_size = utils::is_pod<ColumnT>::value;
};

//! Blob descriptor
template <typename ColumnT>
struct column_info<ColumnT, typename std::enable_if<
    std::is_same<ColumnT, Blob>::value>::type>
{
    static ColumnDataType data_type() {
        return ColumnDataType::blob_t;
    }
    using value_type = Blob;
    static constexpr bool is_fixed_size = utils::is_pod<ColumnT>::value;
};

//! See if the given column data type has a fixed number
//! of bytes, as determined by utils::is_pod<T>
inline bool getColumnIsFixedSize(const ColumnDataType dtype)
{
    using dt = ColumnDataType;

    switch (dtype) {
        case dt::char_t: {
            return column_info<char>::is_fixed_size;
        }
        case dt::int8_t: {
            return column_info<int8_t>::is_fixed_size;
        }
        case dt::uint8_t: {
            return column_info<uint8_t>::is_fixed_size;
        }
        case dt::int16_t: {
            return column_info<int16_t>::is_fixed_size;
        }
        case dt::uint16_t: {
            return column_info<uint16_t>::is_fixed_size;
        }
        case dt::int32_t: {
            return column_info<int32_t>::is_fixed_size;
        }
        case dt::uint32_t: {
            return column_info<uint32_t>::is_fixed_size;
        }
        case dt::int64_t: {
            return column_info<int64_t>::is_fixed_size;
        }
        case dt::uint64_t: {
            return column_info<uint64_t>::is_fixed_size;
        }
        case dt::float_t: {
            return column_info<float>::is_fixed_size;
        }
        case dt::double_t: {
            return column_info<double>::is_fixed_size;
        }
        case dt::string_t: {
            return column_info<std::string>::is_fixed_size;
        }
        case dt::blob_t: {
            return column_info<Blob>::is_fixed_size;
        }
        case dt::fkey_t: {
            return true;
        }
    }

    simdb_throw("Unreachable")
    return false;
}

//! Get the number of bytes for a fixed-size column data type.
//! This method throws if getColumnIsFixedSize() returns
//! false for the given data type.
inline size_t getFixedNumBytesForColumnDType(
    const ColumnDataType dtype,
    const std::vector<size_t> & dims = {1})
{
    using dt = ColumnDataType;

    if (!getColumnIsFixedSize(dtype)) {
        throw DBException("Data type is not fixed-size");
    }

    const size_t dims_mult =
        dims.empty() ? 1UL :
        std::accumulate(dims.begin(), dims.end(),
                        1, std::multiplies<size_t>());

    switch (dtype) {
        case dt::char_t: {
            return dims_mult * sizeof(column_info<char>::value_type);
        }
        case dt::int8_t: {
            return dims_mult * sizeof(column_info<int8_t>::value_type);
        }
        case dt::uint8_t: {
            return dims_mult * sizeof(column_info<uint8_t>::value_type);
        }
        case dt::int16_t: {
            return dims_mult * sizeof(column_info<int16_t>::value_type);
        }
        case dt::uint16_t: {
            return dims_mult * sizeof(column_info<uint16_t>::value_type);
        }
        case dt::int32_t: {
            return dims_mult * sizeof(column_info<int32_t>::value_type);
        }
        case dt::uint32_t: {
            return dims_mult * sizeof(column_info<uint32_t>::value_type);
        }
        case dt::int64_t: {
            return dims_mult * sizeof(column_info<int64_t>::value_type);
        }
        case dt::uint64_t: {
            return dims_mult * sizeof(column_info<uint64_t>::value_type);
        }
        case dt::float_t: {
            return dims_mult * sizeof(column_info<float>::value_type);
        }
        case dt::double_t: {
            return dims_mult * sizeof(column_info<double>::value_type);
        }
        case dt::fkey_t: {
            return sizeof(DatabaseID);
        }
        case dt::string_t:
        case dt::blob_t: {
            //Data types that are not fixed width should have thrown
            //an exception at the top of this method. This should be
            //unreachable.
            simdb_throw("Unreachable")
            break;
        }
    }

    simdb_throw("Unreachable")
    return 0;
}

}

