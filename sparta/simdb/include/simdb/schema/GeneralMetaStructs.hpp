// <GeneralMetaStructs> -*- C++ -*-

#pragma once

/*!
 * \brief Metaprogramming utilities for use
 * with SimDB schemas.
 */

#include <deque>
#include <forward_list>
#include <list>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace simdb {

//! Base case: is_container<T> is FALSE
template <typename>
struct is_container : std::false_type {};

//! Vectors
template <typename ColumnT>
struct is_container<std::vector<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Lists
template <typename ColumnT>
struct is_container<std::list<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Forward lists
template <typename ColumnT>
struct is_container<std::forward_list<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Sets
template <typename ColumnT>
struct is_container<std::set<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Unordered sets
template <typename ColumnT>
struct is_container<std::unordered_set<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Deques
template <typename ColumnT>
struct is_container<std::deque<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

//! Base case: is_initializer_list<T> is FALSE
template <typename>
struct is_initializer_list : std::false_type {};

//! Initializer lists for supported types: {"a","b"} / {4,5,3,6} / etc.
template <typename ColumnT>
struct is_initializer_list<std::initializer_list<ColumnT>> : std::true_type {};

//! Base case: is_contiguous<T> is FALSE
template <typename>
struct is_contiguous : std::false_type {};

//! Vectors are the only supported contiguous types for blob retrieval
template <typename ColumnT>
struct is_contiguous<std::vector<ColumnT>> : std::true_type {
    using value_type = ColumnT;
};

}

