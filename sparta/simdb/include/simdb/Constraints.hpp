// <Constraints> -*- C++ -*-

#pragma once

#include "simdb/Errors.hpp"

#include <cstdint>
#include <iostream>

namespace simdb {

//! Constraints enumeration used when building
//! SELECT, UPDATE, and DELETE statements.
enum class constraints : int8_t {
    equal,            // =
    not_equal,        // !=
    greater,          // >
    less,             // <
    greater_equal,    // >=
    less_equal,       // <=
    in_set,           // IN
    not_in_set,       // NOT IN
    INVALID
};

//! Stream operator for constraints enumeration.
inline std::ostream & operator<<(std::ostream & os,
                                 const constraints constraint)
{
    switch (constraint) {
        case constraints::equal:          os << " =  "     ; break;
        case constraints::not_equal:      os << " != "     ; break;
        case constraints::greater:        os << " >  "     ; break;
        case constraints::less:           os << " <  "     ; break;
        case constraints::greater_equal:  os << " >= "     ; break;
        case constraints::less_equal:     os << " <= "     ; break;
        case constraints::in_set:         os << " IN "     ; break;
        case constraints::not_in_set:     os << " NOT IN " ; break;
        case constraints::INVALID:
            throw DBException("Cannot stringify constraints::INVALID");
    }
    return os;
}
    
} // namespace simdb

