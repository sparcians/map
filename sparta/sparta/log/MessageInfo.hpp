// <MessageInfo> -*- C++ -*-

#ifndef __MESSAGE_INFO_H__
#define __MESSAGE_INFO_H__

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <ostream>
#include <string>
#include <thread>
#include <ctime>

#include "sparta/simulation/TreeNode.hpp"

namespace sparta
{
class TreeNode;

    namespace log
    {
        // Placeholders for types expected to be defined elsewhere
        typedef uint32_t thread_id_type; //!< Identifies a thread in the simulator kernel
        typedef uint64_t sim_time_type;  //!< Simulator timestamp type

        typedef int64_t seq_num_type;   //!< Sequence number of a message within a thread ID. Signed so that initial state can be -1.

        /*!
         * \brief Logging Message information excluding actual message content
         */
        struct MessageInfo
        {
            //! \brief Local typedef for category ID
            typedef const std::string* category_id_type;

            //! \brief Timing type for wall-clock time
            typedef double wall_time_type;


            TreeNode const & origin;   //!< Node from which message originated
            wall_time_type wall_time;  //!< Timestamp in wall-clock time (not guaranteed to be monotonically increasing)3A
            sim_time_type sim_time;    //!< Simulator timestamp
            category_id_type category; //!< Category with which this message was created. Must not be NULL
            thread_id_type thread_id;  //!< Thread ID of source
            seq_num_type seq_num;      //!< Sequence number of message within thread
        };


        static constexpr const char* INFO_DELIMITER = " ";

        /*!
         * \brief ostream insertion operator for serializing MessageInfo.
         *
         * The result of this operation ends up directly in log files or on the screen
         */
        std::ostream& operator<<(std::ostream& o, const MessageInfo& info);

    } // namespace log
} // namespace sparta

// __MESSAGE_INFO_H__
#endif
