// <StringUtils> -*- C++ -*-

/*!
 * \file StringUtils.h
 * \brief String utilities used in SimDB
 */

#ifndef __SIMDB_STRING_UTILS_H__
#define __SIMDB_STRING_UTILS_H__

#include "simdb/utils/uuids.hpp"

#include <algorithm>
#include <string>

namespace simdb {
namespace utils {

/*!
 * \brief Generate a random string value. Uses the generateUUID()
 * method. This is here for continuity with the other chooseRand()
 * methods found in MathUtils.h
 */
template <typename T>
typename std::enable_if<
    std::is_same<T, std::string>::value,
T>::type
chooseRand()
{
    return simdb::generateUUID();
}

/*!
 * \brief Utility class which applies a user-provided functor to all
 * chars of a std::string, so that users do not have to remember to
 * always apply these algorithms manually themselves.
 */
template <class AppliedTransform>
class TransformedString
{
public:
    TransformedString() = default;

    TransformedString(const char * str) :
        TransformedString(std::string(str))
    {}

    TransformedString(const std::string & str) :
        str_(str)
    {
        applyTransform_();
    }

    TransformedString & operator=(const std::string & str) {
        str_ = str;
        applyTransform_();
        return *this;
    }

    TransformedString(const TransformedString & rhs) :
        str_(rhs.str_)
    {}

    TransformedString & operator=(const TransformedString & rhs) {
        str_ = rhs.str_;
        return *this;
    }

    inline bool operator==(const TransformedString & rhs) const {
        return str_ == rhs.str_;
    }

    inline bool operator!=(const TransformedString & rhs) const {
        return str_ != rhs.str_;
    }

    inline bool operator==(const std::string & rhs) const {
        return str_ == rhs;
    }

    inline bool operator!=(const std::string & rhs) const {
        return str_ != rhs;
    }

    inline bool operator==(const char * rhs) const {
        return str_ == rhs;
    }

    inline bool operator!=(const char * rhs) const {
        return str_ != rhs;
    }

    inline TransformedString & operator+=(const char ch) {
        TransformedString rhs(std::string(1, ch));
        str_ += rhs.str_;
        return *this;
    }

    inline TransformedString & operator+=(const std::string & str) {
        TransformedString rhs(str);
        str_ += rhs.str_;
        return *this;
    }

    bool empty() const {
        return str_.empty();
    }

    size_t size() const {
        return str_.size();
    }

    void clear() {
        str_.clear();
    }

    inline const std::string & getString() const {
        return str_;
    }

    inline operator std::string() const {
        return str_;
    }

private:
    void applyTransform_() {
        std::transform(str_.begin(), str_.end(), str_.begin(), transformer_);
    }

    std::string str_;
    AppliedTransform transformer_;
};

// Get the lower/UPPER underlying transformed string, and print
// the transformed string to an ostream.

// Functor for ::tolower
struct make_lowercase {
    int operator()(const int ch) const {
        return std::tolower(ch);
    }
};

// Functor for ::toupper
struct make_uppercase {
    int operator()(const int ch) const {
        return std::toupper(ch);
    }
};

// always lowercase string data type
typedef TransformedString<make_lowercase> lowercase_string;

// ALWAYS UPPERCASE STRING DATA TYPE
typedef TransformedString<make_uppercase> UPPERCASE_STRING;

// Get the lowercase underlying transformed string, and print
// the transformed string to an ostream.
inline std::ostream & operator<<(std::ostream & os,
                                 const lowercase_string & s)
{
    os << s.getString();
    return os;
}

// Get the UPPERCASE underlying transformed string, and print
// the transformed string to an ostream.
inline std::ostream & operator<<(std::ostream & os,
                                 const UPPERCASE_STRING & s)
{
    os << s.getString();
    return os;
}

// Less than comparison, so you can put these data types
// into ordered containers like std::set's
template <class AppliedTransform>
inline bool operator<(const TransformedString<AppliedTransform> & one,
                      const TransformedString<AppliedTransform> & two)
{
    return one.getString() < two.getString();
}

// Comparisons against std::string where the utils object is the rhs:
//
//      std::string s1(...);
//      lowercase_string s2(...);
//      if (s1 == s2) {
//         ...
//      }
template <class AppliedTransform>
inline bool operator==(const std::string & one,
                       const TransformedString<AppliedTransform> & two)
{
    return one == two.getString();
}

template <class AppliedTransform>
inline bool operator!=(const std::string & one,
                       const TransformedString<AppliedTransform> & two)
{
    return one != two.getString();
}

} // namespace utils
} // namespace simdb

#endif
