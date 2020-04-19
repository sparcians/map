// <CategoryManager> -*- C++ -*-

#pragma once

#include <string>

#include "sparta/utils/StringManager.hpp"

namespace sparta
{
    namespace log
    {
        class categories
        {
        public:

            static constexpr char WARN_STR[] = "warning";
            static constexpr char DEBUG_STR[] = "debug";
            static constexpr char PARAMETERS_STR[] = "parameters";

            // Builtin Categories
            static const std::string* const WARN;  //!< Indicates a WARNING
            static const std::string* const DEBUG; //!< Indicates a DEBUG
            static const std::string* const PARAMETERS; //!< Indicates PARAMETER setup logs
            static const std::string* const NONE;  //!< Indicates NO category (or for observing, ANY category)

        }; // class categories
    } // namespace log
} // namespace sparta

