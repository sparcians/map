// <LexicalCast> -*- C++ -*-

#ifndef __LEXICAL_CAST_H__
#define __LEXICAL_CAST_H__

#include <cstdlib>
#include <iostream>
#include <math.h>
#include <typeinfo>
#include <bitset>
#include <string>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MetaStructs.hpp"


// For YAML converter (bool string -> bool)
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/convert.h>

/*!
 * \file LexicalCast.hpp
 * \brief String-to-value helpers and string formatting helpers.
 */

#if defined(__clang__)
// for LLVM/clang
namespace std
{
    template <typename _Cp, bool>
    class __bit_reference;
}
#else
// for gcc/icc
namespace std
{
    struct _Bit_reference;
}
#endif

namespace sparta
{

    /*!
     * \brief Helper type for converting _Bit_reference types into bool types
     * for the purpose of determining how to lexically cast a string into that
     * type.
     */
    template <typename T>
    struct bit_reference_to_bool {
        typedef T type;
    };

#if !defined(__linux__) && defined(__llvm__)
    template <typename _Cp>
    struct bit_reference_to_bool<std::__1::__bit_reference<_Cp> > {
        typedef bool type;
    };
#else
    template <>
    struct bit_reference_to_bool<typename std::_Bit_reference> {
        typedef bool type;
    };
#endif

    /*!
     * Cast string to T where T is either ValueType or (if ValueType is a
     * vector) a vector's value_type.
     *
     * boost::lexical_cast and ios_base::operator>> fail to determine type of
     * an integer literal. For example, 0xdeadbeef is blindly interpreted as 0
     * with leftover characters. 070 is interpreted as decimal 70.
     *
     * This attempts to interpret hex and octal in addition to decimal, and
     * string values.
     * \throw SpartaException if cast fails.
     */
    template <class T>
    inline T lexicalCast(const std::string& str, uint32_t base=10) {
        (void) base;
        T tmp;
        std::stringstream ss;
        ss << str;
        ss >> tmp;
        if(((void*)&ss) != nullptr){
            return tmp;
        }

        SpartaException ex("Unable to cast string \"");
        ex << str << "\" to typeid: \"" << typeid(T).name() << "\"";
        throw ex;
    }

    template <class T>
    inline MetaStruct::enable_if_t<MetaStruct::is_stl<T>::value and
                                   std::is_same<typename T::value_type, bool>::value, T>
    lexicalCast(const std::string& str, uint32_t base = 10) = delete;

    template <>
    inline std::string lexicalCast(const std::string& str, uint32_t base) {
        (void) base;
        return str;
    }

    template <>
    inline bool lexicalCast(const std::string& str, uint32_t base) {
        (void) base;
        bool out;
        YAML::Node node(YAML::NodeType::Scalar);
        node = str;

        // Handles conversion of YAML booleans literals like on/off,
        // yes/no, etc.  We're cheating here and using code already
        // written and tested.  Hopefully Jesse Beder doesn't change
        // it on us again. LOL
        if(false == YAML::convert<bool>::decode(node, out)){
            int out_int;
            if(false == YAML::convert<int>::decode(node, out_int)){ // Fall back to 1/0 support
                SpartaException ex("Unable to cast string \"");
                ex << str << "\" to bool";
                throw ex;
            }else{
                out = out_int != 0;
            }
        }
        return out;
    }

    template <>
    inline uint64_t lexicalCast(const std::string& str, uint32_t base) {
        try {
            uint64_t val = std::stoul(str.c_str(), nullptr, base);
            return val;
        }
        catch(std::exception &e) {
            SpartaException ex("Unable to cast string \"");
            ex << str << "\" to uint64_t: " << e.what();
            throw ex;
        }
        return 0;
    }

    template <>
    inline int64_t lexicalCast(const std::string& str, uint32_t base) {
        try {
            int64_t val = std::stol(str.c_str(), nullptr, base);
            return val;
        }
        catch(std::exception &e) {
            SpartaException ex("Unable to cast string \"");
            ex << str << "\" to int64_t: " << e.what();
            throw ex;
        }
        return 0;
    }

    template <>
    inline uint32_t lexicalCast(const std::string& str, uint32_t base) {
        try {
            // There is no conversation of a string to a 32-bit unsigned in C/C++
            uint64_t val = std::stoul(str.c_str(), nullptr, base);
            if (val > std::numeric_limits<uint32_t>::max()) {
                throw std::range_error("Value " + str + " is too large to fit into a uint32_t");;
            }
            return static_cast<uint32_t>(val);
        }
        catch(std::exception &e) {
            SpartaException ex("Unable to cast string \"");
            ex << str << "\" to uint32_t: " << e.what();
            throw ex;
        }
        return 0;
    }

    template <>
    inline int32_t lexicalCast(const std::string& str, uint32_t base) {
        try {
            int32_t val = std::stoi(str.c_str(), nullptr, base);
            return val;
        }
        catch(std::exception &e) {
            SpartaException ex("Unable to cast string \"");
            ex << str << "\" to int32_t: " << e.what();
            throw ex;
        }
        return 0;
    }

    //! Gets number of decimal digits in a uint32_t
    inline uint32_t numDecDigits(uint32_t val){
        if(val <= 1){
            return 1;
        }
        return 1 + floor(log10(val));
    }

} // namespace sparta


// __LEXICAL_CAST_H__
#endif
