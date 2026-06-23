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
    // being explicit on their trait types for defining their own
    // iterators. For Sparta, we'll put 'em back.
    template<class category, class T>
    struct IteratorTraits {
        using value_type      = std::remove_cv_t<T>; // Always non-const per standard
        using difference_type = std::ptrdiff_t;
        using pointer         = T*;
        using reference       = T&;
        using iterator_category = category;
    };

}
