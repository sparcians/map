// <Schema> -*- C++ -*-

#ifndef __SIMDB_SQLITE_SCHEMA_H__
#define __SIMDB_SQLITE_SCHEMA_H__

#include "simdb/schema/Schema.hpp"

#include <iostream>

namespace simdb {

//! Stream operator used when creating a SQL command from an ostringstream.
inline std::ostream & operator<<(std::ostream & os, const ColumnDataType dtype)
{
    using dt = ColumnDataType;

    switch (dtype) {
        case dt::fkey_t:
        case dt::char_t:
        case dt::int8_t:
        case dt::uint8_t:
        case dt::int16_t:
        case dt::uint16_t:
        case dt::int32_t:
        case dt::uint32_t:
        case dt::int64_t:
        case dt::uint64_t: {
            os << "INT"; break;
        }

        case dt::string_t: {
            os << "TEXT"; break;
        }

        case dt::float_t:
        case dt::double_t: {
            os << "FLOAT"; break;
        }

        case dt::blob_t: {
            os << "BLOB"; break;
        }
    }

    return os;
}

} // namespace simdb

#endif
