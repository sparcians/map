// <DetectMemberUtils> -*- C++ -*-


/*!
 * \file DetectMemberUtils.hpp
 * \brief Compile-time SFINAE techniques to detect presence of
 * operators, member-fields and methods by name in any class.
 */

#ifndef __SPARTA_UTILS_DETECT_MEMBER_UTILS_H__
#define __SPARTA_UTILS_DETECT_MEMBER_UTILS_H__

#include <iostream>

namespace sparta{
namespace utils{

//! We need to have the capability to detect the presence of
//  an overloaded operator for a type during compile time.
//  This is needed because in order to annotate the Histograms,
//  we need to label each enum constant with their string names.
//  The process of converting enum constants into string is
//  usually done by an overload global operator <<. But we cannot
//  blindly assume that every enum class will have this overloaded.
//  If we assume so, this will lead to compilation failure.
//  This can be detected using SFINAE using Member Detector Idiom.
//  Reference : https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
namespace has_ostream_operator_impl{

    //!Typedef a char array of size one.
    typedef char no;

    //! Typedef a char array of size two.
    typedef char yes[2];

    //! A fallback struct which can create itself
    //  from any object of any type.
    struct fallback{
        template<typename T>
        fallback(const T&);
    };

    //! Declare a dummy << operator which operates on
    //  any fallback object.
    no operator << (std::ostream const&, fallback const&);

    //! If the class does have an << operator overloaded,
    //  then the return type of invoking it has to be a
    //  std::ostream &. In that case, this function will
    //  be invoked.
    yes& test(std::ostream&);

    //! If the class did not have an << operator overloaded
    //  to begin with, it will use the global << operator
    //  defined in this namespace. It will convert itself into
    //  fallback object and then invoke this function. In this
    //  case, the return type in no.
    no& test(no);

    template<typename T>
    struct has_ostream_operator{
        static std::ostream& s;
        static T const& t;

        //! Test to see what happens when we invoke
        //  << operator on an object of type T. The return
        //  value that we get will tell us if this class T
        //  did have an << operator defined for it or not.
        static constexpr bool value {sizeof(test(s << t)) == sizeof(yes)};
    };
}

template<typename T>
struct has_ostream_operator : has_ostream_operator_impl::has_ostream_operator<T>{};
} // namespace utils
} // namespace sparta
#endif
