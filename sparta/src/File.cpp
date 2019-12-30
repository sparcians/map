// <File> -*- C++ -*-


#include "sparta/utils/File.hpp"

#ifndef __APPLE__
#include <syscall.h>
#endif
#include <unistd.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <ostream>

#include "sparta/utils/Utils.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/TimeManager.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace bfs = boost::filesystem;

namespace sparta
{
    namespace utils
    {

        /*!
         * \brief Helper for compuring an output filename based on its wildcards and
         * some inputs
         */
        std::string computeOutputFilename(const std::string& name,
                                          const std::string& location,
                                          uint32_t idx,
                                          const std::string& sim_name)
        {
            std::string result = name;

            // Grab the OS PID of this process
            pid_t tid;
#ifdef __APPLE__
            tid = getpid();
#else
            // syscall be deprecated in Mac OS - ARRRRR!
            tid = syscall(SYS_gettid);
#endif
            std::stringstream pid_str;
            pid_str << tid;

            // Convert index to a string
            std::stringstream idx_str;
            idx_str << idx;

            // Get timestamp now
            std::string local_time = TimeManager::getTimeManager().getTimestamp();

            replaceSubstring(result, "%l", location);
            replaceSubstring(result, "%i", idx_str.str());
            replaceSubstring(result, "%p", pid_str.str());
            replaceSubstring(result, "%t", local_time);
            replaceSubstring(result, "%s", sim_name);

            return result;
        }

        std::string findArchitectureConfigFile(const std::vector<std::string>& search_dirs,
                                               const std::string& name)
        {
            sparta_assert(name.size() > 0,
                        "Cannot attempt to find an architecture configuration file with an empty-string name");
            sparta_assert(search_dirs.size() > 0,
                        "Arch search dirs should be atleast 1 entry");
            for (const auto& search_dir : search_dirs)
            {
                // Form starting canonical path
                bfs::path p(search_dir);
                if (bfs::exists(p))
                {
                    p = bfs::canonical(p);
                    p /= name;

                    // Check for suffixed veriations first.

                    auto rf = name.rfind(".yaml");
                    if(rf != std::string::npos || rf != name.size()-5){ // Not at end
                        bfs::path p_yaml = p.string() + ".yaml";
                        if(bfs::exists(p_yaml)){
                            return p_yaml.string();
                        }
                    }

                    rf = name.rfind(".yml");
                    if(rf == std::string::npos || rf != name.size()-4){ // Not at end
                        bfs::path p_yml = p.string() + ".yml";
                        if(bfs::exists(p_yml)){
                            return p_yml.string();
                        }
                    }

                    if(bfs::exists(p)){
                        if(bfs::is_directory(p)){
                            try{
                                // Recrusively search for a file of the same name with .yaml within this directory.
                                return findArchitectureConfigFile({p.string()}, name);
                            }catch(SpartaException&){
                                throw SpartaException("Searched for Architecture config \"")
                                    << name << "\" in \"" << search_dir
                                    << "\" did not yield any results. Subdirectory of the arch search dir \""
                                    << p.string() << "\" exists but does not contain a .yaml file of the same name, "
                                    "which is required if using a directory to represent an architecture. "
                                    << ARCH_OPTIONS_RESOLUTION_RULES;
                            }
                        }else if(bfs::is_regular_file(p)){
                            // Found regular file with input name
                            return p.string();
                        }else if(bfs::is_symlink(p)){
                            auto slp = bfs::read_symlink(p);
                            if(slp == bfs::path()){
                                // Empty path object means we could not read symlink
                            }else if(bfs::is_directory(slp)){
                                // Symlink was a dir
                                try{
                                    // Recrusively search for a file of the same name with .yaml within this directory.
                                    return findArchitectureConfigFile({p.string()}, name);
                                }catch(SpartaException&){
                                    throw SpartaException("Searched for Architecture config \"")
                                        << name << "\" in \"" << search_dir
                                        << "\" did not yield any results. Subdirectory symlink of the arch search dir \""
                                        << p.string() << "\" exists but does not contain a .yaml file of the same name, "
                                        "which is required if using a directory to represent an architecture. "
                                        << ARCH_OPTIONS_RESOLUTION_RULES;
                                }
                            }else if(bfs::is_regular_file(slp)){
                                // Symlink was regular file. Return the input path not the canonical
                                return p.string();
                            }else{
                                // Symlink was not a dir or file. continue and fail if there are
                                // no more resolutions to try
                            }
                        }
                    }
                }
            }
            // Failure and no directory existed to search into
            throw SpartaException("Searched for Architecture config \"")
                << name << "\" in \"" << search_dirs
                << "\" did not yield any results. The given name did not correspond to any files even "
                "if .yaml/.yml was appended. " << ARCH_OPTIONS_RESOLUTION_RULES;
            return "";
        }
    }
}
