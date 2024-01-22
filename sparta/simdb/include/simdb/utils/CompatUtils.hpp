// <CompatUtils> -*- C++ -*-

#pragma once

#include <type_traits>

namespace simdb {
namespace utils {

//! \brief Replacement for std::is_pod, which was deprecated in C++20

template<typename T>
struct is_pod {
    static constexpr bool value = std::is_trivial<T>::value
                               && std::is_standard_layout<T>::value;
};

} // namespace utils
} // namespace simdb
