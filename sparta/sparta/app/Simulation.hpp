// <Simulation.h> -*- C++ -*-


/*!
 * \file Simulation.hpp
 * \brief Simulation setup base class
 */
#pragma once

#include <vector>

#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/report/ReportRepository.hpp"
#include "sparta/app/Backtrace.hpp"
#include "sparta/app/ConfigApplicators.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/control/TemporaryRunController.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/pipeViewer/InformationWriter.hpp"

namespace YP = YAML; // Prevent collision with YAML class in ConfigParser namespace.

namespace sparta {

class Clock;
class MemoryProfiler;
class DatabaseAccessor;

namespace python {
    class PythonInterpreter;
}
namespace control {
    class TemporaryRunControl;
}

namespace trigger {
    class SingleTrigger;
    class CounterTrigger;
    class TimeTrigger;
    class CycleTrigger;
    class Trigger;
} // namespace trigger

/*!
 * \brief Sparta Application framework
 */
namespace app {
class LoggingTrigger;
class ConfigApplicator;
class FeatureConfiguration;

/*!
 * \class Simulation
 * \brief Simulator which builds a sparta DeviceTree
 */
class Simulation
{
public:

    /*!
     * \brief Types of semantics attached to certain counters. It is
     *        the responsibility of subclasses to implement
     *        findSemanticCounter such taht it can satisfy requests
     *        for counters having these semantics
     */
    enum CounterSemantic {
        CSEM_INSTRUCTIONS = 0 //!< Instruction count semantic (usually core0)
    };

    //! \brief Not default-constructable
    Simulation() = delete;

    //! \brief Not copy-constructable
    Simulation(const Simulation&) = delete;

    //! \brief Not move-constructable
    Simulation(Simulation&&) = delete;

    //! \brief Not assignable
    Simulation& operator=(const Simulation&) = delete;

    /*!
     * \brief Deferred configuration constructor. Subsequent call to configure
     *        must be made before building/configuring/finalizing.
     * \param sim_name Name of the simulator
     * \param scheduler Pointer to the Scheduler that this Simulation operates with
     */
    Simulation(const std::string& sim_name, Scheduler * scheduler);

    /*!
     * \brief Virtual destructor
     */
    virtual ~Simulation();

    /*!
     * \brief Set a collection of feature name-value pairs.
     * \param feature_config Pointer to a feature configuration class
     *
     * Typically given to us by the CommandLineSimulator who populates
     * the feature values using a command-line "feature" option.
     */
    void setFeatureConfig(const FeatureConfiguration * feature_config) {
        feature_config_ = feature_config;
    }

    /*!
     * \brief Get the database root for this simulation.
     * \return Pointer to the DatabaseRoot
     *
     * This is a container that holds all databases the simulation is
     * using.  The underlying ObjectManager methods such as getTable()
     * and findObject() can be accessed indirectly using the
     * ObjectDatabase class (nested class inside ObjectManager). For
     * example, say that we ran a simulation using the --report
     * command line option, and we want to go through the DatabaseRoot
     * to get the StatisticInstance / reports database records:
     *
     * \code
     *     simdb::DatabaseRoot * db_root = sim->getDatabaseRoot();
     *
     *     simdb::DatabaseNamespace * stats_namespace = db_root->
     *         getNamespace("Stats");
     *
     *     simdb::ObjectManager::ObjectDatabase * stats_db =
     *         stats_namespace->getDatabase();
     * \endcode
     *
     * Once you have the ObjectDatabase for the desired namespace,
     * access the table wrappers like so:
     *
     * \code
     *     std::unique_ptr<simdb::TableRef> ts_table =
     *         stats_db->getTable("Timeseries");
     * \endcode
     *
     * See "simdb/include/simdb/schema/DatabaseRoot.hpp" and
     * "simdb/include/simdb/ObjectManager.hpp" for more info
     * about using these other classes to read and write
     * database records in a SimDB namespace.
     */
    simdb::DatabaseRoot * getDatabaseRoot() const;

    /*!
     * \brief There is a 1-to-1 mapping between a running simulation
     * and the database it is using. Some components in the simulator
     * may have database access, while others are not intended to use
     * the database. This is controlled via command line arguments, and
     * the simulation's DatabaseAccessor knows which components are DB-
     * enabled, and which are not.
     */
    const DatabaseAccessor * getSimulationDatabaseAccessor() const;

    /*!
     * \brief Configures the simulator after construction. Necessary only when
     *        using the simple constructor
     *
     * \param argc command line argc (used by the python shell)
     * \param argv command line argv (used by the python shell)
     * \param configuration The SimulationConfiguration object to
     *                      initialize this Simulation object
     * \param use_pyshell Enable the python shell (experimental)
     */
    void configure(const int argc,
                   char ** argv,
                   SimulationConfiguration * configuration,
                   const bool use_pyshell = false);

    /*!
     * \brief Add a report
     * \pre Must be done before root is finalized or finalizing
     * \param rep Descriptor of report to add
     */
    void addReport(const ReportDescriptor & rep);

    /*!
     * \brief Add new taps the the simulation immediately IF possible.
     * \post Warnings will be printed directly to cerr if any taps cannot be
     * added.
     * \note These taps cannot be removed and are not tracked like those added
     * through configure() or the constructor.
     *
     * This method exists mainly to allow delayed starting of logging
     */
    void installTaps(const log::TapDescVec& taps);

    //! \brief Returns the tree root
    sparta::RootTreeNode* getRoot() noexcept { return &root_; }

    //! \brief Returns the Meta TreeNode root.
    sparta::app::MetaTreeNode* getMetaParamRoot() const noexcept { return meta_.get(); }

    //! \brief Returns the tree root (const)
    const sparta::RootTreeNode* getRoot() const noexcept { return &root_; }

    //! \brief Returns the simulation's scheduler
    sparta::Scheduler * getScheduler() { return scheduler_; }

    //! \brief Returns the simulation's scheduler
    const sparta::Scheduler * getScheduler() const { return scheduler_; }

    /*!
     * \brief Returns whether or not the simulator was configured using a final
     * configuration option, likely via the --read-final-config cml option.
     */
    bool usingFinalConfig() const { return using_final_config_; }

    //! \brief Is the framework ready to run
    bool readyToRun() const { return framework_finalized_; }

    //! \brief Returns the root clock
    sparta::Clock* getRootClock() noexcept { return root_clk_.get(); }

    //! \brief Returns the root clock (const)
    const sparta::Clock* getRootClock() const noexcept { return root_clk_.get(); }

    //! \brief Returns the clock manager
    sparta::ClockManager & getClockManager() noexcept { return clk_manager_; }

    //! \brief Returns the resource set for this Simulation
    sparta::ResourceSet* getResourceSet() noexcept { return &res_list_; }

    /*!
     * \brief Returns this simulator's name
     */
    const std::string& getSimName() const noexcept {
        return sim_name_;
    }

    /*!
     * \brief Returns this simulator's configuration
     */
    SimulationConfiguration * getSimulationConfiguration() const {
        return sim_config_;
    }

    /*!
     * \brief Returns this simulator's feature configuration
     */
    const FeatureConfiguration * getFeatureConfiguration() const {
        return feature_config_;
    }

    /*!
     * \brief Get this simulator's report configuration
     */
    ReportConfiguration * getReportConfiguration() const {
        return report_config_.get();
    }


    // Setup

    /*!
     * \brief Builds hard-coded device tree
     * \note Result can be dumped with getRoot()->renderSubtree(-1, true);
     * to show everything in the device tree.
     * \brief Root will be in the TreeNode::TREE_BUILDING phase
     */
    void buildTree();

    /*!
     * \brief Configure the tree with some node-local config files, params,
     * and node-specific parameters
     * \pre Tree must be in building phase. Check getRoot()->isBuilding()
     * \brief Root will be in the TreeNode::TREE_CONFIGURING phase
     */
    void configureTree();

    /*!
     * \brief Finalizes the device tree
     * \pre Tree must be in configuring phase. Check getRoot()->isConfiguring()
     * \post Tree will be finalized. Check getRoot()->isFinalized()
     * \post Root will be in the TreeNode::TREE_FINALIZED phase
     */
    void finalizeTree();


    /*!
     * \brief Finalize framework before running
     * \pre Tree will be in the TreeNode::TREE_FINALIZED phase. Check
     * getRoot()->isFinalized()
     */
    void finalizeFramework();


    // Running

    /*!
     * \brief Run for specified "time" or less
     * \pre Device tree and framework must be finalized.
     * and ensure finalizeFramework is called. Check readyToRun().
     * \param run_time scheduler run_time argument
     * \note Invokes runRaw_after setting up the dag. Catches exceptions from
     * runRaw_ and outputs summary or debug dumps as necessary
     */
    virtual void run(uint64_t run_time);

    /*!
     * \brief Runs the simulation for a limited run time, returning when done.
     *
     * This can be called multiple times for a simulator's lifetime and
     * indicates a run of the simulator which can be interrupted by components
     * within the simulator.
     *
     * This should include no simulator-level setup code since it may be called
     * multiple (unknown number of) times (never recursively) throughout the
     * lifetime of the simulation.
     *
     * By default, this is done through the SPARTA Scheduler. This method has no
     * exception handling or setup. It should be overridden if the scheduler is
     * not being used to run the simulation (e.g. simulation is functional-only
     * and purely trace-driven).
     */
    virtual void runRaw(uint64_t run_time);

    /*!
     * \brief Asynchronously stop the run
     * \warning This is not thread safe. This is just a call that can come from
     * a signal handler or within the simulator model code to stop at the next
     * tick boundary.
     */
    virtual void asyncStop();


    // Post-processing

    /*!
     * \brief Determine if debug content needs to be dumped and dump if so.
     * Uses dumpDebugContent to dump.
     * \param[in] eptr Exception pointer to inspect
     * \param[in] force Force the dump regardless of configuration
     */
    void dumpDebugContentIfAllowed(std::exception_ptr eptr, bool forcec=false) noexcept;

    /*!
     * \brief Writes all reports to their respective files
     */
    void saveReports();

    /*!
     * \brief After CommandLineSimulator runs the simulation and calls
     * all post-processing APIs, this method will be called. It is the
     * only thing left before our destructor gets called. Do any last-
     * minute final wrap up work if needed in this method.
     */
    void postProcessingLastCall();

    /*!
     * \brief Get a counter by its semantic (if such a counter exists).
     * \param sem Semantic of counter to return
     * \note uses virtual findSemanticCounter_
     * \pre Simulation must be finalized and not in teardown
     */
    const CounterBase* findSemanticCounter(CounterSemantic sem) const {
        sparta_assert(root_.isFinalized(),
                          "Cannot query findSemanticCounter until Simulation is finalized");
        sparta_assert(!root_.isTearingDown(),
                          "Cannot query findSemanticCounter after Simulation has entered teardown");
        return findSemanticCounter_(sem);
    }


    // Status

    /*!
     * \brief Was simulation successful?
     * \return true if no exceptions were thrown; false otherwise
     */
    bool simulationSuccessful() const { return simulation_successful_; }

    /*!
     * \brief return the number of events fired on the scheduler
     * during simulation.
     */
    uint64_t numFired() { return num_fired_; }

    /*!
     * \brief Enable post-run validation explicity
     */
    void enablePostRunValidation() {
        validate_post_run_ = true;
    }
     /*!
      * \brief Gets the pipeline collection path
      */
    std::string getPipelineCollectionPrefix() const {
        return pipeline_collection_prefix_;
    }

    /*!
     * \brief Was this simulation configured with a Python shell?
     * \note Query this after finalizeFramework() is true. Configuration
     * of the simulator can happen after construction but always before
     * the simulation is built (buildTree_). While finalizeFramework()
     * becomes true much later, it is a safe place to query.
     */
    bool usingPyshell() const {
#ifdef SPARTA_PYTHON_SUPPORT
        return pyshell_ != nullptr;
#else
        return false;
#endif
    }

    /*!
     * \brief Get the run control interface for this simulation. This must
     * exist for the lifetime of this simulation.
     *
     * This is overridable so that simulators can instantiate and return
     * their own run control implementation.
     */
    virtual sparta::control::TemporaryRunControl * getRunControlInterface() {
        return rc_.get();
    }

    /*!
     * \brief Write meta-data tree parameters to given ostream
     * \param[in] out osteam to which meta parameter values will be written
     */
    void dumpMetaParameterTable(std::ostream& out) const;


protected:

    /*!
     * \brief This class is used for simulation control callbacks. The callback
     * conditions (trigger expressions) are specified in control YAML files.
     */
    class SimulationController
    {
    public:
        virtual ~SimulationController() {}

        void pause();
        void resume();
        void terminate();

        void invokeNamedEvent(
            const std::string & event_name);

        enum class SimulationStatus {
            Idle,
            Paused,
            Simulating,
            Terminated
        };
        SimulationStatus getSimStatus() const {
            return sim_status_;
        }

    protected:
        explicit SimulationController(
            const sparta::app::Simulation * sim) :
            sim_(sim)
        {}

        void addNamedCallback_(
            const std::string & event_name,
            SpartaHandler callback_method);

    private:
        virtual void pause_(const sparta::app::Simulation * sim) {
            (void) sim;
        }
        virtual void resume_(const sparta::app::Simulation * sim) {
            (void) sim;
        }
        virtual void terminate_(const sparta::app::Simulation * sim) {
            (void) sim;
        }
        void verifyFinalized_() const;

        const sparta::app::Simulation * sim_ = nullptr;
        std::unordered_map<std::string, SpartaHandler> callbacks_;
        std::set<std::string> invoked_callbacks_;
        SimulationStatus sim_status_ = SimulationStatus::Idle;

        friend class sparta::app::Simulation;
    };

    /*!
     * \brief Set a controller to handle custom simulation events
     */
    void setSimulationController_(
        std::shared_ptr<SimulationController> controller);

    /*!
     * \brief Triggered simulation pause callback
     */
    void pause_();

    /*!
     * \brief Triggered simulation resume callback
     */
    void resume_();

    /*!
     * \brief Triggered simulation terminate callback
     */
    void terminate_();

    /*!
     * \brief Triggered simulation custom named event
     */
    void customEvent_(const std::string & event_name);

    /*!
     * \brief Initialization of custom simulation event triggers
     */
    void setupControllerTriggers_();

    /*!
     * \brief Custom controller to handle various simulation events
     */
    std::shared_ptr<SimulationController> controller_;

    /*!
     * \brief Expression triggers to invoke custom simulation events via
     * the SimulationController object
     */
    std::vector<std::shared_ptr<trigger::ExpressionTrigger>> controller_triggers_;

    /*!
     * \brief Print and count the number of Parameters which have had their
     * values changed to something different than the default.
     * \note "default" refers to the parameter default OR the architecture (--arch)
     * "default" override.
     * \note uses sparta::ParameterBase::isDefault
     * \return Number of non-default parameters
     */
    uint32_t dumpNonDefaultParameters_(TreeNode* root, std::ostream& out);

    /*!
     * \brief Same counting behavior as dumpNonDefaultParameters_ but does not
     * print.
     */
    uint32_t countNonDefaultParameters_(TreeNode* root);

    /*!
     * \brief Re-read volatile parameter values from the virtual tree
     * and write them to the parmeters. This is used by simulators if special
     * parmeter application ordering is required.
     * \param[in] root Node at which a search for ParameterSets is started. Each
     * ParameterSet found as an ancestor of this node will have its volatile
     * params updated
     * \warning Do not pass a Parameter node directly as it will not be updated.
     */
    uint32_t reapplyVolatileParameters_(TreeNode* root);

    /*!
     * \brief Re-read all parameter values from the virtual tree.
     * \see reapplyVolatileParameters_ for precondition, return, and params
     */
    uint32_t reapplyAllParameters_(TreeNode* root);

    /*!
     * \brief Include an extension factory for this simulation's device tree nodes.
     * They will be given to specific tree nodes right after they are instantiated.
     * Which extension(s) go to which tree node(s) is determined by configuration
     * files.
     */
    void addTreeNodeExtensionFactory_(const std::string & extension_name,
                                      std::function<TreeNode::ExtensionsBase*()> creator);

    //! \name Virtual Setup Interface
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Allows implementer to create new nodes in the tree
     */
    virtual void buildTree_() = 0;

    /*!
     * \brief Allows implementer to manually configure the tree if required.
     * \pre Tree will be built
     * \pre Tree will have all command-line parameters applied
     */
    virtual void configureTree_() = 0;

    /*!
     * \brief Allows implementer to bind ports to gether.
     * \pre Tree will be finalized
     * \pre Logging taps specified on the command line will be attached to
     * appropriate nodes
     */
    virtual void bindTree_() = 0;

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Enter run-control loop causing the simulator to run or give
     * control to an interactive shell.
     * \note Subclasses may override this method to provide custom state/setup
     * or exception handling. This is called once per simulation lifetime.
     * \note Exceptions generated within the model should be re-thrown up to
     * the caller of this function so that SPARTA can properly log failures and
     * quit with a proper exit.
     */
    virtual void runControlLoop_(uint64_t run_time);

    /*!
     * \brief Run for specified "time"
     * \pre Device tree and framework must be finalized.
     * and ensure finalizeFramework is called. Check readyToRun().
     * \param run_time scheduler run_time argument
     * \note Invokes runRaw_() after setting up the dag. Catches exceptions
     * from runRaw_() and outputs summary or debug dumps as necessary.
     */
    virtual void runRaw_(uint64_t run_time);

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Implements findSemanticCounter
     * \param sem Semantic of counter to return
     * \pre Simulation must be finalized and not in teardown
     * \return Pointer to counter with this semantic of nullptr if none. This
     * method must return the same pointer each call.
     */
    virtual const CounterBase* findSemanticCounter_(CounterSemantic sem) const {
        (void) sem;
        return nullptr;
    }

    /*!
     * \brief Dump debug content to a timestamped file regardless of
     * debug-dumping control flags
     * \param filename Filename of output or "" if output was written to
     * cerr. Updated by this method even if the are errors
     * \param exception Exception to write at top of tile
     * \param backtrace String describing backtrace at the time the exception
     * was thrown
     * \return true if there were no issues. False if output is possible
     * truncated due to an internal exception
     */
    bool dumpDebugContent_(std::string& filename,
                           const std::string& exception,
                           const std::string& backtrace) noexcept;

    /*!
     * \brief a Callback that officially starts pevent collection.
     */
    void delayedPeventStart_();
    /*!
     * \brief Notification callback invoked when a new node is attached as a
     * descendant of this simulator's root node
     */
    void rootDescendantAdded_(const sparta::TreeNode& node_added);

    /*!
     * \brief Creates and attaches a tap to a node based on its descriptor
     * \param td Descriptor of tap to create and attach
     * \param n Node to which tap should be attached
     * \post The tap will be created and appended to the taps_to_del_ list.
     * \post td will have its usage count incremented
     */
    void attachTapTo_(const log::TapDescriptor& td, sparta::TreeNode* n);

    /*!
     * \brief Creates and attaches a report to a node based on its descriptor
     * \param directoryH Repository directory where the new report goes
     * \param def_file Report definition file (.yaml)
     * \param n Node to which report should be attached
     * \param replacements Set of replacements to give to the report definition
     * parser as substitutions in the report name or report's local stat names
     * \post The report will be created and appended to the repository
     */
    void attachReportTo_(sparta::ReportRepository::DirectoryHandle directoryH,
                         const ReportDescriptor & rd,
                         sparta::TreeNode* n,
                         const std::vector<std::string>& replacements);

    /*!
     * \brief Check that all virtual parameters have been read from a given tree
     * \param[out] pt ParameterTree to inspect
     * \throw SpartaException if any parameters are unread. Exception will explain
     * which parameter is a problem and where it came from (e.g. which file)
     */
    void checkAllVirtualParamsRead_(const ParameterTree& pt);

    /*!
     * \brief Verify that the given report descriptor can be added to
     * this simulation
     */
    void validateDescriptorCanBeAdded_(
        const ReportDescriptor & rd,
        const bool using_pyshell) const;

    /*!
     * \brief Sets up all reports in this simulation. This can be called during
     * finalization or deferred until later.
     */
    void setupReports_();

    /*!
     * \brief Right before the main sim loop, this method is called in order
     * to create any SimDB triggers the simulation was configured to use.
     * These triggers dictate when database namespace(s) are opened and
     * closed for reads and writes via the simdb::TableProxy class.
     */
    void setupDatabaseTriggers_();

    /*!
     * \brief In the case where a comma-separated list of file formats was
     * used to specify the output file format (i.e. 'csv, csv_cumulative'),
     * this method will expand the report descriptors given to this simulation
     * appropriately.
     */
    ReportDescVec expandReportDescriptor_(const ReportDescriptor & rd) const;

    /*!
     * \brief Sets up any heap profiler(s) used in this simulation
     */
    void setupProfilers_();

    /*!
     * \brief If report statistics are being streamed out of this simulation,
     * share the run controllers's stream controller object with each of the
     * statistics stream root nodes. To illustrate:
     *
     *   Simulation
     *       - owns a RunController
     *           - *shares* a stream controller          <--|
     *       - owns a ReportRepository                      |
     *           - owns report / SI hierarchies             |- make this connection
     *               - each has a root StreamNode           |
     *                   - *shares* a stream controller  <--|
     */
    void setupStreamControllers_();

    /*! \brief Clock manager for all clocks in simulation.
     *
     * This can be created first and destroyed last in case there are
     * resources/reports that still need use of the clocks
     */
    sparta::ClockManager clk_manager_;

    /*!
     * \brief Heap profiler(s), if any
     */
    std::shared_ptr<sparta::MemoryProfiler> memory_profiler_;

    /*!
     * \brief Repository of all reports for this simulation
     */
    std::unique_ptr<sparta::ReportRepository> report_repository_;

    /*!
     * \brief Backtracing utility for error signals
     */
    Backtrace backtrace_;

    /*!
     * \brief Simulation name
     */
    const std::string sim_name_;

    /*!
     * \brief User-specified Taps to delete at teardown. These should outlast
     * the entire tree so that they can intercept destruction log messages
     */
    std::vector<std::unique_ptr<sparta::log::Tap>> taps_to_del_;

    /*!
     * \brief Map of of resources available to this simulation.
     * This must outlast destruction of the tree root_ and its children
     */
    sparta::ResourceSet res_list_;

    /*!
     * \brief Scheduler this simulation will use. If no scheduler was given
     * to the simulation's constructor, it will use the singleton scheduler.
     */
    Scheduler *const scheduler_;

    /*!
     * \brief Default automaticly generated report containing the entire
     * simulation
     */
    std::unique_ptr<Report> auto_summary_report_;

    /*!
     * \brief Root node containing the clock tree (called "clocks")
     */
    std::unique_ptr<sparta::RootTreeNode> clk_root_node_;

    /*!
     * \brief Root of clock tree (direct child of clk_root_node_). Represents
     * hypercycles. Destruct after all nodes
     */
    Clock::Handle root_clk_;

    /*!
     * \brief Root of device tree: "top". Destruct after all non-root nodes
     * (see to_delete_)
     */
    sparta::RootTreeNode root_;

    /*!
     * \brief Meta-tree containing simulation meta-information in parameters
     * in "meta.params.fizbin". Destruct after all non-root nodes
     * (see to_delete_)
     * \note Created after parameter preprocessing since it has some parameters
     * of its own.
     */
    std::unique_ptr<MetaTreeNode> meta_;

    /*!
     * \brief Tree node extension factories by name.
     */
    std::unordered_map<
        std::string,
        std::function<TreeNode::ExtensionsBase*()>> tree_node_extension_factories_;

    /*!
     * \brief Has the framework been finalized
     */
    bool framework_finalized_ = false;

    /*!
     * \brief Vector of TreeNodes to delete automatically at destruction.
     * Add any nodes allocated to this list to automatically manage them.
     */
    std::vector<std::unique_ptr<sparta::TreeNode>> to_delete_;


    // The SimulationConfiguration object
    SimulationConfiguration * sim_config_{nullptr};

    // The FeatureConfiguration object
    const FeatureConfiguration * feature_config_{nullptr};

    // Order matters: Taps should die before any other nodes (or after all other
    // nodes are guaranteed destructed)

    sparta::log::Tap warn_to_cerr_; //!< Tap which warns to cerr

    /*!
     * \brief Tap which, if constructed, will write all warnings to a file
     */
    std::unique_ptr<sparta::log::Tap> warn_to_file_;

    uint64_t num_fired_; //!< Total number of scheduler events firerd

    bool print_dag_ = false; //!< Should the DAG be printed after it is built

    /*!
     * \brief Validate after running
     */
    bool validate_post_run_ = false;

    /*!
     * \brief Pipeline collection prefix
     */
    std::string pipeline_collection_prefix_;

    /*!
     * \brief Vector of Report descriptors applicable to the simulation.
     * These descriptors are used as the tree is built to find nodes on which to
     * instantiate them. These are still used following building of the device
     * tree in order to determine where the completed reports should be saved.
     * This must be destroyed or no longer accessed once the reports_ vector is
     * destroyed
     */
    ReportDescriptorCollection rep_descs_;

    /*!
     * \brief Report configuration object which wraps the simulation's
     * ReportDescriptorCollection
     */
    std::unique_ptr<ReportConfiguration> report_config_;

    /*!
     * \brief Keep the extension descriptors alive for the entire simulation.
     * The Simulation base class is meant to actually allocate and own this
     * memory (named parameter sets) even though simulation subclasses are
     * the only ones who use these extended parameters sets.
     */
    ExtensionDescriptorVec extension_descs_;
    std::set<std::string> nodes_given_extensions_;

    /*!
     * \brief User configuration vector stored at "preprocessParameters"
     * \note Does not own memory.
     */
    std::vector<ConfigApplicator*> user_configs_;

    /*!
     * \brief tells the simulator that we are using the final config option and need
     *        to prevent parameter set callbacks and the simulator itself cannot change
     *        parameters.
     */
    bool using_final_config_ = false;

    /*!
     * \brief Warmup period in instructions before logging pevents
     */
    uint64_t pevent_warmup_icount_ = 0;

    /*!
     * \brief Callback for pevent startup
     */
    SpartaHandler pevent_start_handler_;

    /*!
     * \brief An instruction trigger for observing pevent warmup
     */
    std::unique_ptr<trigger::CounterTrigger> pevent_start_trigger_;

    /*!
     * \brief Trigger for starting logging
     */
    std::unique_ptr<LoggingTrigger> log_trigger_;

    /*!
     * \brief A trigger used to turn on debug useful options
     * at a given cycle, right now this only turns on pipeline
     * collection
     */
    std::unique_ptr<trigger::Trigger> debug_trigger_;

    /*!
     * \brief Was simulation successful?  I.e. no exceptions were thrown
     */
    bool simulation_successful_ = true;

private:

#ifdef SPARTA_PYTHON_SUPPORT
    /*!
     * \brief Python interpreter (TEMPORARY)
     */
    std::unique_ptr<python::PythonInterpreter> pyshell_;
#endif

    /*!
     * \brief A specialized sparta::State which tracks
     * the Simulation and processes special task
     * before Simulation is torn down.
     */
    sparta::State<sparta::PhasedObject::TreePhase> simulation_state_;

    /*!
     * \brief Simulator-specific report descriptor validation. This is
     * called right *after* the report descriptors have been finalized,
     * but right *before* they have been used to create any sparta::Report
     * instantiations.
     *
     * \param report_descriptors This is the set of all descriptors this
     * simulator is using to generate all its reports, populated from all
     * --report <descriptor.yaml> file(s) fed into the simulator from the
     * command line.
     *
     * An example of when you might override this method is if you want
     * to check if your simulator is using --auto-summary together with
     * a report warmup trigger. The auto summary ignores report warmup
     * and stop triggers, which would result in unexpected SI values if
     * you simultaneously created .out files (--auto-summary on) while
     * also generating a JSON report that was triggered off of warmup
     * triggers. The SI values would differ from the .out to the .json
     * despite coming from the same set of StatisticInstance's for the
     * same simulation configuration. You may want to detect that scenario
     * and throw or warn, for instance.
     */
    virtual void validateReportDescriptors_(
        const ReportDescriptorCollection & report_descriptors) const
    {
        (void) report_descriptors;
    }

    /*!
     *\brief Run controller interface
     */
    std::unique_ptr<control::TemporaryRunControl> rc_;

    /*!
     * \brief This database holds things like report
     * metadata and StatisticInstance values.
     *
     * \note This is not publicly accessible. Think of
     * this pointer as a "shortcut" to the SI / reports
     * namespace in the simulation database; it is used
     * so often in Simulation.cpp, and passed around to
     * other classes related to reports / post-processing
     * that we hang onto is as a convenience.
     */
    simdb::ObjectManager * stats_db_ = nullptr;

    /*!
     * \brief Database root for this simulation. This is a container
     * holding all database connections currently in use. The reports /
     * SI database tables & records are just a subset of what can be
     * accessed via the DatabaseRoot. Other databases may be available
     * as well (pipeline collection DB, branch prediction DB, etc.)
     */
    std::unique_ptr<simdb::DatabaseRoot> db_root_;

    /*!
     * \brief Accessor who knows which simulation components are
     * enabled for SimDB access, and which are not.
     */
    std::shared_ptr<DatabaseAccessor> sim_db_accessor_;
private:
    //! At the very end of the simulation's configure() method,
    //! take a first look at the feature values we were given.
    void inspectFeatureValues_();

    //! Tests may enable a post-simulation report validation
    //! step, where contents of the database (SI values, metadata,
    //! etc.) are compared against a baseline report.
    bool isReportValidationEnabled_() const;

    //! List of report filenames which *failed* post-simulation
    //! report verification.
    std::set<std::string> report_verif_failed_fnames_;
};

} // namespace app
} // namespace sparta
