#pragma once

#include <type_traits>
#include <vector>
#include <string>
#include <cstdint>

namespace sparta::extensions {

template <typename T>
struct is_supported_scalar : std::false_type {};

template <> struct is_supported_scalar<int8_t>      : std::true_type {};
template <> struct is_supported_scalar<uint8_t>     : std::true_type {};
template <> struct is_supported_scalar<int16_t>     : std::true_type {};
template <> struct is_supported_scalar<uint16_t>    : std::true_type {};
template <> struct is_supported_scalar<int32_t>     : std::true_type {};
template <> struct is_supported_scalar<uint32_t>    : std::true_type {};
template <> struct is_supported_scalar<int64_t>     : std::true_type {};
template <> struct is_supported_scalar<uint64_t>    : std::true_type {};
template <> struct is_supported_scalar<double>      : std::true_type {};
template <> struct is_supported_scalar<std::string> : std::true_type {};
template <> struct is_supported_scalar<bool>        : std::true_type {};

template <typename T, size_t Depth>
struct is_supported_impl : is_supported_scalar<T> {};

template <typename T, typename Alloc, size_t Depth>
struct is_supported_impl<std::vector<T, Alloc>, Depth>
    : std::conditional_t<
          (Depth < 2),
          is_supported_impl<T, Depth + 1>,
          std::false_type
      > {};

template <typename T>
struct is_supported : is_supported_impl<T, 0> {};

} // namespace sparta::extensions
