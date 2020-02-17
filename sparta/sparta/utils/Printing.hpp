// <Printing> -*- C++ -*-


#ifndef __PRINTING_H__
#define __PRINTING_H__

#include <iostream>
#include <string>
#include <ostream>
#include <vector>
#include <sstream>

#include "sparta/utils/SpartaException.hpp"

/*!
 * \file Printing.hpp
 * \brief Helpers for printing and populating vectors
 */
namespace sparta
{
namespace utils
{

    /*!
     * \brief Numeric display options used by Parameter printing routines
     * \warning When Adding fields, sparta::setIOSFlags must also be updated.
     */
    enum DisplayBase {
        BASE_DEC = 0, //! Decimal display
        BASE_HEX = 1, //! Hex display
        BASE_OCT = 2  //! Octal display
    };

    /*!
     * \brief Configure an ios stream to use given numeric base from
     * sparta::DisplayBase
     * \param stream Stream reference to configure with given sparta::DisplayBase
     * base.
     * \param base sparta::DisplayBase describing the new formatting.
     * \note Also enables showbase (e.g. '0xf' instead of 'f') and boolalpha
     * (true instead of 1 for bools).
     * \throw SpartaException if base is not a supported type.
     */
    inline std::ios_base::fmtflags setIOSFlags(std::ios_base& stream, DisplayBase base) {
        std::ios_base::fmtflags f;
        switch(base){
        case BASE_DEC:
            f = stream.setf(std::ios::dec, std::ios::basefield);
            break;
        case BASE_HEX:
            f = stream.setf(std::ios::hex, std::ios::basefield);
            break;
        case BASE_OCT:
            f = stream.setf(std::ios::oct, std::ios::basefield);
            break;
        default:
            SpartaException ex("Unsupported SPARTA display flag: ");
            ex << base;
            throw ex;
        };
        stream.setf(std::ios::showbase | std::ios::boolalpha);
        return f;
    }

    /*!
     * \brief Converting a vector of intrinsic types (and std::string) to a string
     * \tparam T type of vector to stringize
     * \param v vector to print (value_type=T)
     * \param base Base of displayed integers
     * \param string_quote Quote sequence for printing strings; defaults to no quoting
     * \return std::string of form "[el0, el1, ..., elN-1]"
     * \note This is used internally by Parameter for self-printing, but can
     * be used to print vectors for any purpose
     *
     * \example
     * \code
     * std::vector<int32_t> v; v.push_back(1); v.push_back(2);
     * std::cout << sparta::stringize_value(v, sparta::BASE_HEX) << std::endl;
     * \endcode
     */
    template <class T>
    inline std::string stringize_value(const std::vector<T>& v, DisplayBase base=BASE_DEC,
                                       const std::string& string_quote="") {
        std::stringstream out;
        out << "[";
        typename std::vector<T>::const_iterator itr = v.begin();
        if(itr != v.end()){
            std::ios_base::fmtflags old = setIOSFlags(out, base);
            out << stringize_value((T)(*itr++), base, string_quote);
            out.setf(old);
            for(; itr != v.end(); ++itr){
                out << ", ";
                old = setIOSFlags(out, base);
                out << stringize_value((T)(*itr), base, string_quote);
                out.setf(old);
            }
        }
        out << "]";
        return out.str();
    }

    /*!
     * \brief Converting a stl pair of intrinsic types (and std::string) to a
     * string
     * \tparam T first type in pair to stringize
     * \tparam U second type in pair sto stringize
     * \param p pair to print (of type <T,U>)
     * \param base Base of displayed integers
     * \param string_quote Quote sequence for printing strings; defaults to no quoting
     * \return std::string of form "first:second"
     */
    template <class T, class U>
    inline std::string stringize_value(const std::pair<T,U>& p, DisplayBase base=BASE_DEC,
                                       const std::string& string_quote="") {
        std::stringstream out;
        std::ios_base::fmtflags old = setIOSFlags(out, base);
        out << stringize_value(p.first, base, string_quote) << ":"
            << stringize_value(p.second, base, string_quote);
        out.setf(old);
        return out.str();
    }

    /*!
     * \brief Overload of stringize_value that supports intrinisic types
     * as well as std::string.
     * \see sparta::stringize_value
     *
     * Invoking stringize_value with a non-std::vector type will invoke this
     * method.
     */
    template <class T>
    inline std::string stringize_value(const T& o, DisplayBase base=BASE_DEC,
                                       const std::string& string_quote="") {
        (void) string_quote;
        std::stringstream out;
        std::ios_base::fmtflags old = setIOSFlags(out, base);
        out << o;
        out.setf(old);
        return out.str();
    }

    //! Specialization for printing strings
    template <>
    inline std::string stringize_value<std::string>(const std::string& o, DisplayBase base,
                                                    const std::string& string_quote) {
        (void)base;
        std::stringstream out;
        out << string_quote << o << string_quote;
        return out.str();
    }

    //! Specialization for populating STL vector of string with char*s.
    inline std::vector<std::string>& operator<< (std::vector<std::string>& v, const char * e){
        v.push_back(e);
        return v;
    }

    //! Helper for populating STL vectors
    template <class T, class U>
    inline std::vector<T>& operator<< (std::vector<T>& v, U& e){
        const T& val = (const T&)e;
        v.push_back(val);
        return v;
    }

    //! Helper for populating STL vector
    template <class T, class U>
    inline std::vector<T>& operator<< (std::vector<T>& v, const U& e){
        const T& val = (const T&)e;
        v.push_back(val);
        return v;
    }

} // namespace utils
} // namespace sparta


namespace std {
    //! Vector printer
    template<class Ch,class T, class Tr>
    inline std::basic_ostream<Ch,Tr>&
    operator<< (std::basic_ostream<Ch,Tr>& out, std::vector<T> const & v){
        out << sparta::utils::stringize_value(v);
        return out;
    }

    //! Pair Printer
    template<class Ch, class T, class U, class Tr>
    inline std::basic_ostream<Ch,Tr>&
    operator<< (std::basic_ostream<Ch,Tr>& out, std::pair<T,U> const & p){
        out << sparta::utils::stringize_value(p);
        return out;
    }
}

//! Specialization for populating STL vector of string with char*s.
inline std::vector<std::string>& operator<< (std::vector<std::string>& v, const char * e){
    v.push_back(e);
    return v;
}

//! Helper for populating STL vectors
template <class T, class U>
inline std::vector<T>& operator<< (std::vector<T>& v, U& e){
    const T& val = (const T&)e;
    v.push_back(val);
    return v;
}

//! Helper for populating STL vector
template <class T, class U>
inline std::vector<T>& operator<< (std::vector<T>& v, const U& e){
    const T& val = (const T&)e;
    v.push_back(val);
    return v;
}


// __PRINTING_H__
#endif
