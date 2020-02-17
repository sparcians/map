// <LockedValue> -*- C++ -*-


/**
 * \file   LockedValue.hpp
 *
 * \brief  File that defines a LockedValue
 */

#ifndef __LOCKED_VALUE_H__
#define __LOCKED_VALUE_H__

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta{
namespace utils{
template<typename T>

/**
 * \class LockedValue
 * \brief Provides a wrapper around a value to ensure that once the value
 *        is frozen or locked, it cannot be overwritten again.
 * \code
 * sparta::utils::LockedValue<uint32_t> my_lval;
 * std::cout << my_lval.getValue();
 * my_lval = 16;
 * my_lval = 32;
 * std::cout << my_lval.getValue();
 * my_lval.lock();
 * my_lval.lock();
 * EXPECT_THROW(my_lval = 64);
 * EXPECT_THROW(my_lval.setAndLock(1));
 * sparta::utils::LockedValue<uint16_t> my_lval_2(8);
 * my_lval_2 = 16;
 * my_lval_2.setAndLock(32);
 * EXPECT_THROW(my_lval_2 = 2);
 * my_lval_2.lock();
 * std::cout << my_lval_2 << "\n";
 * \endcode
 */
class LockedValue{
public:
    //! Convenient typedef for the value type
    typedef T value_type;

    //! This is the LockedValue Default Constructor.
    LockedValue() : is_locked_{false}, value_{}{}

    //! This is the LockedValue Constructor with initial value as parameter.
    explicit LockedValue(const T& value) : is_locked_{false}, value_{value}{}

    //! This is the LockedValue Constructor with initial value and lock as parameter.
    LockedValue(const T& value, const bool lock) : is_locked_{lock}, value_{value}{}

    //! Copy-Assignment of a LockedValue to another LockedValue is a deleted function.
    LockedValue<T>& operator = (const LockedValue<T>&) = delete;

    //! This is the assignment operator of the LockedValue class from a given resource.
    //  This asserts if the LockedValue instance is already locked.
    LockedValue<T>& operator = (const T& value){
        sparta_assert(!is_locked_,
                    "LockedValue is already locked and cannot be assigned a new value.");
        value_ = value;
        return *this;
    }

    //! This method assigns a value to LockedValue class and immediately locks it.
    //  This asserts if the LockedValue instance is already locked.
    void setAndLock(const T& value){
        sparta_assert(!is_locked_,
                    "LockedValue is already locked and cannot be set a new value.");
        value_ = value;
        is_locked_ = true;
    }

    //! Lock the LockedValue instance immediately.
    void lock(){
        is_locked_ = true;
    }

    //! Query if LockedValue instance is locked.
    bool isLocked() const{
        return is_locked_;
    }

    //! Get the value of the underlying resource of the LockedValue.
    //  Querying for the value should never throw, be it initialized or not.
    //  This is the const version.
    const value_type& getValue() const{
        return value_;
    }

    //! get the value of the underlying resource of the lockedvalue.
    //  querying for the value should never throw, be it initialized or not.
    //  this is the non-const version.
    value_type& getValue(){
        return value_;
    }

    //! Convert the LockedValue to its underlying resource.
    //  Converting to the value should never throw, be it initialized or not.
    //  This is the const-version.
    operator const value_type& () const{
        return value_;
    }

    //! Convert the LockedValue to its underlying resource.
    //  Converting to the value should never throw, be it initialized or not.
    //  This is the non-const-version.
    operator value_type& (){
        return value_;
    }

    //! Overload the compare equal operator for LockedValue class.
    bool operator == (const value_type& value) const{
        return value_ == value;
    }

    //! Overload the compare not equal operator for LockedValue class.
    bool operator != (const value_type& value) const{
        return !operator == (value);
    }
private:
        bool is_locked_ {false}; //!< Lock state
        value_type value_;       //!< Internal value
};

//! Overload ostream operator for LockedValue class.
template<class T>
std::ostream& operator << (std::ostream& os, const sparta::utils::LockedValue<T>& value){
    os << value.getValue();
    return os;
} // LockedValue
} // namespace utils
} // namespace sparta
#endif
