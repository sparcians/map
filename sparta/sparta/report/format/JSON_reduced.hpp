// <JSON_reduced> -*- C++ -*-


/*!
 * \file JSON_reduced.hpp
 * \brief JSON Report output formatter for only stat value information; reduction of JSON.h
 */

#ifndef __SPARTA_REPORT_FORMAT_JSON_reduced_H__
#define __SPARTA_REPORT_FORMAT_JSON_reduced_H__

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
 * \brief Report formatter for JSON output
 * \note Non-Copyable
 */
class JSON_reduced : public BaseOstreamFormatter
{
private:
    // JavaScript JSON format is considered as version 1.0.
    const std::string version_ = "2.1";

public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    JSON_reduced(const Report* r, std::ostream& output) :
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
    JSON_reduced(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    JSON_reduced(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    /*!
     * \brief Return the JSON version used
     */
    std::string getVersion() const{
        return version_;
    }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~JSON_reduced()
    {
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

    /*!
     * \brief Writes the content of this report to some output
     */
    virtual void writeContentToStream_(std::ostream& out) const override;

    mutable std::vector<std::string> report_local_names_;

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Helper function to flatten a hierarchical name
     * \note This removes everything before the last dot
     */
    std::string flattenReportName(std::string full_name) const{
        std::string local_name = full_name;
        std::size_t last_dot_idx = full_name.find_last_of(".");
        if (last_dot_idx != std::string::npos){
            local_name = full_name.substr(last_dot_idx+1);
        }
        return local_name;
    }
};

//! \brief JSON stream operator
inline std::ostream& operator<< (std::ostream& out, JSON_reduced & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta

// __SPARTA_REPORT_FORMAT_JSON_reduced_H__
#endif
