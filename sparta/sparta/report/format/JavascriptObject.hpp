// <JavascriptObject> -*- C++ -*-

/*!
 * \file JavascriptObject.hpp
 * \brief JavascriptObject Report output formatter
 */

#ifndef __SPARTA_REPORT_FORMAT_JAVASCRIPTOBJECT_H__
#define __SPARTA_REPORT_FORMAT_JAVASCRIPTOBJECT_H__

#include <math.h>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "boost/algorithm/string.hpp"
#include "sparta/report/Report.hpp"

namespace sparta
{
    namespace report
    {
        namespace format
        {

/*!
 * \brief Report formatter for JavascriptObject output
 * \note Non-Copyable
 */
class JavascriptObject : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    JavascriptObject(const Report* r, std::ostream& output) :
        BaseOstreamFormatter(r, output)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param filename File which will be opened and appended to when write() is
     * called
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     */
    JavascriptObject(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    JavascriptObject(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~JavascriptObject()
    {
    }

    // Let the report object know which nodes should be leaf-nodes when
    // generating a report.  E.g., 'top.l2cache' should be a leaf node and
    // all stats under it in that node.
    //
    // NOTE: This method isn't the "right" way to do this, but it lets
    // folks prototype what reports would look like quickly without adding
    // simulator-specific info to SPARTA.
    static void addLeafNode(const std::string & node_name) {
        leaf_nodes_.insert(node_name);
    }

    // Let the report object know which nodes should be leaf-nodes when
    // generating a report.  E.g., all nodes under 'top.core1' should be
    // leaf nodes.
    //
    // NOTE: This method isn't the "right" way to do this, but it lets
    // folks prototype what reports would look like quickly without adding
    // simulator-specific info to SPARTA.
    static void addParentOfLeafNodes(const std::string & node_name) {
        parents_of_leaf_nodes_.insert(node_name + ".");
    }


protected:

    //! \name Output
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Writes a header to some output based on the report
     */
    virtual void writeHeaderToStream_(std::ostream& out) const override  {
        (void) out;
    }

    // Main method called when a report is written
    void writeContentToStream_(std::ostream& out) const override;

    // Write a single report
    // @param report:  The report to write
    // @param all_unit_names: RETURNED: list of all unit names found in
    //   the report
    void writeReport_(std::ostream& out, const Report & report,
                      std::vector<std::string> & all_unit_names) const;

    // Write a list of reports by repeatedly calling the writeReport_() method
    void writeReportList_(std::ostream& out, const std::list<Report> & reports,
                          std::vector<std::string> & all_unit_names) const;

    // Merge the stats from the given report into the existing report being
    // generated
    // @param report The report to merge stats from
    // @param merge_top_name The name of the top-level node that we're
    //    merging stats with
    // @param all_stat_names RETURNED list of all stats added
    void mergeReport_(std::ostream& out, const Report & report,
                      const std::string & merge_top_name,
                      std::vector<std::string> & all_stat_names) const;

    // Merge the stats from a list of reports by repeatedly calling mergeReport_()
    void mergeReportList_(std::ostream& out, const std::list<Report> & reports,
                          const std::string & merge_top_name,
                          std::vector<std::string> & all_stat_names) const;

    // Write the stats from a single report to the out stream
    // @param report The report to write stats for
    // @param stat_prefix Prefix string to use for all stats
    // @param RETURNED: all_stat_names Name of all stats written for this report
    void writeStats_(std::ostream& out, const Report & report,
                     const std::string & stat_prefix, std::vector<std::string> & all_stat_names) const;


    // Return a friendly version of the report name that can be used for
    // all nodes
    std::string getReportName_(const Report & report) const;

    // Return whether a report should be considered a leaf node (and have
    // its children merged into its report) or not
    bool isLeafNode_(const Report & report) const;

private:
    mutable uint32_t decimal_places_ = 2;                //!< Number of decimal places per stat to print
    static std::set<std::string> leaf_nodes_;            //!< Explicit list of leaf nodes
    static std::set<std::string> parents_of_leaf_nodes_; //!< Explicit list of nodes whose children are leaf nodes

};

//! \brief JavascriptObject stream operator
inline std::ostream& operator<< (std::ostream& out, JavascriptObject & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta

// __SPARTA_REPORT_FORMAT_JAVASCRIPTOBJECT_H__
#endif
