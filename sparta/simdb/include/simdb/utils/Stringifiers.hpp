// <Stringifiers> -*- C++ -*-

#pragma once

#include "simdb/schema/GeneralMetaStructs.hpp"

#include <iomanip>
#include <limits>
#include <sstream>

namespace simdb {

//! Stringify scalar numbers for SQL statement creation
template <typename ColumnT>
typename std::enable_if<
    !std::is_same<typename std::decay<ColumnT>::type, const char*>::value and
    std::is_trivial<ColumnT>::value, std::string>::type
stringify(const ColumnT val)
{
    std::ostringstream oss;
    constexpr auto digits10 = std::numeric_limits<ColumnT>::max_digits10;
    oss << std::setprecision(digits10);
    oss << val;
    return oss.str();
}

//! Stringify std::string's for SQL statement creation
template <typename ColumnT>
typename std::enable_if<std::is_same<ColumnT, std::string>::value, std::string>::type
stringify(const ColumnT & val)
{
    std::ostringstream oss;
    oss << "'" << val << "'";
    return oss.str();
}

//! Stringify const char*'s for SQL statement creation
template <typename ColumnT>
typename std::enable_if<
    std::is_same<typename std::decay<ColumnT>::type, const char*>::value, std::string>::type
stringify(const ColumnT val)
{
    std::ostringstream oss;
    oss << "'" << val << "'";
    return oss.str();
}

//! Stringify STL containers
template <typename ColumnT>
typename std::enable_if<is_container<ColumnT>::value, std::string>::type
stringify(const ColumnT & val)
{
    auto begin = val.begin();
    auto end = val.end();
    const auto num_els = std::distance(begin, end);

    if (num_els == 0) {
        return "";
    }

    if (num_els == 1) {
        return "(" + stringify(*begin) + ")";
    }

    std::ostringstream oss;
    oss << "(";
    for (long idx = 0; idx < num_els-1; ++idx) {
        oss << stringify(*begin) << ",";
        ++begin;
    }
    oss << stringify(*begin) << ")";
    return oss.str();
}

//! Stringifier std::initializer_list<T>
template <typename ColumnT>
typename std::enable_if<is_initializer_list<ColumnT>::value, std::string>::type
stringify(ColumnT val)
{
    auto begin = val.begin();
    auto end = val.end();
    const size_t num_els = std::distance(begin, end);

    if (num_els == 0) {
        return "";
    }

    if (num_els == 1) {
        return "(" + stringify(*begin) + ")";
    }

    std::ostringstream oss;
    oss << "(";
    for (size_t idx = 0; idx < num_els-1; ++idx) {
        oss << stringify(*begin) << ",";
        ++begin;
    }
    oss << stringify(*begin) << ")";
    return oss.str();
}

} // namespace simdb

