// <BlobHelpers> -*- C++ -*-

#pragma once

#include <vector>

/*!
 * \file BlobHelpers.h
 * \brief This file contains utilities related to working
 * with SimDB blobs, which are retrieved from the database
 * as std::vector<char>
 */

namespace simdb {

//! Lightweight "alias" to reinterpret_cast a std::vector<char>
//! to a std::vector<T>. This does not copy any underlying data.
template <typename DestinationT>
struct VectorAlias {
    //! Construct with a reference to your raw char vector
    explicit VectorAlias(const std::vector<char> & src_data) :
        src_(src_data)
    {
        static_assert(
            std::is_scalar<DestinationT>::value and
            std::is_arithmetic<DestinationT>::value,
            "simdb::VectorAlias<T> utility is only supported to 'cast' a "
            "vector<char> to a vector of numeric scalars.               \n"
            "Examples (assume 'raw' is a vector<char>):                 \n"
            "    simdb::VectorAlias<uint16_t> alias(raw);               \n"
            "    simdb::VectorAlias<double> alias2(raw);                \n");

        static_assert(
            sizeof(*this) == sizeof(void*),
            "simdb::VectorAlias<T> should have the size of a void pointer");
    }

    //! Comparison with std::vector<T>
    bool operator==(const std::vector<DestinationT> & rhs) const {
        const auto & lhs = *this;
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (size_t idx = 0; idx < rhs.size(); ++idx) {
            if (lhs[idx] != rhs[idx]) {
                return false;
            }
        }
        return true;
    }

    //! Return the number of points in the "aliased" vector. Say
    //! the original vector<char> had 40 elements in it and this
    //! alias is for vector<double> - this size() method would
    //! return 5. There would be five elements if this same raw
    //! vector were interpreted as a vector of doubles.
    size_t size() const {
        return src_.size() / sizeof(DestinationT);
    }

    //! Same as vector<T>::empty() for any type T
    bool empty() const {
        return src_.empty();
    }

    //! Data access without bounds checking
    const DestinationT & operator[](const size_t idx) const {
        return *reinterpret_cast<const DestinationT*>(
            &src_[idx*sizeof(DestinationT)]);
    }

    //! Data access with bounds checking
    const DestinationT & at(const size_t idx) const {
        return *reinterpret_cast<const DestinationT*>(
            &src_.at(idx*sizeof(DestinationT)));
    }

private:
    const std::vector<char> & src_;
};
  
} // namespace simdb

