// <Port> -*- C++ -*-


/**
 * \file   ValidValue.hpp
 *
 * \brief  File that defines a ValidValue
 */

#ifndef __VALID_VALUE_H__
#define __VALID_VALUE_H__

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
         * \param start The value to start with
         */
        template<typename ...ArgsT>
        ValidValue(ArgsT&& ...args) :
            valid_(true),
            value_(std::forward<ArgsT>(args)...)
        {}

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

// __VALID_VALUE_H__
#endif
