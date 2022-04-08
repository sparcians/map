// <PythonDict> -*- C++ -*-

/*!
 * \file PythonDict.hpp
 * \brief PythonDict Report output formatter
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
 * \brief Report formatter for PythonDict output
 * \note Non-Copyable
 */
class PythonDict : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    PythonDict(const Report* r, std::ostream& output) :
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
    PythonDict(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    PythonDict(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~PythonDict()
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
    virtual void writeContentToStream_(std::ostream& out) const override
    {
        out << "report = {" ;
        writeDictContents_(out, report_);
        out << "}" ;
        out << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Write Python Dictionary
     */
    void writeDictContents_(std::ostream& out, const Report* r) const
    {
        std::string leaf_name = r->getName();
        const auto pos = leaf_name.find_last_of(".");
        if(pos != std::string::npos) {
            leaf_name = r->getName().substr(pos+1);
        }
        out << "\"" << leaf_name << "\"" << ": {" ;

        int elements_=0;
        for (const statistics::stat_pair_t& si : r->getStatistics()) {
            if(si.first != ""){
                if(elements_ > 0){
                    out << ", ";
                }
                out << "\"" << si.first << "\": " ;
                double val = si.second.getValue();
                if(isnan(val)){
                    out << "float('nan')";
                }else if(isinf(val)){
                    out << "float('inf')";
                }else{
                    out << Report::formatNumber(val);
                }
                ++elements_;
            }
        }

        for (const Report& sr : r->getSubreports()) {
            if(elements_ > 0){
                out << ", ";
            }
            writeDictContents_(out, &sr);
            ++elements_;
        }

        out << "}" ;
    }

};

//! \brief PythonDict stream operator
inline std::ostream& operator<< (std::ostream& out, PythonDict & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta
