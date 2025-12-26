// <Port> -*- C++ -*-


/**
 * \file   ValidValue.hpp
 *
 * \brief  File that defines a ValidValue
 */

#pragma once

#include <iostream>
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
namespace utils
{

    /**
     * \class ValidValue
     * \brief Provides a wrapper around a value to ensure that the value is assigned
     */
    template <typename T>
    class ValidValue
    {
    public:
        //! Convenient typedef for the value type
        typedef T value_type;

        //! Construct with no validity (not valid, uninitialized)
        ValidValue() :
            valid_(false),
            value_()
        {}

        /**
         * \brief Construct with a valid starting value
         * \param args The value to start with
         *
         * This constructor will _not_ be used when constructing with
         * another sparta::ValidValue.  The const/non-const copy
         * constructors below should/will be used instead
         */
        template<typename ...ArgsT>
        ValidValue(ArgsT&& ...args) :
            valid_(true),
            value_(std::forward<ArgsT>(args)...)
        {}

        //! Allow copies of direct ValidValue -- non-const.  This is
        //! to prevent the variatic template constructor from being
        //! used when the rvalue is another ValidValue
        ValidValue(ValidValue &) = default;

        //! Allow moves
        ValidValue(ValidValue && v) :
            valid_(v.valid_),
            value_(std::move(v.value_))
        {
            v.valid_ = false;
        }

        //! Allow copies
        ValidValue(const ValidValue &) = default;

        //! Allow assignments
        ValidValue & operator=(const ValidValue&) = default;
        ValidValue & operator=(ValidValue&&) = default;

        /**
         * \brief Assignment
         * \param val The value to assign, becomes immediately valid
         * \return The value after assignment
         */
        value_type operator=(const value_type & val) {
            valid_ = true;
            return (value_ = val);
        }

        /**
         * \brief Assignment
         * \param val The value to assign, becomes immediately valid
         * \return The value after assignment
         */
        value_type operator=(value_type && val) {
            valid_ = true;
            return (value_ = std::move(val));
        }

        /**
         * \brief Compare equal
         * \param val The value to compare, asserts if this valid isn't valid
         * \return true if equal
         */
        bool operator==(const value_type & val) const {
            sparta_assert(valid_ == true, "ValidValue is not valid for compare!");
            return (value_ == val);
        }

        /**
         * \brief Compare not equal
         * \param val The value to compare, asserts if this valid isn't valid
         * \return true if not equal
         */
        bool operator!=(const value_type & val) const {
            return !operator==(val);
        }

        /**
         * \brief Compare equal
         * \param val The ValidValue to compare, asserts if this valid isn't valid
         * \return true if equal
         */
        bool operator==(const ValidValue<T> & val) const {
            sparta_assert(valid_ == true, "ValidValue is not valid for compare!");
            return (value_ == val.value_);
        }

        /**
         * \brief Compare not equal
         * \param val The ValidValue to compare, asserts if this valid isn't valid
         * \return true if not equal
         */
        bool operator!=(const ValidValue<T> & val) const {
            return !operator==(val);
        }

        /**
         * \brief Is this value valid
         * \return true if valid
         */
        bool isValid() const {
            return valid_;
        }

        /**
         * \brief Convert the ValidValue to the object it is
         * \return The internal value, asserts if this ValidValue is
         *         NOT valid
         */
        operator const value_type &() const {
            sparta_assert(valid_ == true, "ValidValue is not valid for conversion!");
            return value_;
        }

        /**
         * \brief Convert the ValidValue to the object it is
         *
         * \return The internal value by reference, asserts if this
         *         ValidValue is NOT valid
         */
        operator value_type & () {
            sparta_assert(valid_ == true, "ValidValue is not valid for conversion!");
            return value_;
        }

        /**
         * \brief Get the value - const version
         * \return The internal value, asserts if this ValidValue is NOT valid
         */
        const value_type & getValue() const {
            sparta_assert(valid_ == true, "ValidValue is not valid for getting!");
            return value_;
        }

        /**
         * \brief Get the value
         * \return The internal value by reference, asserts if this
         *         ValidValue is NOT valid
         */
        value_type & getValue() {
            sparta_assert(valid_ == true, "ValidValue is not valid for getting!");
            return value_;
        }

        /**
         * \brief Clear the validity of this object.
         */
        void clearValid() {
            valid_ = false;
        }

        /**
         * \brief Boost::serialization support
         */
        template <typename Archive>
        void serialize(Archive & ar, const unsigned int /* version */) {
            // The operator& here is a reader/writer for the member variable RHS.
            // We should always read and write the valid_ flag, but only do the
            // same for value_ when valid.
            ar & valid_;
            if (valid_)
            {
                ar & value_;
            }
        }

    private:
        bool valid_ = false;  //!< Validity
        value_type value_;    //!< The value
    };

template<class ValidValT>
std::ostream & operator<<(std::ostream & os, const sparta::utils::ValidValue<ValidValT> & vv)
{
    if(vv.isValid()) {
        os << vv.getValue();
    }
    else {
        os << "<invalid ValidValue>";
    }
    return os;
}
}
}
