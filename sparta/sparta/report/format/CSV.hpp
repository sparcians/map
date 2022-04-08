// <CSV> -*- C++ -*-

/*!
 * \file CSV.hpp
 * \brief CSV Report output formatter
 */

#pragma once

#include <iostream>
#include <sstream>
#include <math.h>

#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/report/format/ReportHeader.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/Expression.hpp" // stat_pair_t

namespace sparta
{
    namespace report
    {
        namespace format
        {

/*!
 * \brief Report formatter for CSV output
 * \note Non-Copyable
 */
class CSV : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Ostream to write to when write() is called
     */
    CSV(const Report* r, std::ostream& output) :
        BaseOstreamFormatter(r, output)
    {
    }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param filename File which will be opened and appended to when write() is
     * called
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     */
    CSV(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    {
    }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    CSV(const Report* r) :
        BaseOstreamFormatter(r)
    {
    }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~CSV()
    {
    }

    /*!
     * \brief Override from BaseFormatter
     */
    virtual bool supportsUpdate() const override {
        return true;
    }

protected:

    //! \name Output
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Writes a header to some output based on the report
     */
    virtual void writeHeaderToStream_(std::ostream& out) const override  {
        writeCSVHeader_(out, report_);
    }

    /*!
     * \brief Writes the content of this report to some output
     */
    virtual void writeContentToStream_(std::ostream& out) const override {
        writeRow_(out, report_);
    }

    /*!
     * \brief Writes updated information to the stream
     * \param out Stream to which output data will be written
     */
    virtual void updateToStream_(std::ostream& out) const override {
        writeRow_(out, report_);
    }

    /*!
     * \brief Writes out a special 'Skipped' message to the CSV file (exact message
     * will depend on how the SkippedAnnotator subclass wants to annotate this gap
     * in the report)
     */
    virtual void skipOverStream_(std::ostream& out,
                                 const sparta::trigger::SkippedAnnotatorBase * annotator) const override {
        skipRows_(out, annotator, report_);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Write formatted csv for the report to the output
     */
    void dump_(std::ostream& out,
               const Report* r) const {

        writeCSVHeader_(out, r);
        writeRow_(out, r);
    }

    /*!
     * \brief Write a header line to the report
     */
    void writeCSVHeader_(std::ostream& out,
                         const Report* r) const {
        out << "# report=\"" << r->getName() << "\",start=" << r->getStart()
            << ",end=";
        if (r->getEnd() == Scheduler::INDEFINITE) {
            out << "SIMULATION_END";
        } else {
            out << r->getEnd();
        }
        if (!metadata_kv_pairs_.empty()) {
            //Combine metadata key-value map into a single comma-
            //separated string to be added to the header row of
            //the CSV report file.
            out << "," << stringizeRunMetadata_();
        }
        out << '\n';

        if (! r->getInfoString().empty()) {
            out << "# " << r->getInfoString() << "\n";
        }

        if (r->hasHeader()) {
            auto & header = r->getHeader();
            header.attachToStream(out);
            header.writeHeaderToStreams();
        }

        const bool preceded_by_value = false;
        writeSubReportPartialHeader_(out, r, "", preceded_by_value);

        out << "\n";
    }

    /*!
     * \brief Write a subreport header on the current row
     * \param[in] out Ostream to which output will be written
     * \param[in] r Report to print to \a out (recursively)
     * \param[in] prefix Prefix to prepend to any names written
     * \param[in] preceded_by_value Is this call preceded by a value on the same
     * line in \a out, thus requiring a leading comma?
     * \return Returns true if a value was written, false if not.
     */
    bool writeSubReportPartialHeader_(std::ostream& out,
                                      const Report* r,
                                      const std::string& prefix,
                                      bool preceded_by_value) const {
        bool wrote_value = false; // Did this function write a value (this is the result of this function)

        auto itr = r->getStatistics().begin();
        if (itr != r->getStatistics().end()) {
            while(1){
                if(itr == r->getStatistics().begin() && preceded_by_value){
                    out << ","; // Insert comma following last data (which has no trailing comma) before the first value here.
                }

                const statistics::stat_pair_t& si = *itr;
                if(si.first != ""){
                    // Print name = value
                    out << prefix + si.first;
                }else{
                    // Print location = value
                    out << prefix + si.second.getLocation();
                }

                wrote_value = true;

                itr++;
                if(itr == r->getStatistics().end()){
                    break;
                }
                out << ",";
            }
        }
        else {
            // The previous subreport didn't have stats, but that
            // doesn't mean subsequent reports won't.  They need to
            // know that a previous, previous report wrote a value
            wrote_value = preceded_by_value;
        }

        for(const Report& sr : r->getSubreports()){
            const bool sr_wrote_value = writeSubReportPartialHeader_(out, &sr, sr.getName() + ".", wrote_value);

            wrote_value |= sr_wrote_value;
        }

        return wrote_value;
    }

    /*!
     * \brief Write a single row of data
     */
    void writeRow_(std::ostream& out,
                   const Report* r) const {
        const bool preceded_by_value = false;
        writeSubReportPartialRow_(out, r, preceded_by_value);
        out << "\n";
    }

    /*!
     * \brief Writes out a special 'Skipped' message to the CSV file (exact message
     * will depend on how the SkippedAnnotator subclass wants to annotate this gap
     * in the report)
     */
    void skipRows_(std::ostream& out,
                   const sparta::trigger::SkippedAnnotatorBase * annotator,
                   const Report* r) const;

    void getTotalNumStatsForReport_(const Report* r, uint32_t & total_num_stats) const {
        total_num_stats += r->getStatistics().size();
        for (const Report & sr : r->getSubreports()) {
            getTotalNumStatsForReport_(&sr, total_num_stats);
        }
    };

    /*!
     * \brief Write a subreport on the current row
     * \param[in] out Ostream to which output will be written
     * \param[in] r Report to print to \a out (recursively)
     * \param[in] preceded_by_value Is this call preceded by a value on the same
     * line in \a out, thus requiring a leading comma?
     * \return Returns true if a value was written, false if not.
     */
    bool writeSubReportPartialRow_(std::ostream& out,
                                   const Report* r,
                                   bool preceded_by_value) const {
        bool wrote_value = false; // Did this function write a value (this is the result of this function)

        auto itr = r->getStatistics().begin();
        if (itr != r->getStatistics().end()) {
            while(1){
                if(itr == r->getStatistics().begin() && preceded_by_value){
                    out << ","; // Insert comma following last data (which has no trailing comma) before the first value here.
                }

                // Print the value
                const statistics::stat_pair_t& si = *itr;
                out << Report::formatNumber(si.second.getValue());

                wrote_value = true; // 1 or more values written here

                itr++;
                if(itr == r->getStatistics().end()){
                    break;
                }
                out << ",";
            }
        }
        else {
            // The previous subreport didn't have stats, but that
            // doesn't mean subsequent reports won't.  They need to
            // know that a previous, previous report wrote a value
            wrote_value = preceded_by_value;
        }

        for (const Report& sr : r->getSubreports()){
            const bool sr_wrote_value = writeSubReportPartialRow_(out, &sr, wrote_value);

            wrote_value |= sr_wrote_value;
        }

        return wrote_value;
    }

    /*!
     * \brief Combine metadata key-value map into a single
     * comma-separated string.
     */
    std::string stringizeRunMetadata_() const
    {
        if (metadata_kv_pairs_.empty()) {
            return "";
        }

        std::ostringstream oss;
        for (const auto & md : metadata_kv_pairs_) {
            oss << md.first << "=" << md.second << ",";
        }

        std::string stringized = oss.str();
        stringized.pop_back();
        return stringized;
    }
};

//! \brief CSV stream operator
inline std::ostream& operator<< (std::ostream& out, CSV & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta
