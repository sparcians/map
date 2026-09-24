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
    template<class category, class T, bool is_const_iterator>
    struct IteratorTraits {
        using value_type      = std::remove_cv_t<T>; // Always non-const per standard
        using difference_type = std::ptrdiff_t;
        using iterator_category = category;

        private:
            // What underlying type does this iterator reference (adds const for const iterators)
            using referenced_value_type = std::conditional_t<is_const_iterator, std::add_const_t<value_type>, value_type>;

        public:
            using pointer         = std::add_pointer_t<referenced_value_type>;
            using reference       = std::add_lvalue_reference_t<referenced_value_type>;
    };

}
