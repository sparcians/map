// <ReportHeader> -*- C++ -*-

/*!
 * \file ReportHeader.hpp
 * \brief Utility class to be used with CSV report formatters. This class
 * re-writes CSV header values at any point during simulation. Only integral
 * header values can be overwritten however; string header values are locked
 * after the first call to the 'writeHeaderToStreams()' method.
 */

#ifndef __SPARTA_REPORT_HEADER_H__
#define __SPARTA_REPORT_HEADER_H__

#include "sparta/utils/SpartaException.hpp"

#include <type_traits>
#include <iostream>
#include <iomanip>

namespace sparta {
namespace report {
namespace format {

class ReportHeaderContent
{
private:
    template <typename T,
              typename = typename std::enable_if<std::is_integral<T>::value>::type>
    void set_(const std::string & name, const T value)
    {
        static_assert(std::is_unsigned<T>::value,
                      "Integral header values must be greater than or equal to zero. "
                      "Signed integer types are not supported.");
        checkHasMapKey_(name);
        checkNoWhiteSpace_(name);
        checkNotExistingString_(name, static_cast<uint64_t>(value));
        integral_values_[name] = static_cast<uint64_t>(value);
    }

    void set_(const std::string & name, const std::string & value)
    {
        if (map_keys_locked_) {
            throw SpartaException(
                "You may not update string header values after "
                "'ReportHeader::writeHeaderToStreams()' has been "
                "called. Only numeric values can be changed at that point.");
        }
        checkNoWhiteSpace_(name);
        checkNotExistingInteger_(name, value);
        text_values_[name] = value;
    }

    void checkHasMapKey_(const std::string & name) const
    {
        if (!map_keys_locked_) {
            return;
        }

        const bool no_integral_with_name =
            (integral_values_.find(name) == integral_values_.end());

        const bool no_string_with_name =
            (text_values_.find(name) == text_values_.end());

        const bool is_new_header_value =
            (no_integral_with_name && no_string_with_name);

        if (is_new_header_value) {
            throw SpartaException(
                "You may not add new values to report headers once "
                "the 'writeHeaderToStream()' method is called. Values may "
                "only be updated. Offending header variable is '") << name << "'";
        }
    }

    void checkNoWhiteSpace_(const std::string & name) const
    {
        if (name.find(" ") != std::string::npos) {
            throw SpartaException(
                "You may not add header info with a name containing any whitespace. "
                "Offending header variable is '") << name << "'";
        }
    }

    void checkNotExistingString_(const std::string & name,
                                 const uint64_t attempted_value) const
    {
        auto iter = text_values_.find(name);
        if (iter != text_values_.end()) {
            std::ostringstream oss;
            oss << "Header assignment '" << name << "=" << attempted_value << "' "
                << "is not allowed. This has already been assigned the value '"
                << iter->second << "'. (type mismatch)";
            throw SpartaException(oss.str());
        }
    }

    void checkNotExistingInteger_(const std::string & name,
                                  const std::string & attempted_value) const
    {
        auto iter = integral_values_.find(name);
        if (iter != integral_values_.end()) {
            std::ostringstream oss;
            oss << "Header assignment '" << name << "=" << attempted_value << "' "
                << "is not allowed. This has already been assigned the value '"
                << iter->second << "'. (type mismatch)";
            throw SpartaException(oss.str());
        }
    }

    std::string stringifyContent_(const std::string & name) const
    {
        auto text_iter = text_values_.find(name);
        if (text_iter != text_values_.end()) {
            return text_iter->second;
        }

        auto int_iter = integral_values_.find(name);
        if (int_iter != integral_values_.end()) {
            std::ostringstream oss;
            oss << int_iter->second;
            return oss.str();
        }

        return "";
    }

    std::map<std::string, std::string> stringifyAllContent_() const
    {
        std::map<std::string, std::string> stringified;
        for (const auto & val : integral_values_) {
            stringified[val.first] = stringifyContent_(val.first);
        }
        for (const auto & val : text_values_) {
            stringified[val.first] = val.second;
        }
        return stringified;
    }

    ReportHeaderContent() = default;
    ReportHeaderContent(const ReportHeaderContent &) = delete;
    ReportHeaderContent & operator=(const ReportHeaderContent &) = delete;

    std::map<std::string, uint64_t> integral_values_;
    std::map<std::string, std::string> text_values_;
    bool map_keys_locked_ = false;

    friend class ReportHeaderWriter;
    friend class ReportHeader;
};

class ReportHeaderWriter
{
private:
    void writeHeaderToStream_()
    {
        const std::map<std::string, uint64_t> & integral_values = content_.integral_values_;
        const std::map<std::string, std::string> & text_values = content_.text_values_;

        const size_t total_num_vals = (integral_values.size() + text_values.size());
        if (total_num_vals == 0) {
            return;
        }

        size_t cell_index = 0;
        std::ostringstream oss_content;

        static std::ostringstream max64;
        if (max64.str().empty()) {
            max64 << std::numeric_limits<uint64_t>::max();
        }

        static const uint64_t fixed_integral_width = max64.str().size();

        std::map<std::string, std::string> entries_as_text;
        for (const auto & val : integral_values) {
            std::ostringstream entry_oss;
            entry_oss << std::left
                      << std::setfill(' ')
                      << std::setw(fixed_integral_width)
                      << val.second;
            entries_as_text[val.first] = entry_oss.str();
        }
        for (const auto & val : text_values) {
            entries_as_text[val.first] = val.second;
        }
        for (const auto & entry : entries_as_text) {
            oss_content << entry.first << "=" << entry.second;
            if (cell_index++ < total_num_vals - 1) {
                oss_content << ",";
            }
        }

        if (!header_position_.isValid()) {
            header_position_ = (uint32_t)os_.tellp();
        }
        const long current_position = os_.tellp();
        os_.seekp(header_position_.getValue());

        const std::string final_str = "# " + oss_content.str() + "\n";
        os_ << final_str;

        sparta_assert(!header_bytes_.isValid() ||
                    static_cast<size_t>(header_bytes_) == final_str.size());
        if (header_bytes_.isValid()) {
            os_.seekp(current_position);
        }
        header_bytes_ = final_str.size();
        content_.map_keys_locked_ = true;
    }

    ReportHeaderWriter(std::ostream & os,
                       ReportHeaderContent & content) :
        os_(os),
        content_(content)
    {
        if (&os_ == &std::cout) {
            throw SpartaException("You may not attach a report header object to stdout");
        }
    }

    ReportHeaderWriter(const ReportHeaderWriter &) = delete;
    ReportHeaderWriter & operator=(const ReportHeaderWriter & ) = delete;

    std::ostream & os_;
    utils::ValidValue<unsigned long> header_position_;
    utils::ValidValue<unsigned long> header_bytes_;
    ReportHeaderContent & content_;

    friend class ReportHeader;
};

class ReportHeader
{
public:
    ReportHeader() = default;
    ~ReportHeader() = default;

    template <typename T,
              typename = typename std::enable_if<std::is_integral<T>::value>::type>
    void set(const std::string & name, const T value)
    {
        content_.set_(name, value);
        writeHeaderToStreams();
    }

    void set(const std::string & name, const std::string & value)
    {
        content_.set_(name, value);
        writeHeaderToStreams();
    }

    std::string getStringified(const std::string & name) const
    {
        return content_.stringifyContent_(name);
    }

    std::map<std::string, std::string> getAllStringified() const
    {
        return content_.stringifyAllContent_();
    }

    void reservePlaceholder(const std::string & name)
    {
        set(name, std::numeric_limits<uint64_t>::max());
    }

    void attachToStream(std::ostream & os)
    {
        if (output_streams_.find(&os) == output_streams_.end()) {
            output_streams_[&os].reset(new ReportHeaderWriter(os, content_));
        }
    }

    void detachFromStream(const std::ostream & os)
    {
        auto iter = output_streams_.find(&os);
        if (iter != output_streams_.end()) {
            output_streams_.erase(iter);
        }
    }

    void writeHeaderToStreams()
    {
        for (auto & stream : output_streams_) {
            stream.second->writeHeaderToStream_();
        }
    }

private:
    ReportHeader(const ReportHeader &) = delete;
    ReportHeader & operator=(const ReportHeader &) = delete;

    ReportHeaderContent content_;
    std::map<const std::ostream*, std::unique_ptr<ReportHeaderWriter>> output_streams_;
};

} // namespace format
} // namespace report
} // namespace sparta

#endif
