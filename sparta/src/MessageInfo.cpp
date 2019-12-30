// <MessageInfo> -*- C++ -*-


/*!
 * \file MessageInfo.cpp
 * \brief Prints Log message information
 */

#include "sparta/log/MessageInfo.hpp"

#include <iomanip>
#include <ostream>

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"

namespace sparta {
    namespace log {

std::ostream& operator<<(std::ostream& o, const MessageInfo& info) {
    std::ios::fmtflags f = o.flags();

    double t = double(info.wall_time);

    o << '{';

    o << std::setfill('0') << std::dec; // Applies to the following numbers

    // sim time
    o << std::setw(8) << std::right << info.sim_time << INFO_DELIMITER;

    // clock time
    const Clock* clk = info.origin.getClock();
    if(clk){
        o << std::setw(8) << std::right << clk->currentCycle() << INFO_DELIMITER;
    }else{
        o << "--------" << INFO_DELIMITER;
    }

    // wall time
    o << std::setfill('0') << std::setw(10) << std::right << std::fixed
      << std::setprecision(4) << t << INFO_DELIMITER;
    o.flags(f); // drop precision and fixed specifiers

    o << std::setfill('0') << std::right << std::dec; // Applies to the following numbers

    o << std::hex;

    // thread id
    o << "0x" << std::setw(2) << info.thread_id << INFO_DELIMITER;

    // sequence id
    o << "0x" << std::setw(8) << info.seq_num << INFO_DELIMITER;

    // origin
    o << info.origin.getLocation() << INFO_DELIMITER;

    // category
    o << *info.category << "} ";

    // restore ostream flags
    o.flags(f);

    return o;
}

    } // namespace log
} // namespace sparta
