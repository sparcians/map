// <PythonInterpreter.h> -*- C++ -*-


/*!
 * \file PythonInterpreter.h
 * \brief Instantiates python interpreter instance
 */

#pragma once

#include "sparta/sparta.hpp" // For macro definitions
#include "simdb_fwd.hpp"

#pragma once
#define _GNU_SOURCE
#endif
#pragma once
#endif

#include <signal.h>
#include <string>
#include <memory>
#include <Python.h>

namespace sparta {

class RootTreeNode;

namespace app {
    class Simulation;
    class SimulationConfiguration;
    class ReportConfiguration;
}

namespace control {
    class TemporaryRunControl;
}

namespace statistics {
    class StatisticsArchives;
    class StatisticsStreams;
}

namespace async {
    class AsynchronousTaskEval;
}

namespace python {

    /*!
     * \brief Stack-based GIL lock
     */
    class LocalGIL {
        PyGILState_STATE gstate_;
    public:
        LocalGIL() :
            gstate_(PyGILState_Ensure())
        {}

        ~LocalGIL() {
            PyGILState_Release(gstate_);
        }

    };

    /*!
     * \brief Wraps pyhton initialization into a class
     *
     * \warning Python can only be initlized once per process. Multiple interpreters
     * can be created. This initializeds Python and creates interpreters. Do not
     * instantiate if Python is initialized already.)
     *
     * \note May be able to instantiate this multiple times per process as long as
     * lifestpans do not overlap but this is untested and no known use exists at
     * this point
     */
    class PythonInterpreter {
        std::unique_ptr<char[]> progname_;
        std::unique_ptr<char[]> homedir_;

        struct sigaction sigint_act_; //!< signal action
        struct sigaction sigint_next_; //!< Next handler in chain (replaced by this class)

        /*!
         * \brief Run control interface currently being used
         * \todo Support multiple RC interfaces
         */
        control::TemporaryRunControl* run_controller_ = nullptr;

    public:
        /*!
         * \brief Helper to statically track that one intpreter instance exists at a time
         *
         * Sets flag when created and clears flag when destructed. Instantiated through a member
         * of interpreter so that it will always be cleanly destructed (setting flag to false)
         * when the interpreter class is destroyed no matter where the owning class fails
         *
         * This is necessary to help ensure that one instance of this class exist at a time so that
         * signal handlers can be properly mainained
         */
        class SingleInstanceForce {
            static PythonInterpreter* curinstance_;

        public:
            SingleInstanceForce(PythonInterpreter* inst) {
                sparta_assert(curinstance_ == nullptr,
                            "Attempted to create a new Python interpreter instance while another was still alive.");
                curinstance_ = inst;
            }

            ~SingleInstanceForce() {
                curinstance_ = nullptr;
            }

            static PythonInterpreter* getCurInstance() { return curinstance_; }
        };

        PythonInterpreter(const std::string& progname,
                          const std::string& homedir,
                          int argc,
                          char** argv);

        ~PythonInterpreter();

        //! ========================================================================
        //! Global State
        //! @{

        std::string getExecPrefix() const;

        std::string getPythonFullPath() const;

        std::string getPath() const;

        std::string getVersion() const;

        std::string getPlatform() const;

        std::string getCompiler() const;

        void publishSimulationConfiguration(app::SimulationConfiguration * sim_config);

        void publishReportConfiguration(app::ReportConfiguration * report_config);

        void publishStatisticsArchives(statistics::StatisticsArchives * archives);

        void publishStatisticsStreams(statistics::StatisticsStreams * streams);

        void publishSimulationDatabase(simdb::ObjectManager * sim_db);

        void publishDatabaseController(simdb::AsyncTaskEval * db_queue);

        void publishSimulator(app::Simulation * sim);

        void publishTree(RootTreeNode * n);

        void publishRunController(control::TemporaryRunControl* rc);

        void removePublishedObject(const void * obj_this_ptr);

        /*!
         * \brief Temporary interactive REPL loop
         * \post Updates exit code with simulation result. Should not allow Python or C++ exceptions
         * to bubble out of this function except for framework errors (C++ exceptions only)
         * \see getExitCode
         */
        void interact();

        /*!
         * \brief Handle a SIGINT signal
         */
        void handleSIGINT(siginfo_t * info, void * ucontext);

        /*!
         * \brief Exit the shell and return control from interact
         */
        void asyncExit(int exit_code);

        /*!
         * \brief IPython hook callback handler before each prompt display
         */
        void IPyPrePrompt(PyObject* embed_shell);

        /*!
         * \brief IPython hook callback handler once IPython shell is initialized
         */
        void IPyShellInitialized();

        /*!
         * \brief Return the exit code set by asyncExit (0 not asyncExit not called)
         */
        int getExitCode() const { return exit_code_; }

    private:

        SingleInstanceForce sif_;

        std::unique_ptr<PyObject, void (*)(PyObject*)> ipython_inst_;

        int exit_code_ = 0;

        //! Mapping from void* (published object's this pointer) to the
        //! Python variable name it was published to.
        std::unordered_map<const void*, std::string> published_obj_names_;
    };

} // namespace python
} // namespace sparta

#pragma once
