// <ColumnTypedefs> -*- C++ -*-

#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <string>

namespace simdb {

//! Data types supported by SimDB schemas
enum class ColumnDataType : int8_t {
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    float_t,
    double_t,
    char_t,
    string_t,
    blob_t,
    fkey_t
};

//! From a table's perspective, each column can be uniquely
//! described by its column name and its data type.
using ColumnDescriptor = std::pair<std::string, ColumnDataType>;

//! Blob descriptor used for writing and reading raw bytes
//! to/from the database.
struct Blob {
    const void * data_ptr = nullptr;
    size_t num_bytes = 0;
};

}

