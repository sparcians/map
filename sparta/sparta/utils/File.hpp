// <File> -*- C++ -*-


/**
 * \file   File.hpp
 *
 * \brief  Utilities for file I/O
 */

#ifndef __SPARTA_UTILS_FILE_H__
#define __SPARTA_UTILS_FILE_H__

#include <cstdint>
#include <vector>
#include <string>

namespace sparta
{
    namespace utils
    {

        static constexpr char COUT_FILENAME[] = "1";
        static constexpr char CERR_FILENAME[] = "2";


        //! \todo Create a file manager


        /*!
         * \brief Computes a filename based on the input name replacing it with one of
         * several variables
         * \note Supported variables in report names include:
         * \li \%l Location in device tree of report instantiation
         * \li \%i Index of report instantiation
         * \li \%p Host process ID
         * \li \%t Timestamp/Date
         * \li \%s Simulator name
         */
        std::string computeOutputFilename(const std::string& name,
                                          const std::string& location,
                                          uint32_t idx,
                                          const std::string& sim_name);

        /*!
         * \brief Look up the location of an architecture config file fiven a searchpp
         * dir and a name refering to a config file OR directory within that dir.
         * \throw SpartaException if input cannot resolve to yaml file.
         * \param[in] search_dir Search directory
         * \param[in] name Name of config file (with or without prefix) or
         * architecture dir name within arch search dir. Subclasses can add more
         * advanced resolution of arch names to config files
         * \return Absolute path to architecture configuration file (x.yaml)
         */
        std::string findArchitectureConfigFile(const std::vector<std::string>& search_dirs,
                                               const std::string& name);
        static constexpr char ARCH_OPTIONS_RESOLUTION_RULES[] =                                 \
            "<arch> may be specified as a '.yaml'/'.yml' file in <arch-search-dir>. The yaml suffix is "
            "not required and will be appended automatically if matchines files exists. If a directory "
            "with a name matching <arch> exists in <arch-search-dir>, then the search continues into "
            "that directory for a file named <arch>.yaml (or .yml). If no such file is found or there "
            "was no directory name matching <arch> then, architecture config resolution fails.";

    } // namespace utils
} // namespace sparta

// __SPARTA_UTILS_FILE_H__

#endif
