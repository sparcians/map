// <Gnuplot> -*- C++ -*-

/*!
 * \file Gnuplot.hpp
 * \brief Gnuplot Report output formatter
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
 * \brief Report formatter for Gnuplot output
 * \note Non-Copyable
 */
class Gnuplot : public BaseOstreamFormatter
{
public:

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    Gnuplot(const Report* r, std::ostream& output) :
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
    Gnuplot(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    Gnuplot(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~Gnuplot()
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
        writeGPLTHeader_(out, report_, 1);
    }

    /*!
     * \brief Writes the content of this report to some output
     */
    virtual void writeContentToStream_(std::ostream& out) const override
    {
        writeData_(out, report_);
        out << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Write header to the output
     */
    int writeGPLTHeader_(std::ostream& out, const Report* r, int idx) const
    {
        for (const Report::stat_pair_t& si : r->getStatistics()) {
            out << "# (" << idx++ << ")";
            if (si.first != "") {
               out << " " << si.first;
            }
            out << " (" << si.second.getLocation() << ")" << std::endl;
        }

        for (const Report& sr : r->getSubreports()) {
            idx = writeGPLTHeader_(out, &sr, idx);
        }

        return idx;
    }

    /*!
     * \brief Write the values to the output
     */
    void writeData_(std::ostream& out, const Report* r) const
    {
        for (const Report::stat_pair_t& si : r->getStatistics()) {
            out << si.second.getValue() << " ";
        }

        for (const Report& sr : r->getSubreports()) {
            writeData_(out, &sr);
        }
    }
};

//! \brief Gnuplot stream operator
inline std::ostream& operator<< (std::ostream& out, Gnuplot & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta
