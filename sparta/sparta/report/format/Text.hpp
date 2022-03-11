// <Test> -*- C++ -*-

/*!
 * \file Text.hpp
 * \brief Plaintext Report output formatter
 */

#pragma once

#include <iostream>
#include <sstream>
#include <math.h>

#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    namespace report
    {
        namespace format
        {

/*!
 * \brief Report formatter for plaintext output
 * \note Non-Copyable
 */
class Text : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Default prefix before each report entry
     */
    static const char DEFAULT_REPORT_PREFIX[];

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output ostream to write to when write() is called
     */
    Text(const Report* r, std::ostream& output) :
        BaseOstreamFormatter(r, output),
        show_descs_(false),
        show_sim_info_(true),
        val_col_(0),
        report_prefix_(DEFAULT_REPORT_PREFIX),
        quote_report_names_(true),
        show_report_range_(true),
        indent_subreports_(true),
        write_contentless_reports_(true)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param filename File which will be opened and appended to when write() is
     * called
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     */
    Text(const Report* r,
         const std::string& filename,
         std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode),
        show_descs_(false),
        show_sim_info_(true),
        val_col_(0),
        report_prefix_(DEFAULT_REPORT_PREFIX),
        quote_report_names_(true),
        show_report_range_(true),
        indent_subreports_(true),
        write_contentless_reports_(true)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    Text(const Report* r) :
        BaseOstreamFormatter(r),
        show_descs_(false),
        show_sim_info_(true),
        val_col_(0),
        report_prefix_(DEFAULT_REPORT_PREFIX),
        quote_report_names_(true),
        show_report_range_(true),
        indent_subreports_(true),
        write_contentless_reports_(true)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~Text()
    {
    }

    /*!
     * \brief Enable writing of reports/subreports with no statistics
     */
    void setWriteContentlessReports(bool enable) { write_contentless_reports_ = enable; }

    /*!
     * \brief Will reports having no statistics be written
     */
    bool getWriteContentlessReports() const { return write_contentless_reports_; }

    /*!
     * \brief Enable indentation of subreports
     */
    void setIndentSubreports(bool enable) { indent_subreports_ = enable; }

    /*!
     * \brief Will subreports be indented
     */
    bool getIndentSubreports() const { return indent_subreports_; }

    /*!
     * \brief Enable showing report time ranges
     */
    void setShowReportRange(bool enable) { show_report_range_ = enable; }

    /*!
     * \brief Will time ranges be shown for reports
     */
    bool getShowReportRange() const { return show_report_range_; }

    /*!
     * \brief Enable printing report names in quotes
     */
    void setQuoteReportNames(bool enable) { quote_report_names_ = enable; }

    /*!
     * \brief Will report names be written in quotes
     */
    bool getQuoteReportNames() const { return quote_report_names_; }

    /*!
     * \brief Sets the text printed before each report or subreport in the
     * output. Defaults to DEFAULT_REPORT_PREFIX.
     */
    void setReportPrefix(const std::string& prefix) { report_prefix_ = prefix; }

    /*!
     * \brief Returns the current report prefix
     * \see setReportPrefix
     */
    const std::string& getReportPrefix() const { return report_prefix_; }

    /*!
     * \brief Sets value column alignment in output
     * \param value_column Column to write stream to. A value > 0 attempts to
     * left-align all equal signs (preceding values) in the entire report
     * (and subreport). Names plus indentation which are longer than
     * value_column will cause that value to be right of the desired value_column
     */
    void setValueColumn(uint32_t col) { val_col_ = col; }

    /*!
     * \brief Returns the current value column
     */
    uint32_t getValueColumn() const { return val_col_; }

    /*!
     * \brief Set whether the formatter will show simulation info at the top of
     * the output
     */
    void setShowSimInfo(bool show) { show_sim_info_ = show; }

    /*!
     * \brief Returns whether the formatter will show simulation info at the top
     * of the output
     */
    bool getShowSimInfo() const { return show_sim_info_; }

    /*!
     * \brief Set whether this formatter will write descriptions for stats
     */
    void setShowDescriptions(bool show) { show_descs_ = show; }

    /*!
     * \brief Will this formatter write descriptions for stats
     */
    bool getShowDescriptions() const { return show_descs_; }

    /*!
     * \brief Returns the rightmost column of any field in the report including
     * indentation. This result can be used in setValueColumn to right-align
     * everything without any variation in the value column position
     */
    uint32_t getRightmostNameColumn() const {
        return getRightmostNameColumn_(report_, 0);
    }

protected:

    /*!
     * \brief Recursively implements getRightmostNameColumn
     */
    uint32_t getRightmostNameColumn_(const Report* r, uint32_t depth=0) const {
        sparta_assert(r != nullptr, "null report");
        uint32_t rcol = 0;

        const std::string INDENT_STR = "  "; // Amount to indent each level
        // Additional indent for stats at given indentation level (separate them from subreports)
        const std::string ADDITIONAL_STAT_INDENT = "  ";

        uint32_t indent = 0;
        if(indent_subreports_){
            for(uint32_t d=0; d<depth; ++d){
                indent += INDENT_STR.size();;
            }
        }

        indent += INDENT_STR.size() + ADDITIONAL_STAT_INDENT.size();

        for(const Report::stat_pair_t& si : r->getStatistics()){
            if(si.first != ""){
                // Stat name
                rcol = std::max<uint32_t>(rcol, indent + si.first.size());
            }else{
                // Print location as stat name
                rcol = std::max<uint32_t>(rcol, indent + si.second.getLocation().size());
            }
        }

        for(const Report& sr : r->getSubreports()){
            rcol = std::max(rcol, getRightmostNameColumn_(&sr, depth+1));
        }

        return rcol;
    }

    //! \name Output
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Writes a header to some output based on the report
     */
    virtual void writeHeaderToStream_(std::ostream& out) const override  {
        (void) out;
    }

    /*!
     * \brief Writes the content of this report to some output
     * \param out Stream to write report to
     */
    virtual void writeContentToStream_(std::ostream& out) const override {
        if(show_sim_info_){
            out << sparta::SimulationInfo::getInstance().stringize("", "\n") << std::endl << std::endl;
        }
        dump_(out, report_, 0);
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Write plaintext for the report to the output
     * \param r Report to porint. Must not be nullptr
     */
    void dump_(std::ostream& out, const Report* r, uint32_t depth=0) const
    {
        sparta_assert(r != nullptr, "null report");

        const bool show_descs =
            (r->getStyle("show_descriptions", getShowDescriptions() ? "true" : "false") == "true");

        const std::string INDENT_STR = "  "; // Amount to indent each level
        // Additional indent for stats at given indentation level (separate them from subreports)
        const std::string ADDITIONAL_STAT_INDENT = "  ";

        std::ios_base::fmtflags old = out.flags(); // Store original format

        if(write_contentless_reports_ || hasStatistics_(r)){

            std::stringstream indent;
            if(indent_subreports_){
                for(uint32_t d=0; d<depth; ++d){
                    indent << INDENT_STR;
                }
            }

            // Report name
            out << indent.str() << report_prefix_;
            if(quote_report_names_){
                out << "\"";
            }
            out << r->getName();
            if(quote_report_names_){
                out << "\"";
            }

            // Report range at top-level of the report
            if(show_report_range_ && depth == 0){
                out << " [" << r->getStart() << ",";
                if(r->getEnd() == Scheduler::INDEFINITE){
                    //Most reports will be associated with a Scheduler, but
                    //reports that were recreated from SimDB database records
                    //may not, as those report objects are typically recreated
                    //outside of a simulation entirely (perhaps from the Python
                    //shell to generate one of these text reports, for example).
                    const Scheduler * sched = getScheduler(false);
                    out << (sched ? sched->getCurrentTick() : r->getEnd());
                }else{
                    out << r->getEnd();
                }
                out << "]";
            }
            out << "\n";

            indent << INDENT_STR << ADDITIONAL_STAT_INDENT;

            uint32_t val_col_after_indent = val_col_;
            if(val_col_after_indent >= indent.str().size()){
                val_col_after_indent -= indent.str().size();
            }
            for(const Report::stat_pair_t& si : r->getStatistics()){
                out << indent.str();
                std::stringstream name;
                if(val_col_ > 0){
                    name << std::left << std::setfill(' ') << std::setw(val_col_after_indent);
                }

                // Generate Stat Name
                if(si.first != ""){
                    // Print name
                    name << si.first;
                }else{
                    // Print location = value
                    name << si.second.getLocation();
                }
                name << " = ";

                // Generate Value
                double val = si.second.getValue();
                name << Report::formatNumber(val);

                // Print description column
                if(show_descs){
                    uint32_t desc_col = val_col_after_indent + desc_col_offset_;
                    if(name.str().size() < desc_col){
                        uint32_t offset = desc_col - name.str().size();
                        name << std::left << std::setfill(' ') << std::setw(offset) << "";
                    }

                    // Print the expression after the value
                    //name << " # " << si.second.getExpressionString();

                    // Print the description
                    const bool include_stat_expression = false;
                    std::string desc = si.second.getDesc(include_stat_expression);
                    uint32_t pos = 0;
                    name << " # " << desc.substr(0,std::min<size_t>(desc.size(), desc_col_width_));
                    pos += desc_col_width_;
                    while(desc.size() > pos){
                        name << '\n' << indent.str();
                        name << std::setfill(' ') << std::setw(desc_col) << "";
                        name << " # " << desc.substr(pos, std::min<size_t>(desc.size()-pos, desc_col_width_));
                        pos += desc_col_width_;
                    }
                }

                // Write line to the output
                out << name.str() << std::endl;
            }

            // Print newline if any stats were printed
            if(r->getStatistics().size() > 0){
                out << std::endl;
            }

            for(const Report& sr : r->getSubreports()){
                dump_(out, &sr, depth+1);
            }

            // Print addtional newline after the subreports if any stats were
            // printed at this level
            if(r->getSubreports().size() > 0){
                out << std::endl;
            }
        }

        out.flush(); // For easier debugging of issues

        out.setf(old); // Restore original format
    }

    /*!
     * \brief Recursively deterines if this report or any of its subreports have
     * 1 or more statistics at any depth. This can be used to cull subreports
     * from being printed if they have no useful content
     * \param r root Report to start checking for presence of any statistics
     */
    bool hasStatistics_(const Report* r) const {
        if(r->getStatistics().size() > 0){
            return true;
        }

        for(auto& sr : r->getSubreports()){
            if(hasStatistics_(&sr)){
                return true;
            }
        }

        return false;
    }

private:

    /*!
     * \brief Offset from value column to description column (if printed)
     */
    uint32_t desc_col_offset_ = 10;

    /*!
     * \brief Width of the description column if used
     */
    uint32_t desc_col_width_ = 60;

    /*!
     * \brief Show descriptions
     */
    bool show_descs_;

    /*!
     * \brief Show the simulation info at the start of the report
     */
    bool show_sim_info_;

    /*!
     * \brief Current value alignment column
     */
    uint32_t val_col_;

    /*!
     * \brief Prefix printed before each report entry
     */
    std::string report_prefix_;

    /*!
     * \brief Should report names be in quotes
     */
    bool quote_report_names_;

    /*!
     * \brief Should the time range of each report be shown
     */
    bool show_report_range_;

    /*!
     * \brief Should subreports be indented
     */
    bool indent_subreports_;

    /*!
     * \brief Should reports having no content be written
     */
    bool write_contentless_reports_;
};

//! \brief Text stream operator
inline std::ostream& operator<< (std::ostream& out, Text & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta
