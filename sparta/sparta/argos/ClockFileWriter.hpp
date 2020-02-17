// <ClockFileWriter.hpp>  -*- C++ -*-

/**
 * \file ClockfileWriter.hpp
 *
 * \brief Contains Clockfilewriter class
 */

#ifndef __CLOCK_FILE_WRITER_H__
#define __CLOCK_FILE_WRITER_H__

#include <iostream>
#include <fstream>
#include <sstream>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta{
namespace argos
{
    /**
     * \brief Object capable of writing file with clock info entries for a given
     * device tree for consumption by the argos viewer
     *
     * File format is a version line:
     * \verbatim
     * <version>
     * \endverbatim
     * (where version is ClockFileWriter::VERSION)
     *
     * followed by 1 line showing hypercycle (tick) frequency in Hz:
     * \verbatim
     * <hypercycle_tick_freq_hz>
     * \endverbatim
     *
     * Followed by any number of single-line entries; 1 per each clock in the
     * the simulation (in no particular order) each having the form:
     * \verbatim
     * <clock_uid_int>,<clock_name>,<period_in_hc_ticks>,<clock_ratio_numerator>,<clock_ration_denominator>\n
     * \endverbatim
     * Note that the newline (\n) will be present on every line
     *
     * Lines beginning with '#' as the first character are comments
     */
    class ClockFileWriter
    {
    public:

        /*!
         * \brief Default Constructor
         * \param prefix Prefix of file to which data will be written.
         */
        ClockFileWriter(const std::string& prefix,
                        const std::string& fn_extension = "clock.dat",
                        const uint32_t fmt_version = 1) :
            file_(prefix + fn_extension, std::ios::out)
        {
            if(!file_.is_open()){
                throw sparta::SpartaException("Failed to open clock file \"")
                    << prefix + fn_extension << "\" for write";
            }

            // Throw on write failure
            file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);

            file_ << fmt_version << " # Version Number" << std::endl;
        }


        /*!
         * \brief Writes content of an entire clock tree with the given root to
         * this clocks file.
         * \param root Root node of clock tree to write to the file
         * \warning There is no need to write each node individually.
         * \warnings Any nodes inserted with this operator more than once will
         * cause repeat entries
         */
        ClockFileWriter& operator<<(const sparta::Clock& clk) {

            //! \todo Need to extract a frequency (in Hz) from clk
            file_ << 1 << " # Tick frequency" << std::endl;
            recursWriteClock_(&clk);
            file_.flush();
            return *this;
        }

    private:

        /*!
         * \brief Recursively writes an entry for the given clock and all
         *        children in pre-order.
         * \param node Node for which entriy should be written. Children will
         *             then be iterated.
         * \post May write any number of lines to the output file
         *       depending on number of nodes and each node's name,
         *       group info, and aliases.
         */
        void recursWriteClock_(const sparta::Clock* clk)
        {
            sparta_assert(clk != nullptr);

            file_ << clk->getNodeUID() << ','
                  << clk->getName()    << ','
                  << clk->getPeriod()  << ','
                  << clk->getRatio().getNumerator() << ','
                  << clk->getRatio().getDenominator() << std::endl;

            // Recurse into children
            for(const sparta::TreeNode* child : sparta::TreeNodePrivateAttorney::getAllChildren(clk)) {
                const sparta::Clock* child_clk = dynamic_cast<const sparta::Clock*>(child);
                if(child_clk){
                    recursWriteClock_(child_clk);
                }
            }
        }

        /*!
         * \brief Filename with which this file was actually opened (includes
         * prefix given at construction)
         */
        std::string filename_;

        /*!
         * \brief File to which clock data will be written.
         */
        std::ofstream file_;

    }; // class ClockFileWriter

}//namespace argos
}//namespace sparta

// __CLOCK_FILE_WRITER_H__
#endif
