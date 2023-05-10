// <Destination.cpp> -*- C++ -*-


/*!
 * \file Destination.cpp
 * \brief Contains implementation of destination writer functions
 */

#include "sparta/log/Destination.hpp"

#include <fstream>
#include <iomanip>
#include <ostream>

#include "sparta/simulation/Clock.hpp"

namespace sparta {
    namespace log {

DestinationManager::DestinationVector sparta::log::DestinationManager::dests_;
const Formatter::Info FMTLIST[] = {
    /*! Writes source, category, content */
    { ".log.basic",
      "basic formatter. Contains message origin, category, and content",
      [](std::ostream& s) -> sparta::log::Formatter* { return new sparta::log::BasicFormatter(s); } },

    /*! Writes all message info */
    { ".log.verbose",
      "verbose formatter. Contains all message meta-data",
      [](std::ostream& s) -> sparta::log::Formatter* { return new sparta::log::VerboseFormatter(s); } },

    /*! Raw data */
    { ".log.raw",
      "verbose formatter. Contains no message meta-data",
      [](std::ostream& s) -> sparta::log::Formatter* { return new sparta::log::RawFormatter(s); } },

    /*! Writes all content to HTML table */
    /*{ ".log.html",  "html formatting. Contains all message data",
      [](std::ofstream& s) -> sparta::log::Formatter* { return new sparta::log::HTMLFormatter(s); } },*/

    /*! Writes most content excluding thread/sequence info (default because it is last in the list) */
    { nullptr,
      "Moderate information formatting. Contains most message meta-data excluding thread and "
      "message sequence.",
      [](std::ostream& s) -> sparta::log::Formatter* { return new sparta::log::DefaultFormatter(s); } }
};

const Formatter::Info* Formatter::FORMATTERS = FMTLIST;

void DefaultFormatter::write(const sparta::log::Message& msg)
{
    std::ios::fmtflags f = stream_.flags();

    if(msg.print_info)
    {
        stream_ << '{';

        stream_ << std::setfill('0') << std::dec; // Applies to the following numbers

        // sim time
        stream_ << "" << std::setw(10) << std::right << msg.info.sim_time << INFO_DELIMITER;

        // clock time
        const Clock* clk = msg.info.origin.getClock();
        if(clk){
            stream_ << std::setw(8) << std::right << clk->currentCycle() << INFO_DELIMITER;
        }else{
            stream_ << "--------" << INFO_DELIMITER;
        }

        // origin
        stream_ << msg.info.origin.getLocation() << INFO_DELIMITER;

        // category
        stream_ << *msg.info.category << "} ";
    }

    stream_ << copyWithReplace(msg.content, '\n', "") << std::endl;

    // restore ostream flags
    stream_.flags(f);

    stream_.flush();
}

    } // namespace log
} // namespace sparta
