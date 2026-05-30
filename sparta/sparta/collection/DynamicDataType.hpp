// <DynamicDataType.hpp> -*- C++ *-*

#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <limits>
#include <regex>
#include <string>

#include "simdb/apps/argos/ArgosCollector.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"
#include "simdb/utils/TypeTraits.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"

namespace sparta::collection::detail {

//! Regex helpers for legacy operator<< format detection.
//! Bare-value formats (5/6/7) use simple tokens (no commas, spaces, or parens).
//! Colon formats allow ':' inside a value but not spaces or commas.
//! Paren formats allow any non-')' inside parentheses.
namespace dynamic_format_regex {

inline bool fullMatch(const std::string& s, const std::regex& re)
{
    return std::regex_match(s, re);
}

inline const std::regex& parserImpl1Pattern()
{
    static const std::regex re(
        R"(^\s*(?:[A-Za-z][A-Za-z0-9_\-]*\s*\(\s*[^)]+\s*\)\s*)+$)");
    return re;
}

inline const std::regex& parserImpl2Pattern()
{
    static const std::regex re(
        R"(^\s*(?:[A-Za-z][A-Za-z0-9_\-]*\s*\(\s*[^)]+\s*\)\s*,\s*)+)"
        R"([A-Za-z][A-Za-z0-9_\-]*\s*\(\s*[^)]+\s*\)\s*$)");
    return re;
}

inline const std::regex& parserImpl3Pattern()
{
    // At least two space-separated name:value pairs; values may contain ':'.
    static const std::regex re(
        R"(^\s*[A-Za-z][A-Za-z0-9_\-]*\s*:\s*[^\s,]+(?:\s+[A-Za-z][A-Za-z0-9_\-]*\s*:\s*[^\s,]+)+\s*$)");
    return re;
}

inline const std::regex& parserImpl4Pattern()
{
    // Comma-separated name:value pairs (single pair allowed); values may contain ':'.
    static const std::regex re(
        R"(^\s*(?:[A-Za-z][A-Za-z0-9_\-]*\s*:\s*[^\s,]+(?:\s*,\s*[A-Za-z][A-Za-z0-9_\-]*\s*:\s*[^\s,]+)*)\s*$)");
    return re;
}

inline const std::regex& parserImpl5Pattern()
{
    static const std::regex re(
        R"(^\s*[^,\s\(\)\[\]:]+(?:\s+[^,\s\(\)\[\]:]+)*\s*$)");
    return re;
}

inline const std::regex& parserImpl6Pattern()
{
    static const std::regex re(
        R"(^\s*[^,\s\(\)\[\]:]+(?:\s*,\s*[^,\s\(\)\[\]:]+)+\s*$)");
    return re;
}

inline const std::regex& parserImpl7Pattern()
{
    static const std::regex re(
        R"(^\s*\[\s*(?:[^,\s\(\)\[\]:]+(?:\s*,\s*[^,\s\(\)\[\]:]+)*\s*)?\]\s*$)");
    return re;
}

//! Extraction regexes (distinct from full-string validation patterns).
inline const std::regex& nameParenValueToken()
{
    static const std::regex re(
        R"(([A-Za-z][A-Za-z0-9_\-]*)\s*\(\s*([^)]+)\s*\))");
    return re;
}

inline const std::regex& nameColonValueToken()
{
    static const std::regex re(
        R"(([A-Za-z][A-Za-z0-9_\-]*)\s*:\s*([^\s,]+))");
    return re;
}

inline const std::regex& simpleValueToken()
{
    static const std::regex re(R"(([^,\s\(\)\[\]:]+))");
    return re;
}

enum class FieldParseStyle {
    ParenSpace,
    ParenComma,
    ColonSpace,
    ColonComma,
    ValueSpace,
    ValueComma,
    ValueBracket,
};

inline void extractNamedPairs_(
    const std::string& stringified_fields,
    const std::regex& token_re,
    std::vector<std::string>& parsed_field_names,
    std::vector<std::string>& parsed_field_values)
{
    const std::sregex_iterator end;
    for (std::sregex_iterator it(stringified_fields.begin(),
                                 stringified_fields.end(),
                                 token_re);
         it != end;
         ++it) {
        parsed_field_names.push_back((*it)[1].str());
        parsed_field_values.push_back((*it)[2].str());
    }
}

inline void extractSimpleValues_(
    const std::string& stringified_fields,
    std::vector<std::string>& parsed_field_values)
{
    const std::sregex_iterator end;
    for (std::sregex_iterator it(stringified_fields.begin(),
                                 stringified_fields.end(),
                                 simpleValueToken());
         it != end;
         ++it) {
        parsed_field_values.push_back((*it)[1].str());
    }
}

inline void parseFields_(
    const std::string& stringified_fields,
    const FieldParseStyle style,
    std::vector<std::string>& parsed_field_names,
    std::vector<std::string>& parsed_field_values)
{
    parsed_field_names.clear();
    parsed_field_values.clear();

    switch (style) {
    case FieldParseStyle::ParenSpace:
    case FieldParseStyle::ParenComma:
        extractNamedPairs_(
            stringified_fields, nameParenValueToken(),
            parsed_field_names, parsed_field_values);
        break;
    case FieldParseStyle::ColonSpace:
    case FieldParseStyle::ColonComma:
        extractNamedPairs_(
            stringified_fields, nameColonValueToken(),
            parsed_field_names, parsed_field_values);
        break;
    case FieldParseStyle::ValueSpace:
    case FieldParseStyle::ValueComma:
        extractSimpleValues_(stringified_fields, parsed_field_values);
        parsed_field_names.assign(parsed_field_values.size(), "");
        break;
    case FieldParseStyle::ValueBracket: {
        static const std::regex bracket_content_re(
            R"(^\s*\[\s*(.*?)\s*\]\s*$)");
        std::smatch match;
        if (!std::regex_match(stringified_fields, match, bracket_content_re)) {
            throw simdb::DBException("Failed to parse bracket field values");
        }
        extractSimpleValues_(match[1].str(), parsed_field_values);
        parsed_field_names.assign(parsed_field_values.size(), "");
        break;
    }
    }

    if (parsed_field_values.empty()) {
        throw simdb::DBException("No field values found in stringified_fields");
    }
}

} // namespace dynamic_format_regex

using MinimalType = simdb::argos::MinimalType;

namespace dynamic_field_type {

enum class ValueKind {
    Bool,
    Int,
    Float,
    String,
};

struct ParsedValue {
    ValueKind kind = ValueKind::String;
    bool bool_val = false;
    bool is_negative = false;
    uint64_t u_mag = 0;
    int64_t s_val = 0;
    double float_val = 0.0;
};

struct FieldTypeState {
    bool initialized = false;
    MinimalType type = MinimalType::string_t;
    bool hex_prefix_locked = false;
    bool decimal_locked = false;
    bool seen_negative = false;
    bool is_float = false;
    uint64_t max_u = 0;
    int64_t min_s = 0;
    int64_t max_s = 0;
    double float_min = 0.0;
    double float_max = 0.0;
};

inline std::string trimToken_(const std::string& token)
{
    const auto begin = token.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = token.find_last_not_of(" \t");
    return token.substr(begin, end - begin + 1);
}

inline bool isBoolLiteral_(const std::string& token, bool* out)
{
    if (token.size() == 4) {
        if ((token[0] == 't' || token[0] == 'T') &&
            (token[1] == 'r' || token[1] == 'R') &&
            (token[2] == 'u' || token[2] == 'U') &&
            (token[3] == 'e' || token[3] == 'E')) {
            *out = true;
            return true;
        }
    }
    if (token.size() == 5) {
        if ((token[0] == 'f' || token[0] == 'F') &&
            (token[1] == 'a' || token[1] == 'A') &&
            (token[2] == 'l' || token[2] == 'L') &&
            (token[3] == 's' || token[3] == 'S') &&
            (token[4] == 'e' || token[4] == 'E')) {
            *out = false;
            return true;
        }
    }
    return false;
}

inline bool hasHexPrefix_(const std::string& token)
{
    return token.size() >= 2 && token[0] == '0' &&
           (token[1] == 'x' || token[1] == 'X');
}

inline bool looksLikeFloatLiteral_(const std::string& token)
{
    return token.find('.') != std::string::npos ||
           token.find('e') != std::string::npos ||
           token.find('E') != std::string::npos;
}

inline uint64_t parseHexMagnitude_(const std::string& token)
{
    const std::string digits = token.substr(2);
    if (digits.empty()) {
        throw simdb::DBException("Empty hex literal in field value");
    }
    uint64_t value = 0;
    for (const char c : digits) {
        if (value > (std::numeric_limits<uint64_t>::max() >> 4)) {
            throw simdb::DBException("Hex literal overflows uint64_t");
        }
        value <<= 4;
        int nybble = -1;
        if (c >= '0' && c <= '9') {
            nybble = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            nybble = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            nybble = 10 + (c - 'A');
        } else {
            throw simdb::DBException("Invalid hex digit in field value");
        }
        value |= static_cast<uint64_t>(nybble);
    }
    return value;
}

inline ParsedValue classifyToken_(const std::string& raw_token)
{
    const std::string token = trimToken_(raw_token);
    if (token.empty()) {
        throw simdb::DBException("Empty field value");
    }

    bool bool_val = false;
    if (isBoolLiteral_(token, &bool_val)) {
        ParsedValue out;
        out.kind = ValueKind::Bool;
        out.bool_val = bool_val;
        return out;
    }

    if (hasHexPrefix_(token)) {
        ParsedValue out;
        out.kind = ValueKind::Int;
        out.u_mag = parseHexMagnitude_(token);
        return out;
    }

    if (looksLikeFloatLiteral_(token)) {
        try {
            ParsedValue out;
            out.kind = ValueKind::Float;
            out.float_val = std::stod(token);
            return out;
        } catch (const std::exception&) {
            return ParsedValue{};
        }
    }

    std::size_t pos = 0;
    try {
        const int64_t v = std::stoll(token, &pos);
        if (pos == token.size()) {
            ParsedValue out;
            out.kind = ValueKind::Int;
            if (v < 0) {
                out.is_negative = true;
                out.s_val = v;
            } else {
                out.u_mag = static_cast<uint64_t>(v);
            }
            return out;
        }
    } catch (const std::exception&) {
    }

    try {
        const uint64_t u = std::stoull(token, &pos);
        if (pos == token.size()) {
            ParsedValue out;
            out.kind = ValueKind::Int;
            out.u_mag = u;
            return out;
        }
    } catch (const std::exception&) {
    }

    ParsedValue out;
    out.kind = ValueKind::String;
    return out;
}

inline MinimalType minimalUnsignedType_(uint64_t max_u)
{
    if (max_u <= std::numeric_limits<uint8_t>::max()) {
        return MinimalType::uint8_t;
    }
    if (max_u <= std::numeric_limits<uint16_t>::max()) {
        return MinimalType::uint16_t;
    }
    if (max_u <= std::numeric_limits<uint32_t>::max()) {
        return MinimalType::uint32_t;
    }
    return MinimalType::uint64_t;
}

inline MinimalType minimalSignedType_(int64_t min_s, int64_t max_s)
{
    if (min_s >= std::numeric_limits<int8_t>::min() &&
        max_s <= std::numeric_limits<int8_t>::max()) {
        return MinimalType::int8_t;
    }
    if (min_s >= std::numeric_limits<int16_t>::min() &&
        max_s <= std::numeric_limits<int16_t>::max()) {
        return MinimalType::int16_t;
    }
    if (min_s >= std::numeric_limits<int32_t>::min() &&
        max_s <= std::numeric_limits<int32_t>::max()) {
        return MinimalType::int32_t;
    }
    if (min_s >= std::numeric_limits<int64_t>::min() &&
        max_s <= std::numeric_limits<int64_t>::max()) {
        return MinimalType::int64_t;
    }
    throw simdb::DBException("Integer field value out of int64_t range");
}

inline MinimalType minimalFloatType_(double min_v, double max_v)
{
    if (min_v >= static_cast<double>(std::numeric_limits<float>::lowest()) &&
        max_v <= static_cast<double>(std::numeric_limits<float>::max())) {
        return MinimalType::float_t;
    }
    return MinimalType::double_t;
}

inline void applyHexPrefixLock_(FieldTypeState& state)
{
    if (state.decimal_locked) {
        throw simdb::DBException(
            "Field value format diverged: hex literal after decimal sample");
    }
    state.hex_prefix_locked = true;
}

inline void applyDecimalPrefixLock_(FieldTypeState& state)
{
    if (state.hex_prefix_locked) {
        throw simdb::DBException(
            "Field value format diverged: decimal literal after hex-prefixed sample");
    }
    state.decimal_locked = true;
}

inline void absorbInteger_(FieldTypeState& state, const ParsedValue& parsed)
{
    if (state.is_float) {
        const double dv = parsed.is_negative
                              ? static_cast<double>(parsed.s_val)
                              : static_cast<double>(parsed.u_mag);
        state.float_min = std::min(state.float_min, dv);
        state.float_max = std::max(state.float_max, dv);
        state.type = minimalFloatType_(state.float_min, state.float_max);
        return;
    }

    if (parsed.is_negative) {
        if (!state.seen_negative) {
            state.seen_negative = true;
            state.min_s = parsed.s_val;
            state.max_s = state.initialized
                              ? static_cast<int64_t>(state.max_u)
                              : parsed.s_val;
            state.max_s = std::max(state.max_s, parsed.s_val);
        } else {
            state.min_s = std::min(state.min_s, parsed.s_val);
            state.max_s = std::max(state.max_s, parsed.s_val);
        }
    } else if (state.seen_negative) {
        if (parsed.u_mag > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            throw simdb::DBException(
                "Integer field value cannot be represented after negative samples");
        }
        const int64_t mag = static_cast<int64_t>(parsed.u_mag);
        state.max_s = state.initialized ? std::max(state.max_s, mag) : mag;
    } else {
        state.max_u = state.initialized
                          ? std::max(state.max_u, parsed.u_mag)
                          : parsed.u_mag;
    }

    if (state.seen_negative || state.min_s < 0) {
        state.type = minimalSignedType_(state.min_s, state.max_s);
    } else {
        state.type = minimalUnsignedType_(state.max_u);
    }
}

inline void absorbFloat_(FieldTypeState& state, const ParsedValue& parsed)
{
    const bool promoting_from_integer = state.initialized && !state.is_float;
    state.is_float = true;
    if (!state.initialized || promoting_from_integer) {
        if (promoting_from_integer) {
            if (state.seen_negative || state.min_s < 0) {
                state.float_min = static_cast<double>(state.min_s);
                state.float_max = static_cast<double>(state.max_s);
            } else {
                state.float_min = 0.0;
                state.float_max = static_cast<double>(state.max_u);
            }
            state.float_min = std::min(state.float_min, parsed.float_val);
            state.float_max = std::max(state.float_max, parsed.float_val);
        } else {
            state.float_min = parsed.float_val;
            state.float_max = parsed.float_val;
        }
    } else {
        state.float_min = std::min(state.float_min, parsed.float_val);
        state.float_max = std::max(state.float_max, parsed.float_val);
    }
    state.type = minimalFloatType_(state.float_min, state.float_max);
}

inline void mergeFieldValue_(FieldTypeState& state, const std::string& raw_token)
{
    const std::string token = trimToken_(raw_token);
    if (token.empty()) {
        throw simdb::DBException("Empty field value");
    }

    if (state.initialized && state.type == MinimalType::string_t) {
        return;
    }

    bool bool_val = false;
    if (isBoolLiteral_(token, &bool_val)) {
        if (!state.initialized) {
            state.initialized = true;
            state.type = MinimalType::bool_t;
            return;
        }
        if (state.type != MinimalType::bool_t) {
            throw simdb::DBException(
                "Field type diverged: non-boolean field cannot accept boolean value");
        }
        return;
    }

    if (state.initialized && state.type == MinimalType::bool_t) {
        throw simdb::DBException(
            "Field type diverged: boolean field cannot accept non-boolean value");
    }

    const ParsedValue parsed = classifyToken_(raw_token);
    if (parsed.kind == ValueKind::String) {
        state.initialized = true;
        state.type = MinimalType::string_t;
        return;
    }

    if (hasHexPrefix_(token)) {
        applyHexPrefixLock_(state);
    } else {
        applyDecimalPrefixLock_(state);
    }

    if (!state.initialized) {
        state.initialized = true;
        if (parsed.kind == ValueKind::Float) {
            absorbFloat_(state, parsed);
        } else {
            absorbInteger_(state, parsed);
        }
        return;
    }

    if (parsed.kind == ValueKind::Float) {
        absorbFloat_(state, parsed);
    } else {
        absorbInteger_(state, parsed);
    }
}

} // namespace dynamic_field_type

class DynamicDataType
{
public:
    using MinimalType = sparta::collection::detail::MinimalType;

    DynamicDataType(simdb::DatabaseManager* db_mgr,
                    simdb::argos::CollectionEntryPoint* entry_point,
                    sparta::BitBucket* bit_bucket,
                    simdb::argos::ArgosCollector* argos_collector,
                    uint16_t cid)
        : db_mgr_(db_mgr)
        , entry_point_(entry_point)
        , bit_bucket_(bit_bucket)
        , argos_collector_(argos_collector)
        , cid_(cid)
    {}

    class ParserBase
    {
    public:
        ParserBase(simdb::argos::CollectionEntryPoint* entry_point,
                   sparta::BitBucket* bit_bucket,
                   simdb::argos::ArgosCollector* argos_collector,
                   uint16_t cid)
            : entry_point_(entry_point)
            , bit_bucket_(bit_bucket)
            , argos_collector_(argos_collector)
            , cid_(cid)
        {}

        virtual ~ParserBase() = default;
        virtual void parseAndDump(const std::string& stringified_fields) = 0;

        const std::vector<MinimalType>& getFieldTypes() const
        {
            return field_types_;
        }

    protected:
        void validateFieldNames_(const std::vector<std::string>& field_names)
        {
            if (field_names_.empty()) {
                field_names_ = field_names;
            } else if (field_names_ != field_names) {
                throw simdb::DBException(
                    "Field layout diverged from the first stringified_fields sample");
            }
        }

        bool updateFieldTypes_(const std::vector<std::string>& field_values)
        {
            auto prev_field_types = field_types_;

            if (field_type_states_.size() != field_values.size()) {
                field_type_states_.assign(field_values.size(), {});
                field_types_.assign(field_values.size(), MinimalType::string_t);
            }

            for (size_t i = 0; i < field_values.size(); ++i) {
                dynamic_field_type::mergeFieldValue_(
                    field_type_states_[i], field_values[i]);
                field_types_[i] = field_type_states_[i].type;
            }

            return field_types_ != prev_field_types;
        }

        void parseValidateAndInfer_(
            const std::string& stringified_fields,
            const dynamic_format_regex::FieldParseStyle style)
        {
            dynamic_format_regex::parseFields_(
                stringified_fields, style,
                parsed_field_names_, parsed_field_values_);
            validateFieldNames_(parsed_field_names_);

            if (updateFieldTypes_(parsed_field_values_)) {
                auto stager = argos_collector_->getStager();
                stager->postDynamicFieldChanges(cid_, parsed_field_names_, field_types_);
            }

            std::vector<char> bytes;
            simdb::StreamBuffer buf(bytes, argos_collector_->getTinyStrings());

            sparta_assert(parsed_field_values_.size() == field_types_.size());
            for (size_t i = 0; i < field_types_.size(); ++i) {
                const std::string& field_val_str = parsed_field_values_[i];
                const auto field_type = field_types_[i];
                switch (field_type) {
                    case MinimalType::bool_t:   dumpBool_          (buf, field_val_str); break;
                    case MinimalType::int8_t:   dumpInt_<int8_t>   (buf, field_val_str); break;
                    case MinimalType::int16_t:  dumpInt_<int16_t>  (buf, field_val_str); break;
                    case MinimalType::int32_t:  dumpInt_<int32_t>  (buf, field_val_str); break;
                    case MinimalType::int64_t:  dumpInt_<int64_t>  (buf, field_val_str); break;
                    case MinimalType::uint8_t:  dumpInt_<uint8_t>  (buf, field_val_str); break;
                    case MinimalType::uint16_t: dumpInt_<uint16_t> (buf, field_val_str); break;
                    case MinimalType::uint32_t: dumpInt_<uint32_t> (buf, field_val_str); break;
                    case MinimalType::uint64_t: dumpInt_<uint64_t> (buf, field_val_str); break;
                    case MinimalType::float_t:  dumpFlt_<float>    (buf, field_val_str); break;
                    case MinimalType::double_t: dumpFlt_<double>   (buf, field_val_str); break;
                    case MinimalType::string_t: dumpString_        (buf, field_val_str); break;
                }
            }

            if (entry_point_) {
                entry_point_->setScalarValueBytes(bytes);
            } else if (bit_bucket_) {
                bit_bucket_->dump(bytes.data(), bytes.size());
            }
        }

        void dumpBool_(simdb::StreamBuffer& buf, const std::string& bool_str) const
        {
            sparta_assert(bool_str == "true" || bool_str == "false");
            buf.append(bool_str == "true");
        }

        template <typename IntType>
        void dumpInt_(simdb::StreamBuffer& buf, const std::string& int_str) const
        {
            static_assert(std::is_integral_v<IntType> && std::is_scalar_v<IntType>);

            size_t end_pos;
            auto int_val = utils::smartLexicalCast<IntType>(int_str, end_pos);
            buf.append(int_val);
        }

        template <typename FloatType>
        void dumpFlt_(simdb::StreamBuffer& buf, const std::string& flt_str) const
        {
            static_assert(std::is_floating_point_v<FloatType> && std::is_scalar_v<FloatType>);

            size_t end_pos;
            auto flt_val = utils::smartLexicalCast<FloatType>(flt_str, end_pos);
            buf.append(flt_val);
        }

        void dumpString_(simdb::StreamBuffer& buf, const std::string& s) const
        {
            auto sid = getTinyStringID_(s);
            buf.append(sid);
        }

        uint32_t getTinyStringID_(const std::string& s) const
        {
            // Real-world use:
            if (entry_point_ || bit_bucket_) {
                auto tiny_strings =  entry_point_ ? entry_point_->getTinyStrings() : bit_bucket_->getTinyStrings();
                return tiny_strings->getStringID(s);
            }

            // Test-only use:
            return simdb::TinyStrings<>::BAD_STRING_ID;
        }

        simdb::argos::CollectionEntryPoint* const entry_point_;
        sparta::BitBucket* const bit_bucket_;
        simdb::argos::ArgosCollector* const argos_collector_;
        const uint16_t cid_;

        std::vector<std::string> field_names_;
        std::vector<std::string> parsed_field_names_;
        std::vector<std::string> parsed_field_values_;
        std::vector<MinimalType> field_types_;
        std::vector<dynamic_field_type::FieldTypeState> field_type_states_;
    };

    //! This class handles legacy operator<< that follows this format:
    //!   field1(val1) field2(val2) ... fieldN(valN)
    class ParserImpl1 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl1Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ParenSpace);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   field1(val1), field2(val2), ..., fieldN(valN)
    class ParserImpl2 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl2Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ParenComma);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   field1:val1 field2:val2 ... fieldN:valN
    class ParserImpl3 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl3Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ColonSpace);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   field1:val1, field2:val2, ..., fieldN:valN
    class ParserImpl4 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl4Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ColonComma);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   val1 val2 ... valN
    class ParserImpl5 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl5Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ValueSpace);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   val1, val2, ..., valN
    class ParserImpl6 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl6Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ValueComma);
        }
    };

    //! This class handles legacy operator<< that follows this format:
    //!   [val1,val2,...,valN]
    class ParserImpl7 : public ParserBase
    {
    public:
        using ParserBase::ParserBase;
        static bool isValidFormat(const std::string& stringified_fields) {
            return dynamic_format_regex::fullMatch(
                stringified_fields, dynamic_format_regex::parserImpl7Pattern());
        }
        void parseAndDump(const std::string& stringified_fields) override final {
            sparta_assert(isValidFormat(stringified_fields));
            parseValidateAndInfer_(
                stringified_fields,
                dynamic_format_regex::FieldParseStyle::ValueBracket);
        }
    };

    static std::unique_ptr<ParserBase> createParser(
        simdb::argos::CollectionEntryPoint* const entry_point,
        sparta::BitBucket* const bit_bucket,
        simdb::argos::ArgosCollector* argos_collector,
        const std::string& stringified_fields,
        const uint16_t cid)
    {
        if (ParserImpl7::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl7>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl6::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl6>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl5::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl5>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl4::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl4>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl3::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl3>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl2::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl2>(entry_point, bit_bucket, argos_collector, cid);
        }
        if (ParserImpl1::isValidFormat(stringified_fields)) {
            return std::make_unique<ParserImpl1>(entry_point, bit_bucket, argos_collector, cid);
        }

        throw simdb::DBException("Invalid operator<< format");
        return nullptr;
    }

    void parseAndDump(const std::string& stringified_fields)
    {
        if (!parser_) {
            parser_ = createParser(entry_point_, bit_bucket_, argos_collector_, stringified_fields, cid_);
        }
        parser_->parseAndDump(stringified_fields);
    }

private:
    simdb::DatabaseManager* const db_mgr_;
    simdb::argos::CollectionEntryPoint* const entry_point_;
    sparta::BitBucket* const bit_bucket_;
    simdb::argos::ArgosCollector *const argos_collector_;
    const uint16_t cid_;
    std::unique_ptr<ParserBase> parser_;
};

} // namespace sparta::collection::detail
