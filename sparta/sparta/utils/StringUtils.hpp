// <CommandLineSimulator> -*- C++ -*-


/*!
 * \file StringUtils.hpp
 * \brief Cool string utilities
 */

#ifndef __SPARTA_UTILS_STRING_UTILS_H__
#define __SPARTA_UTILS_STRING_UTILS_H__

#include <cinttypes>
#include <cstring>
#include <string>
#include <cstring>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>

namespace sparta {
namespace utils {

//!===========================================================================
//! function uint64_to_hexstr()
//! - print a uint64 as a hex string
//! - prints an "_" to separate the upper 8 digits from the lower 8 digits
//
inline std::string uint64_to_hexstr (uint64_t val)
{
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << (val >> 32)
        << "_" << std::setw(8) << (val & 0xffffffff);
    return oss.str();
}



//!===========================================================================
//! function uint32_to_hexstr()
//! - print a uint32 as a hex string
//
inline std::string uint32_to_hexstr (uint32_t val)
{
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << val;
    return oss.str();
}


//!===========================================================================
//! function uint64_to_str()
//! - print a uint64 as a string
//
inline std::string uint64_to_str (uint64_t val)
{
    std::ostringstream ss;
    return (static_cast<std::ostringstream*>( &(ss << val))->str());
}


//!===========================================================================
//! function uint32_to_hexstr()
//! - print a uint32 as a hex string
//
inline std::string uint32_to_str (uint32_t val)
{
    std::ostringstream ss;
    return (static_cast<std::ostringstream*>( &(ss << val))->str());
}

//!===========================================================================
//! function int64_to_str()
//! - print a int64 as a string
//
inline std::string int64_to_str (int64_t val)
{
    std::ostringstream ss;
    return (static_cast<std::ostringstream*>( &(ss << val))->str());
}


//!===========================================================================
//! function int32_to_hexstr()
//! - print a int32 as a hex string
//
inline std::string int32_to_str (int32_t val)
{
    std::ostringstream ss;
    return (static_cast<std::ostringstream*>( &(ss << val))->str());
}

//!===========================================================================
//! function bool_to_str()
//! - print a int32 as a hex string
//
inline std::string bool_to_str (bool val)
{
    std::ostringstream ss;
    return (static_cast<std::ostringstream*>( &(ss << std::boolalpha << val))->str());
}

//!===========================================================================
//! function bin_to_hexstr()
//! - print binary data as a hex string
//
inline std::string bin_to_hexstr(const uint8_t *data, size_t size, const std::string &sep=" ")
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = size; i > 0; --i) {
        ss << std::setw(2) << (uint32_t)data[i - 1];
        if (i - 1 != 0) {
            ss << sep;
        }
    }
    return ss.str();
}

//!===========================================================================
//! function bin_to_bitstr()
//! - print binary data as a bit string
//
inline std::string bin_to_bitstr(const uint8_t *data, size_t size, const std::string &sep=" ")
{
    std::stringstream ss;

    for (size_t i = size; i > 0; --i) {
        const auto byte = data[i - 1];
        for (size_t b = 8; b > 0; --b) {
            const auto mask = 1 << (b - 1);

            ss << ((byte & mask)  ? '1' : '0');
        }
        if (i > 1) {
            ss << sep;
        }
    }

    return ss.str();
}

//!===========================================================================
//! function eliminate_whitespace()
//! - strip whitespace from anywhere in a string (including newlines)
//
inline std::string eliminate_whitespace(const std::string &str)
{
    std::string strip_str = str;
    strip_str.erase(std::remove_if(strip_str.begin(), strip_str.end(), ::isspace),
                    strip_str.end());
    return strip_str;
}   //! function eliminate_whitespace()

//!===========================================================================
//! function strip_whitespace()
//! - strip whitespace from the front and back of a string (including newlines)
//
inline std::string strip_whitespace(const std::string &str)
{
    const std::string STR_WHITESPACE = " \t\n\r";

    size_t start_pos = str.find_first_not_of(STR_WHITESPACE);
    size_t end_pos   = str.find_last_not_of(STR_WHITESPACE);

    return str.substr(start_pos, end_pos-start_pos + 1);
}   //! function strip_whitespace()

//!===========================================================================
//! function strip_string_pattern()
//! - strip the given string of the given pattern (at the end/beginning)
//!   Examples:
//!       strip_string_pattern("out", "remove_out") -> "remove_"
//!       strip_string_pattern("out", "out_remove_out") -> "_remove_"
//
inline std::string strip_string_pattern(const std::string & pat,
                                        const std::string & str)
{
    //! Look for the pattern at the beginning of the string
    size_t start_pos = str.find(pat);
    if(start_pos == 0) {
        //! It's at the beginning...
        start_pos += pat.length();
    }
    else {
        start_pos = 0;
    }

    //! Look for the pattern at the end of the string
    size_t end_pos = str.rfind(pat);
    if(end_pos == 0 || (end_pos + pat.length()) != str.length()) {
        //! Found the beginning one OR we found a match NOT at the end
        end_pos = std::string::npos;
    }
    else {
        end_pos -= start_pos;
    }

    return str.substr(start_pos, end_pos);
}   //! function strip_string_pattern()

//!===========================================================================
//! function tokenize()
//! - tokenize a string with an arbitrary set of delimiters, return
//!   result in a string vector
//! - default is space
//! - note:  multiple consecutive whitespaces are treated as separate delimiters,
//!   so that " x   y " would tokenize as ("", "x", "", "", "y", "")
//
inline void tokenize(const std::string &in_str, std::vector<std::string> & str_vector, const std::string &delimiters = " ")
{
    size_t prev_pos = 0;
    size_t cur_pos = 0;
    while ((cur_pos = in_str.find_first_of(delimiters, prev_pos)) != std::string::npos) {
        str_vector.push_back(in_str.substr(prev_pos, cur_pos - prev_pos));
        prev_pos = cur_pos + 1;
    }

    str_vector.push_back(in_str.substr(prev_pos));
}   //! function tokenize()

//!===========================================================================
//! function split_lines_around_tokens()
//! - tokenize multi-line text around line separators and an arbirtary
//!   set of delimiters, returning a vector of tokenized lines
//! - default line separator is newline ('\n')
//! - default token/delimiter applied per line is a space (' ')
//
//! For example, an istream containing this text:
//
//!    x:foo
//!    y:bar:z:baz?w?buz
//
//! Could be tokenized with this call:
//
//!    std::vector<std::vector<std::string>> str_vectors;
//!    utils::split_lines_around_tokens(in_stream, str_vectors, ":?");
//
//! Would return str_vectors containing:
//
//!    {
//!      { "x", "foo" },
//!      { "y", "bar", "z", "baz" }
//!    }
//
//! Assuming the line endings of the text were '\n' (the default line separator)
inline void split_lines_around_tokens(std::istream & in_stream,
                                      std::vector<std::vector<std::string>> & str_vectors,
                                      const std::string & delimiters = " ",
                                      const char line_separator = '\n')
{
    std::string line;
    while (std::getline(in_stream, line, line_separator)) {
        str_vectors.emplace_back(std::vector<std::string>());
        tokenize(line, str_vectors.back(), delimiters);
    }
}   //! function split_lines_around_tokens()


//!===========================================================================
//! function tokenize()
//! - tokenize a character string with an arbitrary set of delimiters, return
//!   result in a string vector
//! - default is space
//! - note:  multiple consecutive whitespaces are treated as separate delimiters,
//!   so that " x   y " would tokenize as ("", "x", "", "", "y", "")
//
inline void tokenize(const char *in_char_str, std::vector<std::string> & str_vector, const std::string &delimiters = " ")
{
    //! Copy char * into a string
    std::string in_str = in_char_str;

    tokenize(in_str, str_vector, delimiters);
}   //! function tokenize()


//!===========================================================================
//! function tokenize_strip_whitespace()
//! - same as tokenize(), but strip whitespace at beginning and end of each string
//
inline void tokenize_strip_whitespace(const std::string &in_str, std::vector<std::string> & str_vector, const std::string &delimiters = " ")
{
    size_t prev_pos = 0;
    size_t cur_pos = 0;
    while ((cur_pos = in_str.find_first_of(delimiters, prev_pos)) != std::string::npos) {
        str_vector.push_back(strip_whitespace(in_str.substr(prev_pos, cur_pos - prev_pos)));
        prev_pos = cur_pos + 1;
    }

    str_vector.push_back(strip_whitespace(in_str.substr(prev_pos)));
}   //! function tokenize_strip_whitespace()


//!===========================================================================
//! function tokenize_strip_whitespace()
//! - tokenize a character string with an arbitrary set of delimiters, return
//!   result in a string vector
//! - default is space
//! - note:  multiple consecutive whitespaces are treated as separate delimiters,
//!   so that " x   y " would tokenize as ("", "x", "", "", "y", "")
//
inline void tokenize_strip_whitespace(const char *in_char_str, std::vector<std::string> & str_vector, const std::string &delimiters = " ")
{
    //! Copy char * into a string
    std::string in_str = in_char_str;

    tokenize_strip_whitespace(in_str, str_vector, delimiters);
}   //! function tokenize_strip_whitespace()


//!===========================================================================
//! function tokenize_on_whitespace()
//! - tokenize a string on whitespace (space, tab, return)
//! - note:  multiple consecutive whitespaces are treated as a single delimiters
//!   so that " x   y " would tokenize as ("x", "y")
//
inline void tokenize_on_whitespace(const std::string &in_str, std::vector<std::string> & str_vector)
{
    const std::string DELIMITER = " \t\n\r";

    size_t prev_pos = 0;
    size_t cur_pos = 0;
    while ((cur_pos = in_str.find_first_of(DELIMITER, prev_pos)) != std::string::npos) {
        if (cur_pos != prev_pos) {
            str_vector.push_back(in_str.substr(prev_pos, cur_pos - prev_pos));
        }
        prev_pos = cur_pos + 1;
    }

    if (prev_pos != in_str.size()) {
        str_vector.push_back(in_str.substr(prev_pos));
    }
}   //! function tokenize()


//!===========================================================================
//! function tokenize_on_whitespace()
//! - tokenize a character string on whitespace (space, tab, return)
//! - note:  multiple consecutive whitespaces are treated as a single delimiters
//!   so that " x   y " would tokenize as ("x", "y")
//
inline void tokenize_on_whitespace(const char *in_char_str, std::vector<std::string> & str_vector)
{
    //! Copy char * into a string
    std::string in_str = in_char_str;

    tokenize_on_whitespace(in_str, str_vector);
}   //! function tokenize()

//!===========================================================================
//! function strcmp_with_null()
//! - Compare two strings. There are two cases in which the strings comare equal;
//! when both strings are nullptr and otherwise if the actual strings are equal.
inline bool strcmp_with_null(const char *s1, const char *s2)
{
    if (s1 != nullptr && s2 != nullptr) {
        return !std::strcmp(s1, s2);
    }
    if (s1 == nullptr && s2 == nullptr) {
        return true;
    }
    return false;
}

//!===========================================================================
//! function LevenshteinDistance()
//
//! - Calculate the Levenshtein distance for two strings.
//!   The returned value gives an indication of how similar
//!   the strings are to one another. This is used in error
//!   diagnostics in the import program.
//
//! Implementation taken from here:
//!   https://gist.github.com/TheRayTracer/2644387
//
inline uint32_t LevenshteinDistance(const char * s, size_t n,
                                    const char * t, size_t m)
{
    ++n;
    ++m;
    std::unique_ptr<size_t[]> dptr(new size_t[n * m]);
    size_t * d = dptr.get();

    memset(d, 0, sizeof(size_t) * n * m);
    for (size_t i = 1, im = 0; i < m; ++i, ++im) {
        for (size_t j = 1, jn = 0; j < n; ++j, ++jn) {
            if (s[jn] == t[im]) {
                d[(i * n) + j] = d[((i - 1) * n) + (j - 1)];
            } else {
                d[(i * n) + j] = std::min({
                    d[(i - 1) * n + j] + 1,          //! A deletion
                    d[(i - 1) * n + j] + 1,          //! An insertion
                    d[(i - 1) * n + (j - 1)] + 1     //! A substitution
                });
            }
        }
    }

    return d[n * m - 1];
}   //! function LevenshteinDistance()

//!===========================================================================
//! Utility class which applies a user-provided functor to all
//! chars of a std::string, so that users do not have to remember
//! to always apply these algorithms manually themselves.
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
        return strcmp(str_.c_str(), rhs) == 0;
    }

    inline bool operator!=(const char * rhs) const {
        return strcmp(str_.c_str(), rhs) != 0;
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

//! Functor for ::tolower
struct make_lowercase {
    int operator()(const int ch) const {
        return std::tolower(ch);
    }
};

//! Functor for ::toupper
struct make_uppercase {
    int operator()(const int ch) const {
        return std::toupper(ch);
    }
};

//! always lowercase string data type
typedef TransformedString<make_lowercase> lowercase_string;

//! ALWAYS UPPERCASE STRING DATA TYPE
typedef TransformedString<make_uppercase> UPPERCASE_STRING;

//! Less than comparison, so you can put these data types
//! into ordered containers like std::set's
template <class AppliedTransform>
inline bool operator<(const TransformedString<AppliedTransform> & one,
                      const TransformedString<AppliedTransform> & two)
{
    return one.getString() < two.getString();
}

//! Comparisons against std::string where the utils object is the rhs:
//
//!      std::string s1(...);
//!      lowercase_string s2(...);
//!      if (s1 == s2) {
//!         ...
//!      }
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

}
}

#endif
