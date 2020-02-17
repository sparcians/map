// <BaseFormatter> -*- C++ -*-

/*!
 * \file BaseFormatter.hpp
 * \brief Basic HTML Report output formatter
 */

#ifndef __SPARTA_REPORT_FORMAT_BASE_FORMATTER_H__
#define __SPARTA_REPORT_FORMAT_BASE_FORMATTER_H__

#include <math.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
    namespace trigger {
        class SkippedAnnotatorBase;
    }
}

namespace sparta {
class Scheduler;

    /*!
     * \brief Namespace containing report data structures
     */
    namespace report {
        /*!
         * \brief Namespace containing report formatters
         */
        namespace format {

// Forward declaration
class BaseFormatter;

/*!
 * \brief Defines a single Formatter Factory
 */
class FormatterFactory {
public:
    /*!
     * \brief File extensions associated with this factory
     * \note All extensions must be lower case
     * \note An empty exts fields implies the end of the static
     * BaseFormatter::FACTORIES vector
     */
    std::vector<std::string> exts;

    /*!
     * \brief Description of this factory
     */
    const std::string desc;

    /*!
     * \brief Factory function pointer.
     * \note Returns new formatter allocated using 'new'. Caller is responsible
     * for deleting
     */
    std::function<BaseFormatter* (const Report*, const std::string&)> factory;
};

/*!
 * \brief Pure Virtual Report formatter for sparta Reports output
 * \note Non-Copyable
 * \note A formatter must be constructed pointing to 1 report, persistent
 * the lifetime of the formatter
 *
 * Formatters generally expect an ended report, but there are no strict rules
 * about the behavior of a formatter when the report it points to changes its
 * data.
 *
 * Changes to the structure (not values) of the report pointed to by this
 * formatter after construction of this formatter are undefined.
 * Formatters are NOT required to respect any changes to the structure of
 * their referenced reports after construction, but may optionally do so.
 */
class BaseFormatter
{
public:

    //! \name Factories
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Factories usable for this formatter
     * \note Must have 1 or more entries.
     * \nost Must end in an entry with an empty exts field
     *
     * The factory at index 0 is the default.
     */
    static const std::vector<FormatterFactory> FACTORIES;

    /*!
     * \brief Selects the appropriate factory for the given output target by
     * comparing the \a format parameter (if not "") against all
     * extensions in each Factory in FACTORIES. If format is not "", then
     * alternatively compares filename against all extensions (with an added '.'
     * prefix) in each Factory in FACTORIES. If no matches are found, returns
     * the default (first Factory in FACTORIES).
     * \note Never searches with both filename and format. Always uses format if
     * not "", and filename if format="".
     * \param lower_filename Lowercase filename for extension matching if \a
     * format is ""
     * \param format Optional format. If not "", this is tested against
     * all extensions in the exts fields of each Factory in FACTORIES.
     */
    static const FormatterFactory*
        determineFactory(const std::string& lower_filename,
                         const std::string& format="");

    /*!
     * \brief Is ths given format string a valid formatter.
     * \return true if format is an exact match for some factory in FACTORIES
     * \note empty format strings will always return false.
     * \note If this returns true, then determineFactory is guaranteed to
     * succeed and return a non-null pointer.
     */
    static bool isValidFormatName(const std::string& format);

    /*!
     * \brief Dumps the supported format keys for reports availble through
     * FACTORIES
     */
    static std::ostream& dumpFormats(std::ostream& o) {
        for(auto& fmtfac : FACTORIES){
            if(fmtfac.exts.size() != 0){
                o << "  ";
                uint32_t idx = 0;
                for(auto& ext : fmtfac.exts){
                    o << ext;
                    ++idx;
                    if(idx != fmtfac.exts.size()){
                        o << ", ";
                    }
                }
                o << " -> " << fmtfac.desc << std::endl;
            }
        }

        return o;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Construction
    //! @{
    ////////////////////////////////////////////////////////////////////////

    //! \brief No default construction
    BaseFormatter() = delete;

    //! \brief No copy construction
    BaseFormatter(const BaseFormatter&) = delete;

    //! \brief No move construction
    BaseFormatter(BaseFormatter&&) = delete;

    //! \brief No assignment
    BaseFormatter& operator=(const BaseFormatter&) = delete;


    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for. This report must exist
     * but the report content can change over time
     * for the lifetime of this Formatter if not nullptr
     */
    BaseFormatter(const Report* r) :
        report_(r)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~BaseFormatter()
    { }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Accessors
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Returns the report with which this formatter was built
     */
    const Report* getReport() const {
        return report_;
    }

    /*!
     * \brief Returns the scheduler tied to this report's tree node context
     */
    const Scheduler* getScheduler(const bool must_exist = true) const {
        sparta_assert(report_);
        auto scheduler = report_->getScheduler();
        sparta_assert(scheduler || !must_exist);
        return scheduler;
    }

    /*!
     * \brief Get the current targert of this formatter (if any)
     * \return Current target if any. nullptr if none
     */
    virtual std::string getTarget() const = 0;

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Output
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Metadata is stored in key-value pairs of strings. This
     * method will not check if the given metadata name already exists
     * in the metadata map, it will simply overwrite its value.
     */
    void setMetadataByNameAndStringValue(
        const std::string & name,
        const std::string & value)
    {
        metadata_kv_pairs_[name] = value;
    }

    //! Give the reporting infrastructure access to all metadata that
    //! has been set. The database report writers need this metadata, and
    //! others may need it as well.
    const std::map<std::string, std::string> & getMetadataKVPairs() const {
        return metadata_kv_pairs_;
    }

    //! Turn off pretty print formatting. Note that some report formats
    //! do not differentiate between pretty and normal formatting. Calling
    //! this method may have no effect.
    void disablePrettyPrint() {
        pretty_print_enabled_ = false;
    }

    //! See if pretty print formatting is enabled or not (note that enabled
    //! is the default)
    bool prettyPrintEnabled() const {
        return pretty_print_enabled_;
    }

    //! Tell this formatter to omit StatisticInstance's from the report if
    //! they have a value of 0. Not all formatters support this behavior,
    //! and if not, they will simply ignore this request. No warning or
    //! exception will occur.
    void omitStatsWithValueZero() {
        zero_si_values_omitted_ = true;
    }

    //! See if this formatter has been told to omit all statistics that
    //! have value 0 from the report.
    bool statsWithValueZeroAreOmitted() const {
        return zero_si_values_omitted_;
    }

    /*!
     * \brief Append the content of this report to some output. Has ios::app
     * semantics. Effectively equivalent to writeHeader() then update()
     * \post This should typically flush all written output to whatever output
     * is being written to.
     * \note This is technically an append call. As such, it is expected to
     * leave contents of target in tact. Caller must clear the target if desired
     * using some external means. For file targets, this should be simple. For
     * non-file targets such as standard streams and databases, clearing does
     * not make sense.
     * targets write semantics (i.e. files are cleared before writing).
     * \throw SpartaException if getReport() is nullptr
     */
    void write() const {
        if(nullptr == report_){
            throw SpartaException("Attempting to write through a Report Formatter without a valid "
                                "Report pointer");
        }
        writeHeader_();
        writeContent_();
    }

    /*!
     * \brief Appends the content of this report to some output target. Has
     * ios::app semantics. Effectively equivalent to writeHeaderTo() then
     * updateTo()
     * \param target Output name. This can be a file name or something else (
     * e.g. URL, pipe). This string is interpreted based on the formatter
     * subclass
     * \note This is technically an append call. As such, it is expected to
     * leave contents of target in tact. Caller must clear the target if desired
     * using some external means. For file targets, this should be simple. For
     * non-file targets such as standard streams and databases, clearing does
     * not make sense.
     * \post This should typically flush all written output to the \a target
     * file
     * \throw SpartaException if getReport() is nullptr
     * \pre report_ will not be nullptr;
     */
    void writeTo(const std::string& target) const {
        if(nullptr == report_){
            throw SpartaException("Attempting to writeTo through a Report Formatter without a valid "
                                "Report pointer. Target was \"") << target << "\"";
        }
        writeHeaderTo_(target);
        writeContentTo_(target);
    }

    /*!
     * \brief Appends header information for the current report to the current
     * output target.
     * \pre Formatter must support updating (supportsUpdate)
     * \note This is not necessary if write/writeTo is used instead
     */
    void writeHeader() const {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to writeHeader through a Report Formatter which does "
                                "not support updates. use write[To] instead");
        }
        if(nullptr == report_){
            throw SpartaException("Attempting to writeHeader through a Report Formatter without a "
                                "valid Report pointer");
        }
        writeHeader_();
    }

    /*!
     * \brief Appends header information for the current report to the output
     * target.
     * \pre Formatter must support updating (supportsUpdate)
     * \note This is not necessary if write/writeTo is used instead
     * \param target Output name. This can be a file name or something else (
     * e.g. URL, pipe). This string is interpreted based on the formatter
     * subclass
     */
    void writeHeaderTo(const std::string& target) const {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to writeHeader through a Report Formatter which does "
                                "not support updates. use write[To] instead. Target was \"")
                                << target << "\"";
        }
        if(nullptr == report_){
            throw SpartaException("Attempting to writeHeaderTo through a Report Formatter without a "
                                "valid Report pointer. Target was \"") << target << "\"";
        }
        writeHeaderTo_(target);
    }

    /*!
     * \brief Get the header name/value pairs that were written out,
     * in the order they were written out
     */
    const std::vector<std::string> & getWrittenHeaderLines() const {
        return written_header_lines_;
    }

    /*!
     * \brief Update the destination with new report data.
     * \pre Formatter must support updating (supportsUpdate)
     *
     * In some cases this is the same as a call to write. However, the formatter may handle
     * update_ calls differently for some output. Typically, the write function will contain initial
     * data while update will cause report data to be appended (if supported) in the form of
     * additional rows or the like.
     */
    void update() const {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to update through a Report Formatter which does not "
                                "support updates");
        }
        if(nullptr == report_){
            throw SpartaException("Attempting to update through a Report Formatter without a valid "
                                "Report pointer");
        }
        update_();
    }


    /*!
     * \brief Update the destination with new report data.
     * \pre Formatter must support updating (supportsUpdate).
     * \param target Output name. This can be a file name or something else (
     * e.g. URL, pipe). This string is interpreted based on the formatter
     * subclass
     *
     * In some cases this is the same as a call to writeTo. However, the formatter may handle
     * updateTo_ calls differently for some output. This is especially important for updateTo,
     * where the formatter does not know if the target file has been written initially or is empty.
     */
    void updateTo(const std::string& target) const {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to updateTo through a Report Formatter which does not "
                                "support updates. Target was \"") << target << "\"";
        }
        if(nullptr == report_){
            throw SpartaException("Attempting to updateTo through a Report Formatter without a valid "
                                "Report pointer. Target was \"") << target << "\"";
        }
        updateTo_(target);
    }

    /*!
     * \brief Does this formatter support update methods (update/updateTo).
     * If false, this is just a one-off formatter
     */
    virtual bool supportsUpdate() const {
        return false;
    }

    /*!
     * \brief Inform the destination that it should skip over the given number
     * of updates with empty / null report data.
     */
    void skip(const sparta::trigger::SkippedAnnotatorBase * annotator) const {
        if (!supportsUpdate()) {
            throw SpartaException(
                "Attempting to skip through a Report Formatter which does not support updates");
        }
        sparta_assert(report_ != nullptr);
        skip_(annotator);
    }

    /*!
     * \brief Optionally get a chance to reset any internal data *after*
     * the simulation's ReportDescriptor(s) have been written out to file
     * or the database, but *before* post-simulation report verification
     * is performed.
     */
    virtual void doPostProcessingBeforeReportValidation() {}

    ////////////////////////////////////////////////////////////////////////
    //! @}

protected:

    /*!
     * \brief Writes the content of this report to some output. Subclasses must
     * implement this functionality
     * \post This should typically flush all written output to whatever output
     * is being written to.
     */
    virtual void writeContent_() const = 0;

    /*!
     * \brief Writes the content of this report to some output. Subclasses must
     * implement this functionality
     * \param target Output name. This can be a file name or something else(
     * e.g. URL, pipe). This string is interpreted based on the formatter
     * subclass
     * \post This should typically flush all written output to the \a target
     * file
     * \pre report_ will not be nullptr;
     */
    virtual void writeContentTo_(const std::string& target) const = 0;

    /*!
     * \brief Writes the heading of this report to some output. Subclasses must
     * implement this functionality
     * \note Only needs to be overridden if supportsUpdate is true
     * \post This should typically flush all written output to whatever output
     * is being written to.
     * \pre report_ will not be nullptr;
     */
    virtual void writeHeader_() const = 0;

    /*!
     * \brief Writes the heading of this report to some output. Subclasses must
     * implement this functionality
     * \note Only needs to be overridden if supportsUpdate is true
     * \post This should typically flush all written output to whatever output
     * is being written to.
     * \pre report_ will not be nullptr;
     */
    virtual void writeHeaderTo_(const std::string& target) const = 0;

    /*!
     * \brief Writes the content of this report to some output.
     * \note Only needs to be overridden if supportsUpdate is true
     * \post This should typically flush all written output to whatever output
     * is being written to.
     * \pre report_ will not be nullptr;
     */
    virtual void update_() const {
        throw SpartaException("update_ called on a ReportFormatter but the method was not implemented");
    }

    /*!
     * \brief Writes the content of this report to some output.
     * \note Only needs to be overridden if supportsUpdate is true
     * \param target Output name. This can be a file name or something else(
     * e.g. URL, pipe). This string is interpreted based on the formatter
     * subclass
     * \post This should typically flush all written output to the \a target
     * file
     * \pre report_ will not be nullptr;
     */
    virtual void updateTo_(const std::string& target) const {
        throw SpartaException("updateTo_ called on a ReportFormatter but the method was not implemented.")
              << " Target was \"" << target << "\"";
    }

    virtual void skip_(const sparta::trigger::SkippedAnnotatorBase *) const {
        throw SpartaException(
            "skip_ called on a ReportFormatter but the method was not implemented.");
    }

    /*!
     * \brief Keep track of the metadata values written. There can
     * be more than one series of metadata values, so we keep these
     * in a vector of strings.
     */
    void recordWrittenMetadata_(const size_t header_row_idx,
                                const std::string & header_line) const
    {
        while (written_header_lines_.size() <= header_row_idx) {
            written_header_lines_.emplace_back("");
        }
        written_header_lines_[header_row_idx] = header_line;
    }

    /*!
     * \brief Report being formatted by this formatter
     */
    const Report* const report_;

    /*!
     * \brief Generic metadata key-value pairs for report headers
     */
    std::map<std::string, std::string> metadata_kv_pairs_;

    //! Flag telling subclasses whether they should pretty print their
    //! contents or not. Not all report formats make a distinction
    //! between pretty and normal formatting.
    bool pretty_print_enabled_ = true;

    //! Flag telling subclasses whether they should omit statistics
    //! from their report if the SI has value 0
    bool zero_si_values_omitted_ = false;

private:
    //! \brief Header variables that were written out, in the order
    //! they were written out
    mutable std::vector<std::string> written_header_lines_;
};

        } // namespace format
    } // namespace report
} // namespace sparta

// __SPARTA_REPORT_FORMAT_BASE_FORMATTER_H__
#endif
