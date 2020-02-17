// <BaseOstreamFormatter> -*- C++ -*-

/*!
 * \file BaseOstreamFormatter.hpp
 * \brief Basic HTML Report output formatter
 */

#ifndef __SPARTA_REPORT_FORMAT_BASE_OSTREAM_FORMATTER_H__
#define __SPARTA_REPORT_FORMAT_BASE_OSTREAM_FORMATTER_H__

#include <iostream>
#include <sstream>
#include <memory>
#include <math.h>
#include <ios>

#include "sparta/report/format/BaseFormatter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include <boost/algorithm/string/split.hpp>

namespace sparta
{
    namespace report
    {
        namespace format
        {

/*!
 * \brief Pure Virtual Report formatter for sparta Reports that write to ostreams
 * Formatters must inherit from this formatter if they wish to support logging
 * to stdout/stderr through sparta::app::Simulation
 * \note Any templated html output should use TemplatedHTML
 * \note Non-Copyable
 */
class BaseOstreamFormatter : public BaseFormatter
{
public:

    //! \brief Reserved name for ostream targets
    static constexpr char OSTREAM_TARGET_NAME[] = "<ostream>";

    /*!
     * \brief Constructor with existing ostream. Has append semantics
     * \param r Report to provide output formatting for. This report must exist
     * for the lifetime of this Formatter, but the Report content can change
     * \param output ostream to write to when write() is called. Can be
     * retrieved wiht getOstream and set with setOstream
     */
    BaseOstreamFormatter(const Report* r, std::ostream& output) :
        BaseFormatter(r),
        output_(&output),
        filename_(OSTREAM_TARGET_NAME)
    { }

    /*!
     * \brief Constructor which opens file. Has append semantics by default,
     * but \a mode can be used to specify how the file is opened.
     * \param r Report to provide output formatting for. This report must exist
     * for the lifetime of this Formatter, but the Report content can change
     * \param output ostream to write to when write() is called. Can be
     * retrieved wiht getOstream and set with setOstream
     * \param filename File which will be opened. This file's ostream will be
     * used as the default content. If filename is "", does not open an ofstream
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     * \throw If file cannot be opened, throws SpartaException
     */
    BaseOstreamFormatter(const Report* r,
                         const std::string& filename,
                         std::ios::openmode mode=std::ios::app) :
        BaseFormatter(r),
        outfile_(filename=="" ? nullptr : new std::ofstream(filename, mode)),
        output_(outfile_.get()),
        filename_(filename)
    {
        if(!outfile_ && filename != ""){
            throw SpartaException("Failed to open file \"") << filename << "\" for storing report";
        }
        if(outfile_){
            // Throw on write failure
            outfile_->exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        }
    }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for. This report must exist
     * for the lifetime of this Formatter
     */
    BaseOstreamFormatter(const Report* r) :
        BaseFormatter(r),
        output_(nullptr)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~BaseOstreamFormatter()
    { }

    /*!
     * \brief Returns the report with which this formatter will write
     */
    std::ostream* getOstream() {
        return output_;
    }

    /*!
     * \brief Sets the ostream to which this formatter will write
     * \param output Output stream to write to from this point on
     * \param filename Filename to return when getTarget is queried. This will
     * typically be stdout or stderr
     */
    std::ostream* setOstream(std::ostream* output,
                             const std::string& filename=OSTREAM_TARGET_NAME) {
        std::ostream* prev = output_;
        output_ = output;
        filename_ = filename;
        return prev;
    }

    /*!
     * \brief Implements BaseFormatter::getTarget
     */
    virtual std::string getTarget() const override {
        return filename_;
    }

    //! \name Public Output Methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Writes report to a specific ostream. Has ios::app semantics on the
     * stream but assumes that any header content must also be written.
     * Invokes the virtual writeHeaderToStream_ and writeContentToStream_
     * methods
     * \post \a out will be flushed after writing
     */
    void writeToStream(std::ostream& out) const {
        if(nullptr == report_){
            throw SpartaException("Attempting to write through a BaseOStreamFormatter without a "
                                "valid Report pointer");
        }
        writeHeaderToStream_(out);
        writeContentToStream_(out);
        out.flush();
    };

    /*!
     * \brief Writes report content to a specific ostream. Has ios::app
     * semantics on the stream and assumes that any header content must have
     * already been written
     * Invokes the virtual writeContentToStream_ method.
     * \post \a out will be flushed after writing
     */
    void writeContentToStream(std::ostream& out) const {
        if(nullptr == report_){
            throw SpartaException("Attempting to write through a BaseOStreamFormatter without a "
                                "valid Report pointer");
        }
        writeContentToStream_(out);
        out.flush();
    };


    /*!
     * \brief Writes report header to a specific ostream. Has ios::app semantics
     * on the stream and assumes that no header has been written yet.
     * Invokes the virtual writeHeaderToStream_ method.
     * \post \a out will be flushed after writing
     */
    void writeHeaderToStream(std::ostream& out) const {
        if(nullptr == report_){
            throw SpartaException("Attempting to write through a BaseOStreamFormatter without a "
                                "valid Report pointer");
        }
        writeHeaderToStream_(out);
        out.flush();
    };

    /*!
     * \brief Writes additional report to a specific ostream. Has ios::app
     * semantics on the stream.
     * Invokes the virtual writeToStream_ method.
     * \post \a out will be flushed after writing
     */
    void updateToStream(std::ostream& out) const {
        if(nullptr == report_){
            throw SpartaException("Attempting to update through a BaseOStreamFormatter without a "
                                "valid Report pointer");
        }
        updateToStream_(out);
        out.flush();
    };

    /*!
     * \brief Skips over <num_skipped> updates for a specific stream.
     */
    void skipOverStream(std::ostream& out,
                        const sparta::trigger::SkippedAnnotatorBase * annotator) const {
        if (report_ == nullptr) {
            throw SpartaException(
                "Attempting to skip through a BaseOStreamFormatter without a "
                "valid Report pointer");
        }
        skipOverStream_(out, annotator);
        out.flush();
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

protected:

    //! \name Virtual Output Methods
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Implements BaseFormatter::writeContent_
     */
    void writeContent_() const override final {
        ensureValidOutput_();
        writeContentToStream(*output_);
    }

    /*!
     * \brief Opens file named by target for append. Writes to it
     * \param target Target interpreted as a filename
     * \throw SpartaException if \a target file cannot be opened for write
     */
    void writeContentTo_(const std::string& target) const override final {
        std::ofstream os(target, std::ios::app);
        if(!os){
            throw SpartaException("Failed to open file \"") << target
                  << "\"for writing report content";
        }
        // Throw on write failure
        os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        writeContentToStream(os);
    }

    /*!
     * \brief Implements BaseFormatter::writeHeader_
     */
    void writeHeader_() const override final {
        ensureValidOutput_();
        writeHeaderToStream(header_output_);
        const std::string header_lines = header_output_.str();

        std::vector<std::string> split;
        boost::split(split, header_lines, boost::is_any_of("\n"));
        for (size_t idx = 0, jdx = 0; idx < split.size(); ++idx) {
            boost::trim_left(split[idx]);
            if (split[idx][0] != '#') {
                continue;
            }
            recordWrittenMetadata_(jdx, split[idx]);
            ++jdx;
        }

        (*output_) << header_lines;
    }

private:

    /*!
     * \brief Opens file named by target for append. Writes to it
     * \param target Target interpreted as a filename
     * \throw SpartaException if \a target file cannot be opened for write
     */
    virtual void writeHeaderTo_(const std::string& target) const override final {
        std::ofstream os(target, std::ios::app);
        if(!os){
            throw SpartaException("Failed to open file \"") << target
                  << "\"for writing report header";
        }
        // Throw on write failure
        os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        writeHeaderToStream(os);
    }

    /*!
     * \brief Implements BaseFormatter::update_
     */
    virtual void update_() const override final {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to update_ through a Report Formatter which does not "
                                "support updates.");
        }
        ensureValidOutput_();
        updateToStream(*output_);
    }

    /*!
     * \brief Implements BaseFormatter::updateTo_
     */
    virtual void updateTo_(const std::string& target) const override final {
        if(!supportsUpdate()){
            throw SpartaException("Attempting to updateTo_ through a Report Formatter which does not "
                                "support updates. Target was \"") << target << "\"";
        }
        std::ofstream os(target, std::ios::app);
        if(!os){
            throw SpartaException("Failed to open file \"") << target << "\"for storing report";
        }
        // Throw on write failure
        os.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        updateToStream(os);
    }

    /*!
     * \brief Implements BaseFormatter::skip_
     */
    virtual void skip_(const sparta::trigger::SkippedAnnotatorBase * annotator) const override final {
        if (!supportsUpdate()) {
            throw SpartaException(
                "Attempting to skip through a Report Formatter which does not support updates");
        }
        ensureValidOutput_();
        skipOverStream(*output_, annotator);
    }

    /*!
     * \brief Writes to a specific ostream. Subclasses must override
     * \post Assumes new file being written and writes any initial content.
     */
    virtual void writeContentToStream_(std::ostream& out) const = 0;

    /*!
     * \brief Writes output header information to a specific ostream. Subclasses
     * must override
     * \post Assumes new file being written and writes any initial content.
     */
    virtual void writeHeaderToStream_(std::ostream& out) const = 0;

    /*!
     * \brief Updates a specific ostream with current report data. Subclasses
     * must override
     * \post Assumes file header has already been written and only new data must
     * be added
     */
    virtual void updateToStream_(std::ostream& out) const {
        (void) out;
        throw SpartaException("updateToStream called on a BaseOstreamFormatter but the method was "
                            "not implemented");
    }

    /*!
     * \brief Updates a specific ostream to skip the given number of updates.
     */
    virtual void skipOverStream_(std::ostream&,
                                 const sparta::trigger::SkippedAnnotatorBase *) const {
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Ensures that the ostream contained within is valid and ready to
     * bet written to. Throws if not.
     */
    void ensureValidOutput_() const {
        if(nullptr == output_){
            throw SpartaException("Cannot write() on a report formatter without a valid output "
                                "stream. Either construct with one or set through setOstream");
        }
    }

    /*!
     * \brief File stream instantiated by this formatter if constructed with a
     * filename string
     */
    std::unique_ptr<std::ofstream> outfile_;

    /*!
     * \brief Output stream being written by this formatter
     */
    std::ostream* output_;

    /*!
     * \brief Output stream this formatter writes header
     * information into. Supports SimDB-regenerated timeseries
     * reports.
     */
    mutable std::ostringstream header_output_;

    /*!
     * \brief Filename to report when getTarget is called
     */
    std::string filename_;
};

//! \brief ReportFormatter stream operator
template<class Ch,class Tr>
inline std::basic_ostream<Ch,Tr>&
operator<< (std::basic_ostream<Ch,Tr>& out, const BaseOstreamFormatter & f) {
    f.writeToStream(out);
    return out;
}

//! \brief TreeNode stream operator
template<class Ch,class Tr>
inline std::basic_ostream<Ch,Tr>&
operator<< (std::basic_ostream<Ch,Tr>& out, const BaseOstreamFormatter * f) {
    if(nullptr == f){
        out << "null ostream formatter";
    }else{
        out << *f; // Use the other operator
    }
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta

// __SPARTA_REPORT_FORMAT_BASE_OSTREAM_FORMATTER_H__
#endif
