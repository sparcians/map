// <BoundedValue.hpp>  -*- C++ -*-


/**
 * \file BoundedValue.hpp
 *
 * \brief Implementation of the Bounded Value class that is
 *        templated based on a certain integral or floating-point
 *        data-type and functions within one or more non-overlapping
 *        operating range.
 */

#ifndef __BOUNDED_VALUE_H__
#define __BOUNDED_VALUE_H__

#include <iostream>
#include <limits>
#include <algorithm>
#include <type_traits>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MetaStructs.hpp"

namespace sparta{
namespace utils{
template<typename T>
class BoundedValue{
public:
    using value_type = T;
    template<typename U>
    using pure_value_t = MetaStruct::decay_t<U>;

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  The 4 variables are BV type, initial value, upper bound and lower bound.
     *  Case when all 4 variables are integral and of the same sign(signed).
     *  Example:
     *  \code{.cpp}
     *  int data = 15;
     *  BoundedValue<int> bv(data, -20, 20);
     *  int lower_bound = -20;
     *  int upper_bound = 20;
     *  \endcode
     *
     *  Case when all 4 variables are integral and of the same sign(unsigned).
     *  The 4 variables are BV type, initial value, upper bound and lower bound.
     *  Example:
     *  \code{.cpp}
     *  uint32_t data = 15;
     *  BoundedValue<uint64_t> bv(data, 0ull, 20ull);
     *  uint16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<uintmax_t> bv(data, lower_bound, upper_bound);
     *  BoundedValue<int> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  MetaStruct::all_same_sign<T, U, V, W>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        sparta_assert(upper_bound > lower_bound);
        // TODO how can you be a value lower than the min() value for a type?
        //sparta_assert(lower_bound >= std::numeric_limits<T>::min());
        sparta_assert(upper_bound <= std::numeric_limits<T>::max());

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  The 4 variables are BV type, initial value, upper bound and lower bound.
     *  Case when BV type, lower and upper bounds are unsigned but initial value is signed.
     *  Example:
     *  \code{.cpp}
     *  int32_t data = 15;
     *  BoundedValue<uint64_t> bv(data, 0ull, 20ull);
     *  uint16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<uintmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_unsigned<pure_value_t<T>>::value        and
                                                  std::is_signed<pure_value_t<U>>::value          and
                                                  std::is_unsigned<pure_value_t<V>>::value        and
                                                  std::is_unsigned<pure_value_t<W>>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        sparta_assert(value >= 0);
        sparta_assert(static_cast<uintmax_t>(lower_bound) >=
                    static_cast<uintmax_t>(std::numeric_limits<T>::min()));
        sparta_assert(static_cast<uintmax_t>(upper_bound) <=
                    static_cast<uintmax_t>(std::numeric_limits<T>::max()));
        sparta_assert(static_cast<uintmax_t>(upper_bound) > static_cast<uintmax_t>(lower_bound));

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case when BV type, and initial value is unsigned but one of the bounds are signed.
     *  Example:
     *  \code{.cpp}
     *  uint32_t data = 15;
     *  BoundedValue<uint64_t> bv(data, 0ull, 20);
     *  int16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<uintmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_unsigned<pure_value_t<T>>::value        and
                                                  std::is_unsigned<pure_value_t<U>>::value        and
                                                  (std::is_signed<pure_value_t<V>>::value         or
                                                  std::is_signed<pure_value_t<W>>::value)>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        sparta_assert(lower_bound >= 0 and
                    static_cast<uintmax_t>(lower_bound) >=
                    static_cast<uintmax_t>(std::numeric_limits<T>::min()));
        sparta_assert(upper_bound >= 0 and
                    static_cast<uintmax_t>(upper_bound) <=
                    static_cast<uintmax_t>(std::numeric_limits<T>::max()));
        sparta_assert(static_cast<uintmax_t>(upper_bound) > static_cast<uintmax_t>(lower_bound));

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case when BV type and one of the bounds is unsigned but initial value and one of the bounds are signed.
     *  \code{.cpp}
     *  Example:
     *  int32_t data = 15;
     *  BoundedValue<uint64_t> bv(data, 0ull, 20);
     *  int16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<uintmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_unsigned<pure_value_t<T>>::value        and
                                                  std::is_signed<pure_value_t<U>>::value          and
                                                  (std::is_signed<pure_value_t<V>>::value         or
                                                  std::is_signed<pure_value_t<W>>::value)>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        sparta_assert(value >= 0);
        sparta_assert(lower_bound >= 0 and
                    static_cast<uintmax_t>(lower_bound) >=
                    static_cast<uintmax_t>(std::numeric_limits<T>::min()));
        sparta_assert(upper_bound >= 0 and
                    static_cast<uintmax_t>(upper_bound) <=
                    static_cast<uintmax_t>(std::numeric_limits<T>::max()));
        sparta_assert(static_cast<uintmax_t>(upper_bound) > static_cast<uintmax_t>(lower_bound));

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case when BV type, lower and upper bounds are signed but initial value is unsigned.
     *  Example:
     *  \code{.cpp}
     *  uint32_t data = 15;
     *  BoundedValue<int64_t> bv(data, -10, 20);
     *  int16_t lower_bound = 20;
     *  int32_t upper_bound = 40;
     *  BoundedValue<intmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_signed<pure_value_t<T>>::value          and
                                                  std::is_unsigned<pure_value_t<U>>::value        and
                                                  std::is_signed<pure_value_t<V>>::value          and
                                                  std::is_signed<pure_value_t<W>>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        sparta_assert(static_cast<intmax_t>(lower_bound) >= static_cast<intmax_t>(std::numeric_limits<T>::min()));
        sparta_assert(static_cast<intmax_t>(upper_bound) <= static_cast<intmax_t>(std::numeric_limits<T>::max()));
        sparta_assert(static_cast<intmax_t>(upper_bound) > static_cast<intmax_t>(lower_bound));
        sparta_assert(static_cast<uintmax_t>(value) <= static_cast<uintmax_t>(std::numeric_limits<T>::max()));

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case when BV type and initial value is signed but one of the bounds is unsigned.
     *  Example:
     *  \code{.cpp}
     *  int32_t data = 15;
     *  BoundedValue<int64_t> bv(data, -10, 20ull);
     *  int16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<intmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_signed<pure_value_t<T>>::value          and
                                                  std::is_signed<pure_value_t<U>>::value          and
                                                  (std::is_unsigned<pure_value_t<V>>::value       or
                                                  std::is_unsigned<pure_value_t<W>>::value)>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        if(lower_bound < 0){
            sparta_assert(static_cast<intmax_t>(lower_bound) >=
                        static_cast<intmax_t>(std::numeric_limits<T>::min()));
        }
        if(upper_bound > 0){
            sparta_assert(static_cast<uintmax_t>(upper_bound) <=
                        static_cast<uintmax_t>(std::numeric_limits<T>::max()));
        }
        if(upper_bound > 0 and lower_bound > 0){
            sparta_assert(static_cast<uintmax_t>(upper_bound) > static_cast<uintmax_t>(lower_bound));
        }

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case when BV type is signed but initial value and one of the bounds is unsigned.
     *  Example:
     *  \code{.cpp}
     *  uint32_t data = 15;
     *  BoundedValue<int64_t> bv(data, -10, 20ull);
     *  int16_t lower_bound = 20;
     *  uint32_t upper_bound = 40;
     *  BoundedValue<intmax_t> bv(data, lower_bound, upper_bound);
     *  \endcode
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<MetaStruct::all_are_integral<T, U, V, W>::value and
                                                  std::is_signed<pure_value_t<T>>::value          and
                                                  std::is_unsigned<pure_value_t<U>>::value        and
                                                  (std::is_unsigned<pure_value_t<V>>::value       or
                                                  std::is_unsigned<pure_value_t<W>>::value)>* = 0) :
        value_{static_cast<pure_value_t<T>>(value)},
        lower_bound_{static_cast<pure_value_t<T>>(lower_bound)},
        upper_bound_{static_cast<pure_value_t<T>>(upper_bound)}{
        // Sanity checks.
        if(lower_bound < 0){
            sparta_assert(static_cast<intmax_t>(lower_bound) >=
                        static_cast<intmax_t>(std::numeric_limits<T>::min()));
        }
        if(upper_bound > 0){
            sparta_assert(static_cast<uintmax_t>(upper_bound) <=
                        static_cast<uintmax_t>(std::numeric_limits<T>::max()));
        }
        if(upper_bound > 0 and lower_bound > 0){
            sparta_assert(static_cast<uintmax_t>(upper_bound) > static_cast<uintmax_t>(lower_bound));
        }

        // Range check.
        checkRange_(value);
    }

    /**
     * \class BoundedValue
     * \brief Parameterized Constructor from related integral arguments.
     *  Case which will immediately assert on compilation.
     *  If we are creating a BV of type integral, then none of initial_value,
     *  lower_bound or upper_bound can be non-integral type.
     *  \param value The value to assign
     *  \param lower_bound The lower bound of this BoundedValue
     *  \param upper_bound The upper bound of this BoundedValue
     */
    template<typename U, typename V = T, typename W = T>
    explicit BoundedValue(U value,
                          V lower_bound = std::numeric_limits<T>::min(),
                          W upper_bound = std::numeric_limits<T>::max(),
                          MetaStruct::enable_if_t<std::is_integral<pure_value_t<T>>::value        and
                                                  (std::is_floating_point<pure_value_t<U>>::value or
                                                   std::is_floating_point<pure_value_t<V>>::value or
                                                   std::is_floating_point<pure_value_t<W>>::value)>* = 0){
        (void) value;
        (void) lower_bound;
        (void) upper_bound;

        static_assert(!std::is_floating_point<pure_value_t<U>>::value,
                      "Cannot assign a floating-point limit to integral BoundedValue.");
        static_assert(!std::is_floating_point<pure_value_t<V>>::value,
                      "Cannot assign a floating-point limit to integral BoundedValue.");
        static_assert(!std::is_floating_point<pure_value_t<W>>::value,
                      "Cannot assign a floating-point limit to integral BoundedValue.");
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from related integral BV of same sign.
     * \param rhs The other BoundedValue object to construct from.
     */
    template<typename U>
    BoundedValue(const BoundedValue<U>& rhs,
                 MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                                         std::is_signed<pure_value_t<T>>::value ==
                                         std::is_signed<pure_value_t<U>>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(rhs.getValue())},
        lower_bound_{static_cast<pure_value_t<T>>(rhs.getLowerBound())},
        upper_bound_{static_cast<pure_value_t<T>>(rhs.getUpperBound())}{

        // Sanity checks.
        sparta_assert(rhs.getLowerBound() >= std::numeric_limits<T>::min());
        sparta_assert(rhs.getUpperBound() <= std::numeric_limits<T>::max());
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from related unsigned integral BV to signed BV.
     * \param rhs The other BoundedValue object to construct from.
     */
    template<typename U>
    BoundedValue(const BoundedValue<U>& rhs,
                 MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                                         std::is_signed<pure_value_t<T>>::value and
                                         std::is_unsigned<pure_value_t<U>>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(rhs.getValue())},
        lower_bound_{static_cast<pure_value_t<T>>(rhs.getLowerBound())},
        upper_bound_{static_cast<pure_value_t<T>>(rhs.getUpperBound())}{

        // Sanity check.
        sparta_assert(rhs.getUpperBound() <= static_cast<U>(std::numeric_limits<T>::max()));
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from related signed integral BV to unsigned BV.
     * \param rhs The other BoundedValue object to construct from.
     */
    template<typename U>
    BoundedValue(const BoundedValue<U>& rhs,
                 MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                                         std::is_unsigned<pure_value_t<T>>::value and
                                         std::is_signed<pure_value_t<U>>::value>* = 0) :
        value_{static_cast<pure_value_t<T>>(rhs.getValue())},
        lower_bound_{static_cast<pure_value_t<T>>(rhs.getLowerBound())},
        upper_bound_{static_cast<pure_value_t<T>>(rhs.getUpperBound())}{

        // Sanity checks.
        sparta_assert(rhs.getValue() >= 0);
        sparta_assert(rhs.getLowerBound() >= 0 and
                    static_cast<T>(rhs.getLowerBound()) >= std::numeric_limits<T>::min());
        sparta_assert(rhs.getUpperBound() >= 0 and
                    static_cast<T>(rhs.getUpperBound()) <= std::numeric_limits<T>::max());
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from floating-point BV to integral BV.
     * \param rhs The other BoundedValue object to construct from.
     */
    template<typename U>
    BoundedValue(const BoundedValue<U>& rhs,
                 MetaStruct::enable_if_t<std::is_floating_point<pure_value_t<U>>::value and
                                         std::is_integral<pure_value_t<T>>::value>* = 0){
        (void)rhs;
        static_assert(std::is_floating_point<pure_value_t<T>>::value,
                      "Cannot assign a floating-point BoundedValue to integral BoundedValue.");
    }

    /**
     * \class BoundedValue
     * \brief Copy Assignment Operator from related integral BV of same sign.
     * \param rhs The other BoundedValue object to assign from.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                            std::is_signed<pure_value_t<T>>::value ==
                            std::is_signed<pure_value_t<U>>::value,
                            BoundedValue<pure_value_t<T>>&>
    operator = (const BoundedValue<U>& rhs){
        // Sanity checks.
        sparta_assert(rhs.getLowerBound() >= std::numeric_limits<T>::min());
        sparta_assert(rhs.getUpperBound() <= std::numeric_limits<T>::max());

        value_ = static_cast<pure_value_t<T>>(rhs.getValue());
        lower_bound_ = static_cast<pure_value_t<T>>(rhs.getLowerBound());
        upper_bound_ = static_cast<pure_value_t<T>>(rhs.getUpperBound());
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from related unsigned integral BV to signed BV.
     * \param rhs The other BoundedValue object to assign from.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                            std::is_signed<pure_value_t<T>>::value and
                            std::is_unsigned<pure_value_t<U>>::value,
                            BoundedValue<pure_value_t<T>>&>
    operator = (const BoundedValue<U>& rhs){
        // Sanity checks.
        sparta_assert(rhs.getUpperBound() <= static_cast<U>(std::numeric_limits<T>::max()));

        value_ = static_cast<pure_value_t<T>>(rhs.getValue());
        lower_bound_ = static_cast<pure_value_t<T>>(rhs.getLowerBound());
        upper_bound_ = static_cast<pure_value_t<T>>(rhs.getUpperBound());
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Copy Constructor from related signed integral BV to unsigned BV.
     * \param rhs The other BoundedValue object to assign from.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value and
                            std::is_signed<pure_value_t<U>>::value and
                            std::is_unsigned<pure_value_t<T>>::value,
                            BoundedValue<pure_value_t<T>>&>
    operator = (const BoundedValue<U>& rhs){
        // Sanity checks.
        sparta_assert(rhs.getValue() >= 0);
        sparta_assert(rhs.getLowerBound() >= 0 and
                    static_cast<T>(rhs.getLowerBound()) >= std::numeric_limits<T>::min());
        sparta_assert(rhs.getUpperBound() >= 0 and
                    static_cast<T>(rhs.getUpperBound()) <= std::numeric_limits<T>::max());

        value_ = static_cast<pure_value_t<T>>(rhs.getValue());
        lower_bound_ = static_cast<pure_value_t<T>>(rhs.getLowerBound());
        upper_bound_ = static_cast<pure_value_t<T>>(rhs.getUpperBound());
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Copy Assignment Operator for floating-point BV to integral BV.
     * \param rhs The other BoundedValue object to assign from.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_floating_point<pure_value_t<U>>::value and
                            std::is_integral<pure_value_t<T>>::value,
                            void>
    operator = (const BoundedValue<U>& rhs){
        (void)rhs;
        static_assert(std::is_floating_point<pure_value_t<T>>::value,
                      "Cannot assign a floating-point BoundedValue to integral BoundedValue.");
    }

    /**
     * \class BoundedValue
     * \brief Assignment operator for integral arguments to related BV.
     * \param value The value to assign to this BoundedValue.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value,
                            BoundedValue<pure_value_t<T>>&>
    operator = (U value){
        checkRange_(value);
        value_ = static_cast<pure_value_t<T>>(value);
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Assignment operator for floating-point arguments to integral BV.
     * \param value The value to assign to this BoundedValue.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_floating_point<pure_value_t<U>>::value and
                            std::is_integral<pure_value_t<T>>::value,
                            void>
    operator = (U value){
        (void)value;
        static_assert(std::is_floating_point<pure_value_t<T>>::value,
                      "Cannot assign a floating-point number to integral BoundedValue.");
    }

    /**
     * \class BoundedValue
     * \brief Overload pre-increment operator.
     */
    BoundedValue<T>& operator++(){
        sparta_assert(value_ < upper_bound_);
        checkRange_(value_ + 1);
        ++value_;
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Overload post-increment operator.
     */
    BoundedValue<T> operator++(int){
        BoundedValue<T> copy(*this);
        ++(*this);
        return copy;
    }

    /**
     * \class BoundedValue
     * \brief Overload pre-decrement operator.
     */
    BoundedValue<T>& operator--(){
        sparta_assert(value_ > lower_bound_);
        checkRange_(value_ - 1);
        --value_;
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Overload post-decrement operator.
     */
    BoundedValue<T> operator--(int){
        BoundedValue<T> copy(*this);
        --(*this);
        return copy;
    }

    /**
     * \class BoundedValue
     * \brief Overload shorthand += operator.
     * \param value The value to add.
     */
    template<typename U = T>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value, BoundedValue<T>&>
    operator+=(U value){
        if(value >= 0){
            const auto remaining = upper_bound_ - value_;
            sparta_assert(static_cast<uintmax_t>(remaining) >= static_cast<uintmax_t>(value),
                        "Adding the right hand side value would violate the upper-bound of this BV.");
            value_ += value;
            return *this;
        }
        const auto remaining = value_ - lower_bound_;
        sparta_assert(static_cast<uintmax_t>(-value) <= static_cast<uintmax_t>(remaining),
                    "Adding the right hand side value would violate the lower-bound of this BV.");
        value_ += value;
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Overload shorthand -= operator.
     * \param value The value to subtract.
     */
    template<typename U = T>
    MetaStruct::enable_if_t<std::is_integral<pure_value_t<U>>::value, BoundedValue<T>&>
    operator-=(U value){
        if(value >= 0){
            const auto remaining = value_ - lower_bound_;
            sparta_assert(static_cast<uintmax_t>(remaining) >= static_cast<uintmax_t>(value),
                        "Deducting the right hand side value would violate the lower-bound of this BV.");
            value_ -= value;
            return *this;
        }
        const auto remaining = upper_bound_ - value_;
        sparta_assert(static_cast<uintmax_t>(-value) <= static_cast<uintmax_t>(remaining),
                    "Deducting the right hand side value would violate the upper-bound of this BV.");
        value_ -= value;
        return *this;
    }

    /**
     * \class BoundedValue
     * \brief Get the value - const version.
     * \return The internal value.
     */
    const value_type getValue() const{
        return value_;
    }

    /**
     * \class BoundedValue
     * \brief Get the value - non-const version.
     * \return The internal value.
     */
    value_type getValue(){
        return value_;
    }

    /**
     * \class BoundedValue
     * \brief Return the lower bound value, const version.
     * \return The lower bound value.
     */
    const value_type getLowerBound() const{
        return lower_bound_;
    }

    /**
     * \class BoundedValue
     * \brief Return the lower bound value, non-const version.
     * \return The lower bound value.
     */
    value_type getLowerBound(){
        return lower_bound_;
    }

    /**
     * \class BoundedValue
     * \brief Return the upper bound value, const version.
     * \return The upper bound value.
     */
    const value_type getUpperBound() const{
        return upper_bound_;
    }

    /**
     * \class BoundedValue
     * \brief Return the upper bound value, non-const version.
     * \return The upper bound value.
     */
    value_type getUpperBound(){
        return upper_bound_;
    }

    /**
     * \class BoundedValue
     * \brief Convert the BoundedValue into underlying resource, const version.
     * \return The internal value.
     */
    operator const value_type& () const{
        return value_;
    }

    /**
     * \class BoundedValue
     * \brief Convert the BoundedValue into underlying resource, non-const version.
     * \return The internal value.
     */
    operator value_type& (){
        return value_;
    }
private:
    T value_;
    T lower_bound_;
    T upper_bound_;

    /**
     * \class BoundedValue
     * \brief Method to handle sign and check range of rhs value.
     * \param value The value to validate.
     */
    template<typename U>
    void checkRange_(const U & value) const{
        checkLowerBound_<U>(value);
        checkUpperBound_<U>(value);
    }

    /**
     * \class BoundedValue
     * \brief Method to check lower bound validity.
     *  Case when rhs value and this value is of same sign.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_signed<pure_value_t<U>>::value ==
                            std::is_signed<pure_value_t<T>>::value, void>
    checkLowerBound_(const U& value) const{
        sparta_assert(value >= lower_bound_);
    }

    /**
     * \class BoundedValue
     * \brief Method to check lower bound validity.
     *  Case when rhs value is unsigned but this value is signed.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_unsigned<pure_value_t<U>>::value and
                            std::is_signed<pure_value_t<T>>::value, void>
    checkLowerBound_(const U& value) const{
        if(lower_bound_ >= 0){
            sparta_assert(value >= static_cast<uintmax_t>(lower_bound_));
        }
    }

    /**
     * \class BoundedValue
     * \brief Method to check lower bound validity.
     *  Case when rhs value is signed but this value is unsigned.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_signed<pure_value_t<U>>::value and
                            std::is_unsigned<pure_value_t<T>>::value, void>
    checkLowerBound_(const U& value) const{
        sparta_assert((value >= 0) && (static_cast<uintmax_t>(value) >= lower_bound_));
    }

    /**
     * \class BoundedValue
     * \brief Method to check upper bound validity.
     *  Case when rhs value and this value is of same sign.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_signed<pure_value_t<U>>::value ==
                            std::is_signed<pure_value_t<T>>::value, void>
    checkUpperBound_(const U& value) const{
        sparta_assert(value <= upper_bound_);
    }

    /**
     * \class BoundedValue
     * \brief Method to check upper bound validity.
     *  Case when rhs value is unsigned but this value is signed.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_unsigned<pure_value_t<U>>::value and
                            std::is_signed<T>::value, void>
    checkUpperBound_(const U& value) const{
        sparta_assert((upper_bound_ >= 0) && (value <= static_cast<uintmax_t>(upper_bound_)));
    }

    /**
     * \class BoundedValue
     * \brief Method to check upper bound validity.
     *  Case when rhs value is signed but this value is unsigned.
     * \param value The value to validate.
     */
    template<typename U>
    MetaStruct::enable_if_t<std::is_signed<pure_value_t<U>>::value and
                            std::is_unsigned<pure_value_t<T>>::value, void>
    checkUpperBound_(const U& value) const{
        sparta_assert((value >= 0) && (static_cast<uintmax_t>(value) <= upper_bound_));
    }
};

/**
 * \class BoundedValue
 * \brief Overloading of ostream operator.
 * \param os A reference to the ostream instance.
 * \param data The BoundedValue to stringize.
 * \return Ostream instance containing the stringified BoundedValue.
 */
template<typename T>
inline std::ostream & operator << (std::ostream & os,
                                   const sparta::utils::BoundedValue<T> & data){
    os << data.getValue();
    return os;
}
}
}
#endif
