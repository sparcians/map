// <MemoryProfiler.hpp> -*- C++ -*-

/**
 * \file   MemoryProfiler.hpp
 * \brief  Simple class to see where memory is being used.  Generates reports
 */

#ifndef __SPARTA_MEMORY_PROFILER_H__
#define __SPARTA_MEMORY_PROFILER_H__

#include <memory>
#include <string>

namespace sparta {

class TreeNode;

namespace app {
    class Simulation;
}

/**
 * \brief Utility used to periodically collect heap usage
 *        statistics throughout a simulation. Supported phases
 *        for inspection include Build, Configure, Bind, and
 *        Simulate.
 *
 * See the command line option '--log-memory-usage' for more
 * information.  Simulation must be built with tcmalloc.
 */
class MemoryProfiler
{
public:
    MemoryProfiler(const std::string & def_file,
                   TreeNode * context,
                   app::Simulation * sim);

    enum class Phase {
        Build,
        Configure,
        Bind,
        Simulate
    };
    void enteringPhase(const Phase phase);
    void exitingPhase(const Phase phase);
    void saveReport();
    void saveReportToStream(std::ostream & os);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}

#endif
