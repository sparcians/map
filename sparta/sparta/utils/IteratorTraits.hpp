// <IteratorTraits.hpp> -*- C++ -*-

/**
 * \file IteratorTraits.hpp
 * \brief Defines a few handy (and now deprecated) C++ iterator traits
 *
 */
#pragma once

namespace sparta::utils
{
    // C++17 deprecates the `std::iterator` in lieu of developers
    // being explicit on thier trait types for defining their own
    // iterators. For Sparta, we'll put 'em back.
    template<class category, class T>
    struct IteratorTraits {
        using difference_type = long;
        using value_type      = T;
        using pointer         = const T*;
        using reference       = const T&;
        using iterator_category = category;
    };
}
