// <Message> -*- C++ -*-

#pragma once

#include "sparta/log/MessageInfo.hpp"

#include <string>


namespace sparta
{
    namespace log
    {
        /*!
         * \brief Contains a logging message header and content
         */
        struct Message
        {
            MessageInfo info;
            const std::string& content;
        };

    } // namespace log
} // namespace sparta

