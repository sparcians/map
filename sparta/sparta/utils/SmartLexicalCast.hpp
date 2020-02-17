// <SmartLex> -*- C++ -*-

/*!
 * \file SmartLexicalCast.hpp
 * \brief Smart lexical casting supporting prefixes, separator ignoring, and
 * suffixes.
 */

#ifndef __SMART_LEX_H__
#define __SMART_LEX_H__

#include <iostream>
#include <utility>
#include <memory>

#include <boost/lexical_cast.hpp>

#include "sparta/utils/LexicalCast.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta {
    namespace utils {

/*!
 * \brief Modifier instance - Associates some suffix strings (e.g. "b") with
 * a semantic (e.g. multiply by one billion)
 */
struct Modifier {
    /*!
     * \brief Suffix strings identifying this modifier (case sensitive)
     */
    std::vector<const char*> options;

    /*!
     * \brief Multiplier applied to the value when this modifier is found
     */
    uint64_t mult;
};

/*!
 * \brief Prefix instance - Associates some prefix strings (e.g. "0x") with
 * a radix and a set of allowed digit characters for parsing a string following
 * this radix.
 */
struct RadixPrefix {

    /*!
     * \brief Prefix strings identifying this prefix (case sensitive)
     */
    const std::vector<const char*> options;

    /*!
     * \brief Radix associated with this prefix
     */
    const uint32_t radix;

    /*!
     * \brief Valid digits in a number following this prefix
     */
    const char* const digits;
};

static const uint32_t DEFAULT_RADIX = 10;

/*!
 * \brief Radixes supported
 * \note The digits field will have precidence over any suffix after the number.
 * Specifically, hex digits b or B will be treated as a hex digit and NEVER as
 * a suffix (billion). If the billion suffix were desired, the g or G suffix
 * would need to be used
 */
static const std::vector<RadixPrefix> prefixes = {
    {{"0x", "0X"},         16, "0123456789abcdefABCDEF"},
    {{"0b", "0B"},         2,  "01"},
    {{"0"},                8,  "01234567"} // '0' Must be last to avoid false positives
};

/*!
 * \brief Suffixes supported.
 */
static const std::vector<Modifier> suffixes = {

    // ISO/IEC 8000 unit prefixes
    // http://en.wikipedia.org/wiki/Mebi#IEC_standard_prefixes

    {{"ki", "Ki",
      "kI", "KI"},             1_u64<<10}, // kibi
    {{"mi", "Mi",
      "mI", "MI"},             1_u64<<20}, // mebi
    {{"gi", "Gi", "bi", "Bi",
      "gI", "GI", "bI", "BI"}, 1_u64<<30}, // gibi
    {{"ti", "Ti",
      "tI", "TI"},             1_u64<<40}, // tebi
    {{"pi", "Pi",
      "pI", "PI"},             1_u64<<50}, // pebi

    // SI unit prefixes (e.g. 'K' in 'KB')

    {{"n", "N"},               1},                // normal
    {{"k", "K"},               1000},             // kilo
    {{"m", "M"},               1000000},          // mega
    {{"g", "G", "b", "B"},     1000000000},       // giga
    {{"t", "T"},               1000000000000},    // tera
    {{"p", "P"},               1000000000000000}  // peta
};

/*!
 * \brief All whitespace characters allowed
 */
static const char* WHITESPACE = " _,\t\n";

/*!
 * \brief All decimal (base 10) digits allowed and '.'
 */
static const char* DECIMAL_DIGITS = "0123456789.";

/*!
 * \brief Performs a lexical cast on a string while attempting to interpret
 * radix prefixes (\see prefixes) and multiplying suffixes (\see suffixes)
 * \param s String to parse. String must contain no extra non-witespace characters where
 * "whitespace" is defined as any character in the WHITESPACE string.
 * \param end_pos [out] Position of string after parsing. All characters after point will be
 * whitespace. If all characters of \a s are consumed, this will be std::string::npos
 * \param allow_recursion Allow recursion during parsing. This allows stringing together values
 * to add together in the way the large values are spoken such as 1b5k (for one billion, five
 * thousand). Note that a prefix on the initial value does not apply to any subsequent values
 * encountered. For example, 0xck22 is (hex 0xc) * (decimal 1000) + (decimal 22)
 * When recursing intenally, this method is always reentered with allow_prefix=false, meaning
 * values parsed after the first are NOT allowed to have prefixes.
 * \param allow_prefix Allow parsing of prefixes. If false, prefixes are not allowed in \a s.
 * this is typiclly useful when recursively parsing the end of the string because of ambiguity
 * between certain suffixes (e.g. 50b and 0b111 could not be joined together because of the '0b').
 *
 * Example:
 * \code
 * // With allow_recursion=true && allow_prefix=true
 * 5b1        => five billion
 * 5t10k2     => five trillion, 10 thousdand, and 2
 * 0xcccb1    => hex value 0xcccb1
 * 0xcccb1g50 => (hex value 0xcccb1 * one billion) + decimal 50
 *
 * // Always illegal
 * 5t0xc00    => ERROR: Prefixes are not allowed on trailing values
 *
 * // With allow_recursion=false && allow_prefix=true
 * 5b         => five billion
 * 0x500      => hex value 0x500
 * 5b100      => ERROR: Recursion disabled
 *
 * // With allow_recursion=true && allow_prefix=false
 * 5b100      => five billion, one hundred
 * 0x5b2k     => ERROR: prefixes disabled
 * \endcode
 *
 * // With period (and allow_recursion)
 * 0.5M       => 500,000
 * .5m        => 500,000
 * .5m2k      => 502,000
 * .5m.2k     => 500,200
 *
 * \todo To resolve prefix/suffix ambiguity and allow stringing together multiple value with
 * prefixes support could be added for '+' as a disamgiuator (e.g. 50+0b111)
 */
template <typename T>
inline T smartLexicalCast(const std::string& s,
                          size_t& end_pos,
                          bool allow_recursion=true,
                          bool allow_prefix=true) {
    (void) allow_recursion;
    (void) allow_prefix;
    T result = lexicalCast<T>(s, 0); // 0 => use auto-radix
    end_pos = std::string::npos;
    return result;
}

/*!
 * \brief Parse and return a numeric string from after a certain position in an input string
 * \param s Input string
 * \param pos Position in input string to begin parsing at
 * \param digits Allowed digits in numeric string (in addition to WHITESPACE)
 * \param after_numeric [out] Position of string \a s after numeric string is extracted
 * \return Parsed numeric string (if possible) Returns "" if not.
 */
inline std::string parseNumericString(const std::string& s, size_t pos, const char* digits, size_t& after_numeric) {
    std::string raw_value_chars = digits;
    raw_value_chars += WHITESPACE;
    after_numeric = s.find_first_not_of(raw_value_chars, pos);

    if(after_numeric == pos){
        return "";
    }

    std::string numeric;
    if(after_numeric == std::string::npos){
        numeric = s.substr(pos);
    }else{
        numeric = s.substr(pos, after_numeric - pos);
    }

    // Remove all "whitespace" from within the string
    for(const char* pc = WHITESPACE; *pc != '\0'; ++pc) {
        std::string to_replace;
        to_replace += *pc;
        replaceSubstring(numeric, to_replace, "");
    }

    return numeric;
}

template <>
inline uint64_t smartLexicalCast(const std::string& s,
                                 size_t& end_pos,
                                 bool allow_recursion,
                                 bool allow_prefix) {
    size_t pos = 0;

    // Skip leading space. If string is ONLY leading space, return 0
    pos = s.find_first_not_of(WHITESPACE);
    if(pos == std::string::npos){
        end_pos = pos; // Output ending position
        return 0;
    }

    std::string numeric; // Numeric (integer) portion of this string
    std::string fractional; // Decimal portion of the string (if any)
    size_t suffix_pos = pos;

    // Determine prefix (if allowed)

    uint32_t radix = DEFAULT_RADIX;
    const char* digits = DECIMAL_DIGITS;
    if(allow_prefix){
        // Look for a prefix (if any) at start of string (after whitespace) and extract radix
        bool found_prefix = false;
        for(auto & prefix : prefixes) {
            for(auto & str : prefix.options) {
                size_t prefix_pos = s.find(str, pos);
                if(prefix_pos == pos) {
                    //std::cout << "found prefix at " << prefix_pos << std::endl;

                    // Attempt to read ahead using this prefix. Parser must be able to get a
                    // non-null string for this prefix to be considered a match.
                    // Note that because the digits string for various prefixes does not contain
                    // '.', this will fail to parse a numeric string if it contains a '.'. This is
                    // the correct behavior
                    size_t spec_suffix_pos;
                    numeric = parseNumericString(s, prefix_pos + strlen(str), prefix.digits, spec_suffix_pos);
                    if(numeric.size() != 0){
                        radix = prefix.radix;
                        digits = prefix.digits;
                        pos = suffix_pos = spec_suffix_pos;
                        found_prefix = true;
                        break;
                    }
                }
            }
            if(found_prefix){
                break;
            }
        }
    }

    //std::cout << "radix = " << radix << std::endl;

    // Extract the (hopefully) numeric portion of the string
    if(numeric.size() == 0){
        sparta_assert(suffix_pos == pos);
        numeric = parseNumericString(s, pos, digits, suffix_pos);
        if(numeric.size() == 0){
            throw SpartaException("Unable to parse a numeric value from substring \"")
                  << s.substr(pos) << "\" within full string \"" << s
                  << "\" for smart lexical casting";
        }
    }

    pos = suffix_pos;

    // Convert a decimal number.
    // note, there are no whitespace or separators in numeric at this time
    size_t decimal_pos = numeric.find('.');
    if(decimal_pos != std::string::npos){
        if(decimal_pos == numeric.size() - 1){
            throw SpartaException("Encountered \".\" at the end of a numeric portion (\"") << numeric
                  << "\") of a string \"" << s << "\"";
        }
        fractional = numeric.substr(decimal_pos + 1);
        numeric = numeric.substr(0, decimal_pos);
    }

    uint64_t value = 0;
    // The C functions of yester-year might or might not error on
    // empty strings.  Different machines do different things. It's
    // silly to send a null string to be cast either way, so catch it
    // here and evaluate to 0.
    if(numeric.find_first_not_of(WHITESPACE) != std::string::npos) {
        // Determine raw integer value of the numeric portion of the string
        value = lexicalCast<uint64_t>(numeric, radix);
    }

    if(pos == std::string::npos){
        if(fractional.size() != 0){
            throw SpartaException("Encountered a fractional value: \"") << numeric << "\" . \""
                  << fractional << "\" but no suffix was found, so this cannot possibly represent "
                  " an integer. Found in \"" << s << "\"";
        }

        end_pos = pos; // Output ending position
    }else{
        // This is redundant with the value extraction above
        //// Ensure the numeric string ONLY contains valid numeric characters
        //size_t non_numeric_char_pos = numeric.find_first_not_of(digits);
        //if(non_numeric_char_pos != std::string::npos){
        //    throw SpartaException("Found non-numeric digit '") << numeric[non_numeric_char_pos]
        //          << "' in portion of string (\"" << numeric << "\") expected to be only the digits: \""
        //          << digits << "\". Encountered when parsing full string\"" << s << "\"";
        //}

        // Find the suffix (if any) and extract a modifier function (e.g. x*1000)
        size_t after_suffix_pos = pos;
        uint64_t suffix_multiplier = 0;
        for(auto & suffix : suffixes) {
            for(auto & str : suffix.options) {
                auto tmp = s.find(str, pos);
                // Check if this suffix comes before another that was found later in the string
                if(tmp == pos) {
                    //std::cout << "found suffix at " << suffix_pos << std::endl;
                    suffix_multiplier = suffix.mult;
                    after_suffix_pos = tmp + strlen(str);
                    break;
                }
            }
            if(suffix_multiplier != 0){
                break;
            }
        }

        if(suffix_multiplier == 0){
            suffix_multiplier = 1;
        }

        // Apply suffix modifier extracted when parsing the suffix (e.g. "k" = N*1000)
        value *= suffix_multiplier;

        // Apply suffix modifier to the fractional portion
        if(fractional.size() != 0){
            // Check each digit to ensure that it is an integer portion of the
            // This is done manually to guarantee NO floating point rounding errors can occur
            uint32_t ten_div = 10;
            for(auto ch : fractional){
                uint64_t frac = (ch - '0');
                sparta_assert(frac < 10);
                frac *= suffix_multiplier;
                if(frac < ten_div){
                    throw SpartaException("Encountered a fractional value: \"") << numeric << "\" . \""
                          << fractional << "\" but suffix multipler was only " << suffix_multiplier
                          << ", so this fraction does not represent and integer. Fraction should "
                          "not have a 1/" << ten_div << " place. Found in \"" << s << "\"";
                }
                frac /= ten_div;
                value += frac;
                ten_div *= 10;
            }
        }

        // Recursively reparse remainder of string and add result to current value
        if(allow_recursion){
            if(after_suffix_pos != std::string::npos){
                uint64_t addition = smartLexicalCast<uint64_t>(s.substr(after_suffix_pos),
                                                               after_suffix_pos, // Pos after parsing
                                                               true,             // Allow recursion
                                                               false);           // Disallow prefixes

                value += addition;
            }
        }

        end_pos = after_suffix_pos;

        // Reject any garbage remaining
        if(end_pos != std::string::npos){
            size_t garbage_pos = s.find_first_not_of(WHITESPACE, end_pos);
            if(garbage_pos != std::string::npos) {
                throw SpartaException("Found non-'whitespace' grabage character '") << s[garbage_pos]
                      << "' after suffix (at or after char " << end_pos << ") in string being "\
                      "smart-lexically-cast: \"" << s << "\"";
            }
        }
    }

    return value;
}

template <>
inline uint32_t smartLexicalCast(const std::string& s,
                                 size_t& end_pos,
                                 bool allow_recursion,
                                 bool allow_prefix) {
    uint64_t val = smartLexicalCast<uint64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_u64 << 32)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a uint32_t because it "
              "contained a value this type could not contain: " << val;
    }
    return (uint32_t)val;
}

template <>
inline uint16_t smartLexicalCast(const std::string& s,
                                 size_t& end_pos,
                                 bool allow_recursion,
                                 bool allow_prefix) {
    uint64_t val = smartLexicalCast<uint64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_u64 << 16)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a uint32_t because it "
              "contained a value this type could not contain: " << val;
    }
    return (uint16_t)val;
}

template <>
inline uint8_t smartLexicalCast(const std::string& s,
                                size_t& end_pos,
                                bool allow_recursion,
                                bool allow_prefix) {
    uint64_t val = smartLexicalCast<uint64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_u64 << 8)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a uint32_t because it "
              "contained a value this type could not contain: " << val;
    }
    return (uint8_t)val;
}

template <>
inline int64_t smartLexicalCast(const std::string& s,
                                size_t& end_pos,
                                bool allow_recursion,
                                bool allow_prefix) {
    // Get negative sign from front of string
    size_t after_neg_pos = 0;
    size_t neg_pos = s.find_first_not_of(WHITESPACE);
    bool negate = false;
    if(neg_pos != std::string::npos){
        if(s[neg_pos] == '-'){
            negate = true;
            after_neg_pos = neg_pos + 1;
        }
        // after_neg_pos remains 0
    }

    uint64_t val = smartLexicalCast<uint64_t>(s.substr(after_neg_pos), end_pos, allow_recursion, allow_prefix);
    if((negate == false && val >= (1_u64 << 63))
       || (negate == true && val > (1_u64 << 63))) {
        throw SpartaException("Could not lexically cast \"") << s << "\" to a int64_t because it "
              "contained a value  this type could not contain: " << val;
    }
    int64_t signed_val = val;
    if(negate){
        signed_val *= -1;
    }
    return signed_val;
}

template <>
inline int32_t smartLexicalCast(const std::string& s,
                                size_t& end_pos,
                                bool allow_recursion,
                                bool allow_prefix) {
    int64_t val = smartLexicalCast<int64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_64 << 31)
       || val < -(1_64 << 31)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a int32_t because it "
              "contained a value larger than this type could not contain: " << val;
    }
    return (int32_t)val;
}

template <>
inline int16_t smartLexicalCast(const std::string& s,
                                size_t& end_pos,
                                bool allow_recursion,
                                bool allow_prefix) {
    int64_t val = smartLexicalCast<int64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_64 << 16)
       || val < -(1_64 << 16)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a int16_t because it "
              "contained a value this type could not contain: " << val;
    }
    return (int16_t)val;
}

template <>
inline int8_t smartLexicalCast(const std::string& s,
                               size_t& end_pos,
                               bool allow_recursion,
                               bool allow_prefix) {
    int64_t val = smartLexicalCast<int64_t>(s, end_pos, allow_recursion, allow_prefix);
    if(val >= (1_64 << 8)
       || val < -(1_64 << 8)){
        throw SpartaException("Could not lexically cast \"") << s << "\" to a int8_t because it "
              "contained a value this type could not contain: " << val;
    }
    return (int8_t)val;
}

template <>
inline double smartLexicalCast(const std::string& s,
                               size_t& end_pos,
                               bool allow_recursion,
                               bool allow_prefix) {
    (void) allow_recursion;
    (void) allow_prefix;

    utils::ValidValue<double> dbl_value;
    end_pos = 0;

    try {
        dbl_value = boost::lexical_cast<double>(s);
    } catch (const boost::bad_lexical_cast &) {
        throw SpartaException("Could not lexically cast \"") << s << "\" to a double";
    }

    end_pos = std::string::npos;
    return dbl_value.getValue();
}

    }; // namespace utils
} // namespace sparta

// __SMART_LEX_H__
#endif
