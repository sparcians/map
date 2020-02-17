// <Event.h> -*- C++ -*-


/**
 * \file   Traits.hpp
 *
 * \brief File that defines compile-time queries on data types.
 * Extends c++11's
 */

#ifndef __TRAITS_H__
#define __TRAITS_H__

#include <type_traits>

template <typename T>
class sparta_traits
{
    typedef char yep;
    typedef long nope;

    // Determine if the data type has a get() method
    template <typename C> static yep hasGetMethod (decltype(&C::get) );
    template <typename C> static nope hasGetMethod (...);

    // Determine if the data type should use -> or . accessors
    template <typename C> static yep hasPointerOperator( decltype(&C::operator->) ) ;
    template <typename C> static nope hasPointerOperator(...);

    // Determine if the data type has an iterator type defined
    template <typename C> static typename C::iterator hasIterator(int);
    template <typename> static void hasIterator(...);

public:

    //! Has a value of true if T responds to -> operator
    enum { is_smartptr  = sizeof(hasGetMethod<T>(0)) == sizeof(yep) };
    enum { stl_smartptr = !std::is_void<decltype(hasPointerOperator<T>(0))>::value };
    enum { stl_iterable = !std::is_void<decltype(hasIterator<T>(0))>::value };

};

/*!
 * \brief getAsPointer Intended to convert what object is given to a pointer type
 * \param obj The object to convert to a pointer
 * \tparam value_type The type of the object
 *
 * This version of the function will evaluate the value_type to see if
 * it's NOT a smart pointer type (like a unique_ptr or a shared_ptr)
 * and will return the given object as an address
 */
template<class value_type>
inline static typename std::enable_if<sparta_traits<value_type>::is_smartptr == false,
                                      const value_type*>::type
getAsPointer(const value_type & obj)
{
    return &obj;
}

/*!
 * \brief getAsPointer Intended to convert what object is given to a pointer type
 * \param obj The object to convert to a pointer
 * \tparam value_type The type of the object
 *
 * This version of the function will evaluate the value_type to see if
 * it IS a smart pointer type (like a unique_ptr or a shared_ptr) and
 * will return the given object as an address via the smart pointer's
 * \p get() method.
 */
template<class value_type>
inline static typename std::enable_if<sparta_traits<value_type>::is_smartptr == true,
                                      const typename value_type::element_type*>::type
getAsPointer(const value_type & obj)
{
    return obj.get();
}

// template<class value_type>
// inline static typename std::enable_if<std::is_pointer<value_type>::value == true,
//                                       const value_type>::type
// getAsPointer(const value_type & obj)
// {
//     return obj;
// }

#endif
