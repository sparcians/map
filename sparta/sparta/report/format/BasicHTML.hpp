// <BasicHTML> -*- C++ -*-

/*!
 * \file BasicHTML.hpp
 * \brief Basic HTML Report output formatter
 */

#pragma once

#include <iostream>
#include <sstream>
#include <math.h>

#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/report/Report.hpp"

namespace sparta
{
    namespace report
    {
        namespace format
        {

/*!
 * \brief Report formatter for basic (untemplated) HTML output
 * \note Any templated html output should use TemplatedHTML
 * \note Non-Copyable
 */
class BasicHTML : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    BasicHTML(const Report* r, std::ostream& output) :
        BaseOstreamFormatter(r, output),
        show_sim_info_(true)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param filename File which will be opened and appended to when write() is
     * called
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     */
    BasicHTML(const Report* r,
              const std::string& filename,
              std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode),
        show_sim_info_(true)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    BasicHTML(const Report* r) :
        BaseOstreamFormatter(r),
        show_sim_info_(true)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~BasicHTML()
    {
    }

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
     * \brief Format a number so that it is decimal aligned. If there is no
     * decimal, assumes an implicit decimal at right of value string
     * \param num Number to format
     * \param alignment Column at which decimal point should be placed
     * \param leading_space [out] leading space added to put the decimal in the
     * correct column
     * \param decimal_pos [out] Number of digits before the decimal point (string
     * size if no decimal found)
     * \param decimal_places Number of decimal places to use if the output is a
     * float. If < 0, see Report::formatNumber for interpretation
     */
    static std::string formatDecimalAlignedNum(double num,
                                               size_t alignment,
                                               size_t& leading_space,
                                               size_t& decimal_pos,
                                               int32_t decimal_places) {
        std::string val = Report::formatNumber(num,
                                               false, // no scientific notation
                                               decimal_places);
        decimal_pos = val.find_first_of('.');
        if(decimal_pos == std::string::npos){
            decimal_pos = val.size();
        }
        if(decimal_pos <= alignment){
            leading_space = alignment - decimal_pos;
        }else{
            leading_space = 0;
        }
        for(size_t i = 0; i < leading_space; ++i){
            val = "&nbsp;" + val;
        }
        return val;
    }

protected:

    /*!
     * \brief Show the simulation info at the start of the report
     */
    bool show_sim_info_;

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
     */
    virtual void writeContentToStream_(std::ostream& out) const override  {
        out << "<html>"
            "<head><title>" << report_->getName() << "</title>"
            "<style type='text/css'>\n"
            "table {\n"
            "  font-family:courier new, monospace;\n"
            "}\n"
            "table.report_table {\n"
            "  border:1px solid #808080;\n"
            "}\n"
            "table.subreport_table {\n"
            "  border:1px solid #808080;\n"
            "}\n"
            ".subreport_section {\n"
            "  font-size:80%;\n"
            "  text-align:left;\n"
            "  font-style:italic;\n"
            "  color:#505050;\n"
            "}\n"
            "th.tabletitle {\n"
            "  padding:6px;\n"
            "  text-align:left;\n"
            "  font-family:Helvetica, Verdana, sans-serif;\n"
            "  font-size:120%;\n"
            "  border-bottom:3px solid #404040;\n"
            "  background-color:#fffff0;\n"
            "}\n"
            "th.tablesection {\n"
            "  font-weight:normal;"
            "  text-align:left;\n"
            "  font-family:Helvetica, Verdana, sans-serif;\n"
            "  font-size:80%;\n"
            "  border-bottom:1px solid #404040;\n"
            "  background-color:#d0d0d0;\n"
            "}\n"
            "th {\n"
            "  font-size:95%;\n"
            "  background-color:#d0d0d0;\n"
            "  border-bottom:1px solid #707070;\n"
            "  border-right:1px solid #C0C0C0;\n"
            "  }\n"
            "td {\n"
            "  border-bottom:1px solid #707070;\n"
            "  border-right:1px solid #C0C0C0;\n"
            "}\n"
            "td.name {\n"
            "  font-size:80%;\n"
            "  text-align:left;\n"
            "  padding-right:4px;\n"
            "  width:400px;\n"
            "}\n"
            "td.value {\n"
            "  font-size:80%;\n"
            "  width:180px;\n"
            "  padding-left:8px;\n"
            "  font-weight:bold;\n"
            "}\n"
            "td.expression {\n"
            "  font-size:75%;\n"
            "  color:#505050;\n"
            "  padding-left:8px;\n"
            "}\n"
            "td.info {\n"
            "  text-align:right;\n"
            "  font-style:italic;"
            "  font-size:80%;"
            "  padding-right:20px;\n"
            "}\n"
            "span.info_span {\n"
            "  font-style:italic;\n"
            "  font-size:70%;\n"
            "}\n"
            "span.units_span {\n"
            //"  font-style:italic;\n"
            "  font-size:115%;\n"
            "  color:#808080;\n"
            "}\n"
            "td.infoval {\n"
            "  text-align:left;\n"
            "  font-style:italic;\n"
            "  font-size:90%;\n"
            "}\n"
            "td.subreport_td {\n"
            "  text-align:left;\n"
            "  padding:12px 8px 0px 16px;\n"
            "}\n"
            "</style>\n"
            "<script>\n"
            "function hideNode(name) {\n"
            "    document.getElementById(name).style.display='none';\n"
            "    document.getElementById(name + \"_show\").style.display='inline';\n"
            "    document.getElementById(name + \"_hide\").style.display='none';\n"
            "}\n"
            "function showNode(name) {\n"
            "    document.getElementById(name).style.display='block';\n"
            "    document.getElementById(name + \"_show\").style.display='none';\n"
            "    document.getElementById(name + \"_hide\").style.display='inline';\n"
            "}\n"
            "</script>\n"
            "</head>\n"
            "<body style='font-size:8px;'>";

        if(show_sim_info_){
            out << "<table style='width:100%; border:1px solid black;'><tbody>\n"
                << sparta::SimulationInfo::getInstance().stringize("<tr><td>", "</td></tr>")
                << "</tbody></table>\n"
                << "<br/><br/>\n"
                << std::endl;
        }

        dump_(out, report_, 0);

        out << "</body>"
            << "</html>\n";
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Write html for the report to the output
     */
    void dump_(std::ostream& out, const Report* r, uint32_t depth) const {
        assert(r);

        uint32_t num_stat_columns = strtoul(r->getStyle("num_stat_columns", "1").c_str(), 0, 0);
        bool show_descriptions = (r->getStyle("show_descriptions", "true") == "true");

        uint32_t entire_column_span = (2 + show_descriptions) * num_stat_columns;
        sparta_assert(entire_column_span > 0);

        out << "<table cellpadding=3 cellspacing=0";
        out << " style='width:100%;'";
        if(depth > 0){
            out << " class='subreport_table'";
        }else{
            out << " class='report_table'";
        }
        out << ">";

        std::stringstream tmp;
        tmp << r->getName() << "_" << cur_id_++ << std::dec;
        const std::string content_id = tmp.str();

        out << "<thead><tr>";

        if(!r->getParent() || r->getParent()->getStyle("collapsible_children", "yes") == "yes"){
            out << "<th colspan=" << entire_column_span << " class='tabletitle'>"
                << "<input type=\"button\" "
                   "onclick=\"hideNode('" << content_id << "');\" id=\"" << content_id << "_hide\" "
                   "value=\"-\">"
                << "<input type=\"button\" "
                   "onclick=\"showNode('" << content_id << "');\" id=\"" << content_id << "_show\" "
                   "value=\"+\" "
                   "style='display:none;'>"
                << "&nbsp;" << r->getName()
                << "</th>";
        }else{
            out << "<th colspan=" << entire_column_span << " class='tablesection'>"
                << r->getName()
                << "</th>";
        }

        out << "</tr></thead>\n<tbody id=\"" << content_id << "\">";

        if(r->getStatistics().size() > 0){

            const uint32_t decimal_places = strtoul(r->getStyle("decimal_places", "6").c_str(), nullptr, 0);

            // Iterate stats to find out the number of characters needed for the
            // whole number portion of any output for the purpose of decimal
            // alignment
            size_t val_decimal_alignment = 0;
            for(const statistics::stat_pair_t& si : r->getStatistics()){
                size_t leading_space = 0;
                size_t decimal_pos = 0;
                formatDecimalAlignedNum(si.second.getValue(),
                                        40,
                                        leading_space,
                                        decimal_pos,
                                        decimal_places);
                val_decimal_alignment = std::max(val_decimal_alignment, decimal_pos);
            }

            const std::vector<statistics::stat_pair_t> & stats = r->getStatistics();
            uint32_t num_rows = stats.size() / num_stat_columns;
            if ((stats.size() % num_stat_columns) != 0) {
                num_rows++;
            }

            for (uint32_t row_idx = 0; row_idx < num_rows; row_idx++) {

                out << "<tr>\n";

                for (uint32_t stat_col_idx = 0; stat_col_idx < num_stat_columns; stat_col_idx++) {
                    uint32_t stat_idx =
                        (row_idx) + (stat_col_idx * num_rows);

                    if (stat_idx >= stats.size()) {
                        uint32_t stat_column_span = 2 + show_descriptions;
                        out << "<td colspan=" << stat_column_span << "> &nbsp; </td>";
                        continue;
                    }

                    const statistics::stat_pair_t & si = stats[stat_idx];

                    // Compute expression with < and > characters escaped for HTML
                    // Be sure to omit range and fully resolve all sub-expressions
                    std::string expr = si.second.getExpressionString(false, true);
                    replaceSubstring(expr, "<", "&lt;");
                    replaceSubstring(expr, ">", "&gt;");

                    std::string tool_tip_text = si.second.getDesc(false) + "\n" + expr;
                    out << "\n<td class='name' ";
                    out << "title='" << tool_tip_text << "'";
                    out << ">";

                    if(si.first != ""){
                        out << si.first;
                    }else{
                        std::string loc = si.second.getLocation();
                        replaceSubstring(loc, "<", "&lt;");
                        replaceSubstring(loc, ">", "&gt;");
                        out << loc;
                    }

                    size_t leading_space = 0;
                    size_t decimal_pos = 0;
                    std::string val = formatDecimalAlignedNum(si.second.getValue(),
                                                              val_decimal_alignment,
                                                              leading_space,
                                                              decimal_pos,
                                                              decimal_places);

                    out << "</td>\n";

                    // Determine any additional styles for this cell
                    std::string additional_td_style;

                    // If value semantic is percentage and value is greater than 100% + epsilon,
                    // show that the data is bad
                    const auto vs = si.second.getValueSemantic();
                    if(vs == StatisticDef::VS_PERCENTAGE && si.second.getValue() > 100.01){
                        additional_td_style = "background-color:#df0000;color:#ffffff;font-weight:bold;";
                    }

                    // Generate this cell
                    out << "<td class='value' "
                        << "title='" << tool_tip_text << "' "
                        << "style='" << additional_td_style << "' "
                        << ">";
                    if(val.find("nan") != std::string::npos){
                        out << "<span style='font-weight:bold; color:red;'>" << val << "</span>";
                    }else{
                        out << val;
                    }
                    if(vs == StatisticDef::VS_PERCENTAGE){
                        out << "<span class='units_span'>%</span>";
                    }
                    out << "</td>\n";

                    if (show_descriptions) {
                        out << "<td class='expression'>"
                            << si.second.getDesc(false); // do not show stat expressions
                        out << "</td>\n";
                    }
                }
            }
            out << "</tr>\n";
        }

        if(r->getSubreports().size() > 0){
            out << "<tr><td colspan=" << entire_column_span << " class='subreport_td'>\n";

            for(const Report& sr : r->getSubreports()){
                dump_(out, &sr, depth+1);
            }
            out << "<br/>";
            out << "</td></tr>\n";

        }
        out << "</tbody>\n</table>";
    }

private:

    /*!
     * \brief An increasing ID appended to each html anchor created to guarantee
     * unique anchor IDs as well as consistency for testing.
     */
    mutable uint64_t cur_id_ = 0;

};

        } // namespace format
    } // namespace report
} // namespace sparta
