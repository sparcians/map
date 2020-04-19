// <ConfigParser> -*- C++ -*-

#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


// Special-case config file nodes

#define INCLUDE_KEYS {"#include", "include"}
#define COMMENT_KEY_START "//" // In addition to normal yaml '#' comments

namespace sparta
{
    /*!
     * \brief Namespace containing all config-file parsers (readers)
     */
    namespace ConfigParser
    {

        /*!
         * \brief Base class for configuration parser objects
         */
        class ConfigParser
        {
        public:

            /*!
             * \brief Keyword parameter value indicating the parameter should be
             * flagged as not required instead of assigning a new value
             */
            static constexpr char OPTIONAL_PARAMETER_KEYWORD[] = "<OPTIONAL>";

            /*!
             * \brief Constructor
             * \param filename Config file to consume
             */
            ConfigParser(const std::string& filename)
            {
                (void) filename;
            }
        };
    }
} // namespace sparta

