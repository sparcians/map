// <JSON> -*- C++ -*-

/*!
 * \file JSON.hpp
 * \brief JSON Report output formatter
 */

#ifndef __SPARTA_REPORT_FORMAT_JSON_H__
#define __SPARTA_REPORT_FORMAT_JSON_H__

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
class JSON : public BaseOstreamFormatter
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
    JSON(const Report* r, std::ostream& output) :
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
    JSON(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    JSON(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    std::string getVersion() const{
        return version_;
    }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~JSON()
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

    /*!
     * \brief Member variables to keep JSON string references alive
     * until rapidjson stringizes the stats contents
     */
    mutable std::vector<std::string> report_local_names_;
    mutable std::vector<std::vector<std::string>> ordered_keys_;
    mutable std::vector<std::string> statistics_descs_;

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
inline std::ostream& operator<< (std::ostream& out, JSON & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta

// __SPARTA_REPORT_FORMAT_JSON_H__
#endif
