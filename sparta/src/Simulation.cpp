
#include <algorithm>
#include <exception>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/any.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/timer/timer.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <cstdint>
#include <ctime>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <yaml-cpp/parser.h>

#include "sparta/app/Simulation.hpp"
#include "sparta/app/AppTriggers.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/report/format/Text.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/utils/File.hpp"
#include "sparta/parsers/YAMLTreeEventHandler.hpp"
#include "src/State.tpp"
#include "sparta/kernel/MemoryProfiler.hpp"
#include "sparta/statistics/dispatch/streams/StatisticsStreams.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/impl/hdf5/HDF5ConnProxy.hpp"
#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/schema/DatabaseRoot.hpp"
#include "simdb/Errors.hpp"
#include "sparta/report/db/Schema.hpp"
#include "sparta/report/db/SimInfoSerializer.hpp"
#include "sparta/report/db/ReportVerifier.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/kernel/PhasedObject.hpp"
#include "sparta/report/DatabaseInterface.hpp"
#include "simdb/schema/Schema.hpp"
#include "simdb/schema/TableSummaries.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/utils/Colors.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/report/ReportRepository.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/kernel/SleeperThreadBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/app/Backtrace.hpp"
#include "sparta/app/ConfigApplicators.hpp"
#include "sparta/app/MetaTreeNode.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/control/TemporaryRunController.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/log/Destination.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/log/categories/CategoryManager.hpp"
#include "sparta/statistics/dispatch/streams/StreamNode.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/utils/StringUtils.hpp"

namespace YAML {
class EventHandler;
}  // namespace YAML

namespace sparta::report::format {
class BaseFormatter;
}  // namespace sparta::report::format

namespace sparta::statistics {
class StreamController;
}  // namespace sparta::statistics


#ifdef SPARTA_PYTHON_SUPPORT
#include "python/sparta_support/PythonInterpreter.hpp"
#endif


namespace sparta {
namespace app {

#ifdef SPARTA_TCMALLOC_SUPPORT

class ScopedMemoryProfiler
{
public:
    ScopedMemoryProfiler(MemoryProfiler & profiler,
                         const MemoryProfiler::Phase phase) :
        profiler_(profiler),
        phase_(phase)
    {
        profiler.enteringPhase(phase_);
    }
    ~ScopedMemoryProfiler()
    {
        profiler_.exitingPhase(phase_);
    }
private:
    MemoryProfiler & profiler_;
    const MemoryProfiler::Phase phase_;
};

#define PHASE_PROFILER(profiler, phase) \
    std::unique_ptr<ScopedMemoryProfiler> mem_profiler; \
    if (profiler) { \
        mem_profiler.reset(new ScopedMemoryProfiler(*profiler, phase)); \
    }

#else

#define PHASE_PROFILER(profiler, phase) \
    (void) profiler; \
    (void) phase;

#endif

/*!
 * \brief YAML parser class to turn simulation control definition files:
 *
 *   content:
 *       pause:  'core0.rob.stats.total_number_retired >= 1000'
 *       resume: 'core0.rob.stats.total_number_retired >= 2500'
 *       term:   'core0.rob.stats.total_number_retired >= 9000'
 *       hello:  'notif.my_own_channel1 == 500'
 *       world:  'notif.my_own_channel2 != 404'
 *
 * Into a simple dictionary of keys (pause, resume, ...) and their
 * associated expression strings:
 *
 *   { 'pause',  'core0.rob.stats.total_number_retired >= 1000' },
 *       ...                   ...
 *   { 'hello',  'notif.my_own_channel1 == 500'                 },
 *       ...                   ...
 */
class SimControlFileParserYAML
{
    class SimControlFileEventHandlerYAML : public sparta::YAMLTreeEventHandler
    {
        app::TriggerKeyValues trigger_kv_pairs_;

        /*!
         * \brief Reserved keywords for this parser's dictionary
         */
        static constexpr char KEY_CONTENT[] = "content";
        static constexpr char KEY_PAUSE[]   = "pause";
        static constexpr char KEY_RESUME[]  = "resume";
        static constexpr char KEY_TERM[]    = "term";

        virtual bool handleEnterMap_(
            const std::string & key,
            NavVector & context) override final
        {
            (void) context;

            if (key.empty() || key == KEY_CONTENT) {
                return false;
            }

            throw sparta::SpartaException(
                "Unrecognized keyword being used in a YAML map: '") << key << "'";
            return false;
        }

        virtual void handleLeafScalar_(
            sparta::TreeNode * n,
            const std::string & value,
            const std::string & assoc_key,
            const std::vector<std::string> & captures,
            node_uid_t uid) override final
        {
            (void) n;
            (void) captures;
            (void) uid;

            trigger_kv_pairs_[assoc_key] = value;
        }

        virtual bool isReservedKey_(const std::string & key) const override final
        {
            return !key.empty();
        }

    public:
        SimControlFileEventHandlerYAML(const std::string & def_file,
                                       NavVector device_trees) :
            sparta::YAMLTreeEventHandler(def_file, device_trees, false)
        {
        }

        const app::TriggerKeyValues & getTriggerExpressions() const
        {
            return trigger_kv_pairs_;
        }
    };

public:
    explicit SimControlFileParserYAML(const std::string & def_file) :
        fin_(),
        parser_(),
        def_file_(def_file)
    {
        sparta_assert(boost::filesystem::exists(def_file_),
                    ("File '" + def_file + "' cannot be found"));
        fin_.open(def_file.c_str(), std::ios::in);
        sparta_assert(fin_.is_open());
        parser_.reset(new YP::Parser(fin_));
    }

    explicit SimControlFileParserYAML(std::istream & content) :
        fin_(),
        parser_(new YP::Parser(content)),
        def_file_("<istream>")
    {
    }

    const app::TriggerKeyValues & getTriggerExpressions(
        sparta::TreeNode * context)
    {
        if (evt_handler_ != nullptr) {
            return evt_handler_->getTriggerExpressions();
        }

        std::shared_ptr<sparta::YAMLTreeEventHandler::NavNode> scope(
            new sparta::YAMLTreeEventHandler::NavNode({
                nullptr, context, {}, 0}));

        evt_handler_.reset(new SimControlFileEventHandlerYAML(def_file_, {scope}));
        while (parser_->HandleNextDocument(*((YP::EventHandler*)evt_handler_.get()))) {}

        return evt_handler_->getTriggerExpressions();
    }

private:
    std::ifstream fin_;
    std::unique_ptr<YP::Parser> parser_;
    std::string def_file_;
    std::unique_ptr<SimControlFileEventHandlerYAML> evt_handler_;
};

constexpr char SimControlFileParserYAML::
    SimControlFileEventHandlerYAML::KEY_CONTENT[];
constexpr char SimControlFileParserYAML::
    SimControlFileEventHandlerYAML::KEY_PAUSE[];
constexpr char SimControlFileParserYAML::
    SimControlFileEventHandlerYAML::KEY_RESUME[];
constexpr char SimControlFileParserYAML::
    SimControlFileEventHandlerYAML::KEY_TERM[];

}
}

namespace sparta {
    namespace app {

/*!
 * \brief Helper for printing scheduler information with respect to elapsed time
 */
void printSchedulerPerformanceInfo(std::ostream& o, const boost::timer::cpu_timer &timer, Scheduler * scheduler)
{
    double elapsed_user_seconds   = timer.elapsed().user/1000000000.0;
    const double THOUSAND = 1000;
    //const double MILLION = 1000000;

    if(elapsed_user_seconds != 0) {
        o << "  Simulation Performace      : " << timer.format(4, "wall(%w), system(%s), user(%u)") << std::endl;
        o << "  Scheduler Tick Rate  (KTPS): "
          << scheduler->getCurrentTick()/elapsed_user_seconds/THOUSAND
          << "  (1k ticks per second)" << std::endl;
        o << "  Scheduler Event Rate (KEPS): "
          << scheduler->getNumFired()/elapsed_user_seconds/THOUSAND
          << " KEPS (1k events per second)" << std::endl;
    }
    else {
        o << "  *** Simulation Performance cannot be measured -- no user time detected. "
             "Did the simulator run long enough?" << std::endl;
    }
    o << "  Scheduler Events Fired: "
      << scheduler->getNumFired() << std::endl;
}

simdb::DatabaseRoot * Simulation::getDatabaseRoot() const
{
    return db_root_.get();
}

const DatabaseAccessor * Simulation::getSimulationDatabaseAccessor() const
{
    return sim_db_accessor_.get();
}

Simulation::Simulation(const std::string& sim_name,
                       Scheduler * scheduler) :
    clk_manager_(scheduler),
    sim_name_(sim_name),
    scheduler_(scheduler),
    root_clk_(nullptr),
    root_(this, scheduler->getSearchScope()),
    warn_to_cerr_(sparta::TreeNode::getVirtualGlobalNode(),
                  sparta::log::categories::WARN,
                  std::cerr),
    pevent_start_handler_(SpartaHandler::from_member<Simulation, &Simulation::delayedPeventStart_>
                          (this, "Simulation::delayedPEventStart_")),
    simulation_state_(this)
{

    // Watch for created nodes to which we will apply taps
    root_.getNodeAttachedNotification().REGISTER_FOR_THIS(rootDescendantAdded_);

    // Handle illegal signals.
    // Note: Update documentation if these signals are modified
    backtrace_.setAsHandler(SIGSEGV);
    backtrace_.setAsHandler(SIGFPE);
    backtrace_.setAsHandler(SIGILL);
    backtrace_.setAsHandler(SIGABRT);
    backtrace_.setAsHandler(SIGBUS);

    report_repository_.reset(new sparta::ReportRepository(this));

    // Sanity check - simulations cannot exist without a scheduler
    sparta_assert(scheduler_, "All simulators must be given a non-null scheduler");

    REGISTER_SIMDB_NAMESPACE(Stats, SQLite);
    REGISTER_SIMDB_SCHEMA_BUILDER(Stats, db::buildSimulationDatabaseSchema);

    REGISTER_SIMDB_PROXY_CREATE_FUNCTION(SQLite, []() {
        return new simdb::SQLiteConnProxy;
    });

    REGISTER_SIMDB_PROXY_CREATE_FUNCTION(HDF5, []() {
        return new simdb::HDF5ConnProxy;
    });
}

Simulation::~Simulation()
{
    sparta::SleeperThread::getInstance()->detachScheduler(scheduler_, false);

    // Allow deletion of nodes without error now. This may have been set already
    // by the parent, but calling this function again has no negative effects.
    // This should be done before any simulator subclasses destruct
    try{
        root_.enterTeardown();
    }catch(std::exception& e){
        std::cerr << "Warning: suppressed exception in enterTeardown on root node: "
                  << root_ << ":\n"
                  << e.what() << std::endl;
    }catch(...){
        std::cerr << "Warning: suppressed unknown exception in enterTeardown at "
                  << root_ << std::endl;
    }

    if(meta_){
        try{
            meta_->enterTeardown();
        }catch(std::exception& e){
            std::cerr << "Warning: suppressed exception in enterTeardown on meta node: "
                      << meta_.get() << ":\n"
                      << e.what() << std::endl;
        }catch(...){
            std::cerr << "Warning: suppressed unknown exception in enterTeardown at "
                      << meta_.get() << std::endl;
        }
    }

    if(clk_root_node_){
        clk_root_node_->enterTeardown();
    }

    // Deregister
    root_.getNodeAttachedNotification().DEREGISTER_FOR_THIS(rootDescendantAdded_);

    // ReportRepository's destructor may call back into
    // SimDB objects, such as AsyncTaskController, to do
    // any last-minute data flushes to the database. We
    // should streamline these various classes that need
    // to coordinate post-simulation / post-processing
    // between themselves, and not have to worry about
    // the calling order of their destructors.
    //
    // (Temporary: This is a quick fix for the v1.7.1 release)
    //     - Bug found went like this:
    //        1. Delete the DatabaseRoot
    //        2. Delete the ReportRepository
    //        3. ReportRepository::saveReports() gets implicitly
    //           called from its own destructor
    //        4. Code in RR::saveReports() got a *raw* pointer
    //           to the Simulation's (this) DatabaseRoot, which
    //           by that point had been deleted
    //        5. Use of the dead DatabaseRoot seg faulted
    report_repository_.reset();
    db_root_.reset();
}

void Simulation::configure(const int argc,
                           char ** argv,
                           SimulationConfiguration * configuration,
                           const bool use_pyshell)
{
    (void) argc; // Used by Python shell
    (void) argv; // Used by Python shell
    sparta_assert(configuration != nullptr,
                "You must supply a persistent SimulationConfiguration object");

    sparta_assert(root_.getPhase() == TreeNode::TREE_BUILDING,
                "Cannot re-'configure' sparta::app::Simulation once the tree has been moved out of BUILDING");
    sparta_assert(rep_descs_.size() == 0,
                "Cannot re-'configure' sparta::app::Simulation once a report has been added");

    sim_config_ = configuration;
    print_dag_  = sim_config_->show_dag;

    ReportDescVec expanded_descriptors;
    for (const auto & rd : sim_config_->reports) {
        ReportDescVec one_expanded_descriptor = expandReportDescriptor_(rd);
        expanded_descriptors.insert(expanded_descriptors.end(),
                                    one_expanded_descriptor.begin(),
                                    one_expanded_descriptor.end());
    }
    for (const auto & rd : expanded_descriptors) {
        validateDescriptorCanBeAdded_(rd, use_pyshell);
        rep_descs_.push_back(rd);
    }

    using_final_config_       = sim_config_->hasFinalConfig();

    // This can be set either command line or explicitly.  Either way,
    // if it's at all true, keep it that way.
    validate_post_run_        = sim_config_->validate_post_run;

#ifdef SPARTA_PYTHON_SUPPORT
    // Setup Python if needed, sending only the first argument
    if(use_pyshell) {
        sparta_assert(argv != nullptr && argc > 0);

        pyshell_.reset(new python::PythonInterpreter("sparta python shell", PYTHONHOME, 1, argv));
    }
#endif

    // Now that we've been given our report descriptors that were parsed
    // from yaml, allow access to our report configuration object to let
    // users add more if needed
    report_config_.reset(new ReportConfiguration(sim_config_, &rep_descs_, &root_));

    // Disabling default-warnings tap if applicable
    if(false == sim_config_->warn_stderr){
        warn_to_cerr_.detach(); // Do no observe
    }
    if(!sim_config_->warnings_file.empty()){
        warn_to_file_.reset(new sparta::log::Tap(sparta::TreeNode::getVirtualGlobalNode(),
                                               sparta::log::categories::WARN,
                                               sim_config_->warnings_file));
    }

    // If there are nodes already existing in the tree (e.g. root or "") then
    // there are no notifications for these TreeNodes since they already exist.
    // Install taps immediately instead of through rootDescendantAdded_
    if(sim_config_->trigger_on_type != SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE) {
        if(!sim_config_->getTaps().empty()) {
            log_trigger_.reset(new LoggingTrigger(*this, sim_config_->getTaps()));
        }
    }
    else {
        installTaps(sim_config_->getTaps());
    }

    // Create the meta tree
    meta_.reset(new MetaTreeNode(this, scheduler_->getSearchScope(),
                                 sim_config_->getDefaults().other_meta_params));

    setupProfilers_();
    simulation_state_.configure();
    inspectFeatureValues_();
}

// At the very end of the simulation's configure() method,
// take a first look at the feature values we were given.
void Simulation::inspectFeatureValues_()
{
    const bool db_featured_on = IsFeatureValueEnabled(feature_config_, "simdb");
    const bool needs_db = !db_root_ || !stats_db_ || !sim_db_accessor_;

    if (db_featured_on && needs_db) {
        if (!sim_db_accessor_) {
            sim_db_accessor_.reset(new DatabaseAccessor(&root_));
        }

        if (!db_root_) {
            std::string db_dir = sim_config_->getSimulationDatabaseLocation();
            if (db_dir.empty()) {
                auto tempdir = boost::filesystem::temp_directory_path();
                db_dir = tempdir.string();
            }

            db_root_.reset(new simdb::DatabaseRoot(db_dir));
        }

        if (!stats_db_) {
            auto db_root = getDatabaseRoot();
            sparta_assert(db_root, "Could not find database root");

            auto stats_namespace = db_root->getNamespace("Stats");
            sparta_assert(stats_namespace, "Could not find Stats database namespace");

            auto stats_db = stats_namespace->getDatabase();
            sparta_assert(stats_db, "Could not find Stats database");

            stats_db_ = stats_db->getObjectManager();
            sparta_assert(stats_db_, "Could not get ObjectManager from the Stats database");
        }

        simdb::TableSummaries si_summaries;
        db::configureDatabaseTableSummaries(si_summaries);

        if (isReportValidationEnabled_()) {
            si_summaries.excludeTables(
                "ReportVerificationMetadata",
                "ReportVerificationResults",
                "ReportVerificationFailureSummaries",
                "ReportVerificationDeepCopyFiles");
        }

        //TODO - Table summary configurations should be registered
        //via a macro that looks like this:
        //
        //    REGISTER_SIMDB_SCHEMA_SUMMARY(Stats, [](simdb::Schema & schema) {
        //        simdb::TableSummaries si_summaries;
        //        db::configureDatabaseTableSummaries(si_summaries);
        //        schema.setTableSummaryConfig(std::move(si_summaries));
        //    });
        //
        //Or perhaps like this:
        //
        //    REGISTER_SIMDB_SCHEMA_SUMMARY(
        //        Stats, db::configureDatabaseTableSummaries);
        //
        //Where the second argument is a std::function<void(simdb::TableSummaries&)>
        auto & si_schema = stats_db_->getSchema();
        si_schema.setTableSummaryConfig(std::move(si_summaries));
    }
}

bool Simulation::isReportValidationEnabled_() const
{
    if (stats_db_ == nullptr) {
        return false;
    }

    if (IsFeatureValueEnabled(feature_config_, "simdb-verify")) {
        return true;
    }

    return false;
}

void Simulation::addReport(const ReportDescriptor & rep)
{
    sparta_assert(root_.isFinalized() == false || root_.isFinalizing(),
                      "Cannot add a report to a sparta::app::Simulation after tree enters finalization");

    ReportDescVec expanded_descriptors = expandReportDescriptor_(rep);
    for (const auto & rd : expanded_descriptors) {
#ifdef SPARTA_PYTHON_SUPPORT
        const bool pyshell = pyshell_ != nullptr;
#else
        const bool pyshell = false;
#endif

        validateDescriptorCanBeAdded_(rep, pyshell);

        // Warn that --python-shell users will not be able to access this
        // report descriptor from Python. We need tight control over when
        // to republish the 'report_config' object to Python, and since
        // this method can be called any time before the tree is finalized,
        // we can only safely add it directly to the 'rep_descs_' collection,
        // NOT the 'report_config_' object. Bypassing report_config_ is how
        // we prevent republishing to Python.
        if (pyshell) {
            std::cerr << "Warning: The following report descriptor was added to \n"
                "the simulation through the app::Simulation::addReport() \n"
                "method while using SPARTA's Python shell. This descriptor \n"
                "will be added to the simulation's reports, but will not \n"
                "be accessible from Python.\n\n\t" << rd.stringize() << "\n\n";
        }

        // Simply append to list. Nothing to do until finalization (unlike taps)
        rep_descs_.push_back(rep);
    }
}

void Simulation::installTaps(const log::TapDescVec& taps)
{
    for(const log::TapDescriptor& td : taps){
        std::vector<sparta::TreeNode*> roots;
        TreeNodePrivateAttorney::findChildren(sparta::TreeNode::getVirtualGlobalNode(),
                                              td.getLocation(), roots);
        for(sparta::TreeNode* r : roots){
            attachTapTo_(td, r);
        }

        // Check for matches on any other root nodes here if applicable
        // ...

        // Any patterns of "" should be considered associated with everything
        if(td.getLocation() == "" || td.getLocation() == ReportDescriptor::GLOBAL_KEYWORD){
            attachTapTo_(td, sparta::TreeNode::getVirtualGlobalNode());
        }
    }
}

void Simulation::buildTree()
{
    std::cout << "Building tree..." << std::endl;

    // Create a root for the clocks tree. It should share its search scope with
    // the device tree root.
    clk_root_node_.reset(new sparta::RootTreeNode("clocks",
                                                  "Clock Tree Root",
                                                  root_.getSearchScope()));
    root_clk_ = clk_manager_.makeRoot(clk_root_node_.get());

    root_.setClock(root_clk_.get());

#ifdef SPARTA_PYTHON_SUPPORT
    // Hand control over to the python shell for (optional) manual configuration
    if (pyshell_) {
        pyshell_->publishSimulationConfiguration(sim_config_);
        pyshell_->publishReportConfiguration(getReportConfiguration());
        pyshell_->interact();
        if (pyshell_->getExitCode() != 0) {
            throw SpartaException("Python shell exited with non-zero exit code: ")
                << pyshell_->getExitCode();
        }
        report_config_->finishPythonInteraction_();
    }
#endif

    setupProfilers_();

    // Subclass callback
    {
        PHASE_PROFILER(memory_profiler_, MemoryProfiler::Phase::Build);
        buildTree_();
    }

    report_repository_->finalize();
}

void Simulation::configureTree()
{
    std::cout << "Configuring tree..." << std::endl;

#ifdef SPARTA_PYTHON_SUPPORT
    // Hand control over to the python shell for (optional) Resource node configuration
    if (pyshell_) {
        //! Publish the partially built tree and the simulator so that resource nodes
        //  can be added or removed from the shell
        pyshell_->publishTree(getRoot());
        pyshell_->publishSimulator(this);
        pyshell_->interact();
        if (pyshell_->getExitCode() != 0) {
            throw SpartaException("Python shell exited with non-zero exit code: ")
                << pyshell_->getExitCode();
        }
    }
#endif

    root_.enterConfiguring(); // No more adding ResourceTreeNodes

    // Subclass callback
    {
        PHASE_PROFILER(memory_profiler_, MemoryProfiler::Phase::Configure);
        configureTree_();
    }
}

void Simulation::finalizeTree()
{
    std::cout << "Finalizing tree..." << std::endl;
    sparta_assert(root_clk_ != nullptr, "Root clock was not set up in this simulator");

    // No more ResourceTreeNodes can be created during this.
#ifdef SPARTA_PYTHON_SUPPORT
    root_.enterFinalized(pyshell_.get());
#else
    root_.enterFinalized();
#endif

    // No more TreeNodes added to tree from now on

    // Check to see that all taps have been used in the initial
    // startup
    if(sim_config_ &&
       (sim_config_->trigger_on_type == SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE)) {
        auto unused_taps = log::getUnusedTaps(sim_config_->getTaps());
        if(unused_taps.size() > 0){
            sparta::SpartaException ex("Device tree fully realized but the following tap descriptions were "
                                   "not used because there were no tree locations matching them:\n");
            for(const log::TapDescriptor* td : unused_taps){
                ex << td->stringize() << '\n';
            }
            throw ex;
        }
    }

    // Bind nodes within resources
    root_.bindTreeEarly();

    // Subclass callback
    {
        PHASE_PROFILER(memory_profiler_, MemoryProfiler::Phase::Bind);
        bindTree_();
    }

    // Bind nodes within resources
    root_.bindTreeLate();

    if(sim_config_)
    {
        // Ensure that all unbound parameters have been consumed by ParameterSets or explicitly
        checkAllVirtualParamsRead_(sim_config_->getArchUnboundParameterTree());

        // Ensure that all unbound parameters have been consumed by ParameterSets or explicitly
        checkAllVirtualParamsRead_(sim_config_->getUnboundParameterTree());

        // Ensure that all unbound extension parameters were consumed
        checkAllVirtualParamsRead_(sim_config_->getExtensionsUnboundParameterTree());
    }

    // Check ports and such
    root_.validatePreRun();
}

void Simulation::finalizeFramework()
{
    sparta_assert(root_.isFinalized(),
                "Cannot call app::Simulation::finalizeFramework until finalizeTree completes");

    sparta::SleeperThread::getInstance()->attachScheduler(scheduler_);
    // If we need to, kick off the sleeper thread now.
    sparta::SleeperThread::getInstance()->finalize();

    try {
        // Finalize the scheduler in preparation for running
        scheduler_->finalize();
    }
    catch(sparta::DAG::CycleException & e) {
        std::cerr << SPARTA_CMDLINE_COLOR_ERROR "\n\nError: Cycle detected during DAG contruction. "
            "Generated cycle_detection.dot file for examination" SPARTA_CMDLINE_COLOR_NORMAL
                  << std::endl;
        std::ofstream cd("cycle_detection.dot");
        e.writeCycleAsDOT(cd);
        std::cerr << "DOT file generated: cycle_detection.dot Textual version: " << std::endl;
        e.writeCycleAsText(std::cerr);
        throw;
    }
    catch(...) {
        this->dumpDebugContentIfAllowed(std::current_exception());
        throw;
    }

    if(print_dag_) {
        scheduler_->getDAG()->print(std::cout);
    }

    // Enable trigger-based logging taps
    if(sim_config_ &&
       !sim_config_->getTaps().empty() &&
       (sim_config_->trigger_on_type != SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE))
    {
        sparta_assert(log_trigger_,
                    "A logging trigger should have been constructed if debug trigger was set");
        debug_trigger_.reset(new trigger::Trigger("debug_on_trigger", getRootClock()));
        debug_trigger_->addTriggeredObject(log_trigger_.get());

        // Set up trigger. This must be done AFTER finalization so that events can be scheduled
        if(sim_config_->trigger_on_type == SimulationConfiguration::TriggerSource::TRIGGER_ON_CYCLE)
        {
            sparta::Clock * trigger_clk = getRootClock();
            if(!sim_config_->trigger_clock.empty()) {
                // Find the given clock
                std::vector<TreeNode*> results;
                trigger_clk->findChildren(sim_config_->trigger_clock, results);
                if(results.empty()) {
                    throw SpartaException("Cannot find clock '" + sim_config_->trigger_clock + "' for debug-on");
                }
                if(results.size() > 1) {
                    throw SpartaException("Found multiple clocks named '" + sim_config_->trigger_clock + "' for debug-on; please be more specific");
                }
                trigger_clk = dynamic_cast<sparta::Clock *>(results[0]);
                sparta_assert(trigger_clk != nullptr);
            }
            debug_trigger_->setTriggerStartAbsolute(trigger_clk, sim_config_->trigger_on_value);
        }
        else if(sim_config_->trigger_on_type == SimulationConfiguration::TriggerSource::TRIGGER_ON_INSTRUCTION)
        {
            const CounterBase* ictr = findSemanticCounter(Simulation::CSEM_INSTRUCTIONS);
            if(!ictr){
                throw SpartaException("Cannot proceed with a A debug trigger based on "
                                    "instructions because this simulator does not provide a "
                                    "counter with an instruction-count semantic. Simulator "
                                    "must implement: "
                                    "sparta::app::Simulation::findSemanticCounter(CSEM_INSTRUCTIONS)");
            }
            debug_trigger_->setTriggerStartAbsolute(ictr, sim_config_->trigger_on_value);
        }
        else if(sim_config_->trigger_on_type != SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE)
        {
            throw SpartaException("Unknown trigger_on_type: ") << static_cast<uint32_t>(sim_config_->trigger_on_type)
                                                             << " detected during debug trigger initialization";
        }
    }

    this->setupControllerTriggers_();

    // Set up reports.  This must happen after the DAG is finalized so
    // that the report startup trigger can be scheduled
    this->setupReports_();

    framework_finalized_ = true;
}

void Simulation::run(uint64_t run_time)
{
    dumpMetaParameterTable(std::cout);
    //auto num_non_defaults = dumpNonDefaultParameters_(&root_, std::cout);
    auto num_non_defaults = countNonDefaultParameters_(&root_);
    std::cout << "Non-default model parameters: " << num_non_defaults << std::endl;

    std::cout << "Running..." << std::endl;

    if(false == framework_finalized_){
        throw sparta::SpartaException("Cannot run the simulation until the framework is finalized. "
                                  "See Simulation::finalizeFramework");
    }

    // Create any SimDB triggers the simulation was configured to use
    this->setupDatabaseTriggers_();

    // Setup Pevent instruction warmup
    if (pevent_warmup_icount_ > 0) {
        // We are waiting, so we must setup a trigger.
        const CounterBase * ictr = findSemanticCounter(CSEM_INSTRUCTIONS);
        if (!ictr) {
            throw SpartaException("Cannot proceed with a report warmup instruction count > 0 because "
                                "this simulator does not provide a counter with an "
                                "instruction-count semantic. Simulator must implement: "
                                "sparta::app::Simulation::findSemanticCounter(CSEM_INSTRUCTIONS)");
        }

        pevent_start_trigger_.reset(new trigger::CounterTrigger(
            "SimulationPeventStartup",
            pevent_start_handler_,
            ictr,
            pevent_warmup_icount_));

    }

    std::exception_ptr eptr;
    try{
        //! \todo Time actual run time by moving this to run controller instead of here
        //! \todo Time running per thread
        boost::timer::cpu_timer timer;
        timer.start();
        try{
            // Actually run the simulation (or allow it to be controlled)
            PHASE_PROFILER(memory_profiler_, MemoryProfiler::Phase::Simulate);
            runRaw_(run_time);
        } catch(const simdb::DatabaseInterrupt &) {
            // Nothing to do. These are not "real" exceptions - nothing
            // bad happened, there was just a request to stop database
            // work from continuing. Don't add this exception info to
            // the 'eptr' variable.
        } catch (...) {
            eptr = std::current_exception();
        }
        timer.stop();

        if(eptr == std::exception_ptr()){
            std::cout << "Running Complete\n";
            // Show simulator performance
            printSchedulerPerformanceInfo(std::cout, timer, scheduler_);
        }else{
            std::cerr << SPARTA_CMDLINE_COLOR_ERROR "Exception while running" SPARTA_CMDLINE_COLOR_NORMAL
                      << std::endl;
            try {
                std::rethrow_exception(eptr);
            }
            catch(const std::exception &e) {
                std::cerr << e.what() << std::endl;
                eptr =  std::current_exception();
            }
        }

        // Rethrow exception if necessary
        if(eptr != std::exception_ptr()){
            simulation_successful_ = false;
            std::rethrow_exception(eptr);
        }

        // Validate simulation state. There is no exception at this point
        if(validate_post_run_){
            std::cout << "Validating post-run..." << std::endl;
            try{
                root_.validatePostRun();
            }catch(...){
                std::cerr << SPARTA_CMDLINE_COLOR_ERROR "  Exception while validating post-run "
                          "simulation state. To disable this test, do not set --validate-post-run"
                          SPARTA_CMDLINE_COLOR_NORMAL
                          << std::endl;
                throw;
            }
        }

        std::cout << SPARTA_CMDLINE_COLOR_GOOD "Run Successful!" SPARTA_CMDLINE_COLOR_NORMAL "\n";
    }catch(...){
        eptr = std::current_exception(); // capture
    }

    if(eptr == std::exception_ptr()) {
        // Indicate to the root and its components that simulation has
        // terminated
        root_.simulationTerminating();
    }

    // If no sim_config, assume false
    const bool report_on_error = (sim_config_ && sim_config_->report_on_error);
    if(eptr == std::exception_ptr() || report_on_error){
        // Write reports
        saveReports();
    }

    if(eptr == std::exception_ptr()){
        // Dump debug if there was no error and the policy is to always dump.
        // otherwise the dump will be done by an external exception handler
        dumpDebugContentIfAllowed(eptr, false);
    }else{
        std::rethrow_exception(eptr);
    }
}

void Simulation::dumpDebugContentIfAllowed(std::exception_ptr eptr, bool force) noexcept
{
    // Assume a DEBUG_DUMP_ERROR if no sim_config
    const SimulationConfiguration::PostRunDebugDumpPolicy debug_dump_policy =
        (sim_config_ ? sim_config_->debug_dump_policy : SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_ERROR);
    const SimulationConfiguration::PostRunDebugDumpOptions debug_dump_opts =
        (sim_config_ ? sim_config_->debug_dump_options :
         SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_EVERYTHING);
    std::string filename_out = (sim_config_ ? sim_config_->dump_debug_filename : "");

    if (force
        || (debug_dump_policy == SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_ALWAYS)
        || (eptr != std::exception_ptr()
            && (debug_dump_policy == SimulationConfiguration::PostRunDebugDumpPolicy::DEBUG_DUMP_ERROR)))
    {
        std::cerr << "  [out] Writing error dump file '" << filename_out << "'" << std::endl;
        // Get exception filename
        std::string exception;
        std::string backtrace;
        if(eptr != std::exception_ptr()){
            try{
                if(eptr != std::exception_ptr()){
                    std::rethrow_exception(eptr);
                }
            }catch(const SpartaException& e){
                exception = e.what();
                if (debug_dump_opts != SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_NOTHING) {
                    backtrace = e.backtrace(); // Sparta exceptions generate backtraces when thrown
                }
                eptr = std::current_exception(); // re-capture
            }catch(const std::exception& e){
                exception = e.what();
                eptr = std::current_exception(); // re-capture
            }catch(...){
                exception = "Unknown Exception";
                eptr = std::current_exception(); // re-capture
            }
        }else{
            exception = "Exiting without exception";
        }
        bool success = dumpDebugContent_(filename_out, exception, backtrace);
        std::cerr << "  [out] Debug state written to ";
        if(filename_out != ""){
            std::cerr << "\"" << filename_out << "\"";
        }else{
            std::cerr << "stderr";
        }
        std::cerr << std::endl;
        if(false == success){
            std::cerr << SPARTA_CMDLINE_COLOR_WARNING "Warning: Exception while writing debug state. Output "
                         "may be incomplete" SPARTA_CMDLINE_COLOR_NORMAL << std::endl;
        }
    }
}

void Simulation::runControlLoop_(uint64_t cmdline_run_time)
{
#ifdef SPARTA_PYTHON_SUPPORT
    // TEMP: test out python shell
    if(pyshell_) {
        //! \todo Refactor things heavily so that the shell actually controls
        //! phases of setup in some circumstances
        pyshell_->publishTree(getRoot());
        pyshell_->publishSimulator(this);
        rc_.reset(new control::TemporaryRunControl(this, getScheduler()));
        pyshell_->publishRunController(rc_.get());
        setupStreamControllers_();

        // Pass control to python
        //! \todo Consider passing command line run time to kick-off sim-running automatically
        pyshell_->interact();

        if(pyshell_->getExitCode() != 0){
            throw SpartaException("Python shell exited with non-zero exit code: ")
                << pyshell_->getExitCode();
        }
        return;
    }
#endif
    Simulation::runRaw(cmdline_run_time);
}

void Simulation::runRaw_(uint64_t run_time)
{
    runControlLoop_(run_time);
}

void Simulation::runRaw(uint64_t run_time)
{
    scheduler_->run(run_time,
                    sim_config_->scheduler_exacting_run,
                    sim_config_->scheduler_measure_run_time);
}

void Simulation::asyncStop()
{
    scheduler_->stopRunning();
}

void Simulation::saveReports()
{
    std::cout << "Saving reports..." << std::endl;

    db::ReportVerifier formatted_reports_to_verify;

    // Print summary report when there is no exception
    if(auto_summary_report_){
        sparta::report::format::Text summary_fmt(auto_summary_report_.get());
        summary_fmt.setValueColumn(summary_fmt.getRightmostNameColumn());
        summary_fmt.setReportPrefix("");
        summary_fmt.setQuoteReportNames(false);
        summary_fmt.setWriteContentlessReports(false);
        summary_fmt.setShowSimInfo(false); // No need to summarize the simulator here
        //summary_fmt.setIndentSubreports(false);
        const SimulationConfiguration::AutoSummaryState auto_summary_state =
            (sim_config_ ? sim_config_->auto_summary_state : SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_NORMAL);
        if(auto_summary_state == SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_VERBOSE){
            summary_fmt.setShowDescriptions(true);
        }
        std::cout << summary_fmt << std::endl;;

        if (stats_db_) {
            std::cout << "  [simdb] Validation of database-generated "
                         "auto summary is currently unavailable\n";
        }
    }

    std::map<std::string, std::shared_ptr<report::format::BaseFormatter>> formatters_in_use;

    auto save_reports = [&]() {
        //Write out the SimulationInfo global data to the database.
        //This table holds metadata that is common to all reports
        //that came from this simulation.
        if (stats_db_ != nullptr) {
            formatters_in_use = report_repository_->getFormattersByFilename();
            db::SimInfoSerializer serializer(SimulationInfo::getInstance());
            serializer.serialize(*stats_db_);
            std::cout << "SimDB file is: " << stats_db_->getDatabaseFile() << std::endl;
        }

        auto saved_reports = report_repository_->saveReports();
        (void) saved_reports;
    };

    if (stats_db_ != nullptr) {
        stats_db_->safeTransaction([&]() {
            save_reports();
            stats_db_->captureTableSummaries();
        });
    } else {
        save_reports();
    }

    simdb::AsyncTaskEval * db_thread =
        stats_db_ ? stats_db_->getTaskQueue() : nullptr;

#ifdef SPARTA_PYTHON_SUPPORT
    if (pyshell_) {
        //Remove the DB worker variable from the Python namespace.
        //This allows the last Python interaction to access the database
        //as much as it wants without incurring useless synchronization
        //and queue flushes.
        if (db_thread) {
            pyshell_->removePublishedObject(db_thread);
        }

        std::cout << "\n* * * Simulation is finished. Type 'quit' to exit "
                     "the Python shell. * * *\n" << std::endl;
        pyshell_->interact();
    }
#endif

    //Reports have all been saved to disk / database / etc.
    //Stop the database consumer thread and delete it now.
    if (db_thread != nullptr) {
        db_thread->stopThread();
    }

#ifdef SPARTA_TCMALLOC_SUPPORT
    if (memory_profiler_) {
        memory_profiler_->saveReport();
    }
#endif

    if (isReportValidationEnabled_()) {
        stats_db_->safeTransaction([&]() {
            for (const auto & rd : rep_descs_) {
                if (rd.isEnabled()) {
                    formatted_reports_to_verify.addReportToVerify(rd);
                }
            }

            for (const auto & formatter : formatters_in_use) {
                formatted_reports_to_verify.addBaseFormatterForPreVerificationReset(
                    formatter.first, formatter.second.get());
            }

            auto verification_summary =
                formatted_reports_to_verify.verifyAll(*stats_db_,
                                                      scheduler_);

            if (verification_summary->hasSummary()) {
                verification_summary->serializeSummary(*stats_db_);
            }

            namespace bfs = boost::filesystem;
            std::map<std::string, std::string> final_dest_files =
                verification_summary->getFinalDestFiles();

            //Store some basic info about any failures that occurred while
            //checking these report files. We'll be given a chance to error
            //out or produce a warning shortly.
            report_verif_failed_fnames_ =
                verification_summary->getFailingReportFilenames();

            //Copy any report files that were renamed or moved somewhere
            //inside the verification directory. The SPARTA simulator is
            //still expecting the report files where their yaml file
            //specified.
            for (const auto & to_copy : final_dest_files) {
                const std::string & simdb_dest_file = to_copy.first;
                const std::string & orig_yaml_dest_file = to_copy.second;
                if (bfs::exists(simdb_dest_file)) {
                    try {
                        if (bfs::exists(orig_yaml_dest_file)) {
                            bfs::remove(orig_yaml_dest_file);
                        }
                        bfs::copy_file(simdb_dest_file, orig_yaml_dest_file);
                    } catch (const std::exception & ex) {
                        std::cout << "  [simdb] Unable to copy report file '"
                                  << simdb_dest_file << "' to '"
                                  << orig_yaml_dest_file << "'"
                                  << std::endl;
                    }
                }
            }
        });
    }
}

void Simulation::postProcessingLastCall()
{
    std::ostringstream oss;

    if (!report_verif_failed_fnames_.empty()) {
        //The report filename might have been something like:
        //   AccuracyCheckedDBs/abcd-1234/out.json
        //
        //Let's turn these strings into something like this:
        //   AccuracyCheckedDBs/abcd-1234.db/out.json
        //
        //Which is shorthand for "database file abcd-1234.db, found
        //in the AccuracyCheckedDBs subfolder, failed to verify the
        //contents of dest_file out.json"
        auto make_err_string_from_report_fname = [](const std::string & fname) {
            auto slash = fname.find_last_of("/");
            if (slash == std::string::npos) {
                return fname;
            }

            std::string err = "'" + fname.substr(0, slash);
            err += ".db" + fname.substr(slash) + "'";
            return err;
        };

        oss << "Simulation failed to verify reports: ";
        if (report_verif_failed_fnames_.size() == 1) {
            oss << make_err_string_from_report_fname(
                       *report_verif_failed_fnames_.begin());
        } else {
            size_t idx = 0;
            auto iter = report_verif_failed_fnames_.begin();
            while (idx < report_verif_failed_fnames_.size() - 1) {
                oss << make_err_string_from_report_fname(*iter) << ", ";
                ++iter;
                ++idx;
            }
            oss << make_err_string_from_report_fname(*iter);
        }
        oss << "\n";
    }

    //Make sure the ReportDescriptor report files all ended
    //up in their *original* dest_file locations. We may have
    //overwritten their dest_file values, but we should have
    //put those report files back to where the outside world
    //(production SPARTA simulators, sparta_core_example tests)
    //expected to find them.
    if (isReportValidationEnabled_()) {
        for (const auto & rd : rep_descs_) {
            if (rd.isEnabled()) {
                std::string orig_dest_file = rd.getDescriptorOrigDestFile();
                if (!boost::filesystem::exists(orig_dest_file)) {
                    auto not_slash = orig_dest_file.find_first_not_of("/");
                    if (not_slash != std::string::npos) {
                        orig_dest_file = orig_dest_file.substr(not_slash);
                    }

                    if (!boost::filesystem::exists(orig_dest_file)) {
                        oss << "A report descriptor's dest_file was not "
                            << "found at the end of a SimDB verification-enabled simulation "
                            << "('" << orig_dest_file << "')\n";
                    }
                }
            }
        }
    }

    const std::string last_call_exception_str = oss.str();
    if (!last_call_exception_str.empty()) {
        throw SpartaException(last_call_exception_str);
    }

    if (sim_config_ && stats_db_) {
        namespace bfs = boost::filesystem;
        const std::string dest_path_str1 = sim_config_->getLegacyReportsCopyDir();
        if (!dest_path_str1.empty()) {
            const std::string dest_path_str2 =
                bfs::path(stats_db_->getDatabaseFile()).stem().string();

            const std::set<std::string> & collected_formats =
                sim_config_->getLegacyReportsCollectedFormats();

            if (!dest_path_str1.empty() && !dest_path_str2.empty()) {
                for (const auto & rd : rep_descs_) {
                    if (!rd.isEnabled()) {
                        continue;
                    }

                    if (!collected_formats.empty()) {
                        utils::lowercase_string lower_format = rd.format;
                        if (collected_formats.find(lower_format.getString()) ==
                            collected_formats.end())
                        {
                            continue;
                        }
                    }

                    const std::string dest_path_str3 = rd.format;
                    if (dest_path_str3.empty()) {
                        continue;
                    }

                    bfs::path dest_path = bfs::path(dest_path_str1 + "/" +
                                                    dest_path_str2 + "/" +
                                                    dest_path_str3);

                    if (!bfs::exists(dest_path)) {
                        try {
                            bfs::create_directories(dest_path);
                        } catch (const std::exception & ex) {
                            std::cout << "  [simdb] Error occurred while collecting "
                                      << "legacy reports: '" << ex.what() << "'"
                                      << std::endl;
                            continue;
                        } catch (...) {
                        }
                    } else if (!bfs::is_directory(dest_path)) {
                        throw SpartaException("Path exists, but is not a directory: '")
                            << dest_path.string() << "'";
                    }

                    std::string orig_dest_file = rd.getDescriptorOrigDestFile();
                    if (!bfs::exists(orig_dest_file)) {
                        auto not_slash = orig_dest_file.find_first_not_of("/");
                        if (not_slash != std::string::npos) {
                            orig_dest_file = orig_dest_file.substr(not_slash);
                        }
                    }

                    if (orig_dest_file == "1") {
                        continue;
                    }

                    if (bfs::exists(orig_dest_file)) {
                        const std::string src_file = orig_dest_file;
                        const std::string dest_file =
                            dest_path_str1 + "/" +
                            dest_path_str2 + "/" +
                            dest_path_str3 + "/" +
                            orig_dest_file;

                        try {
                            bfs::copy_file(src_file, dest_file);
                        } catch (const std::exception & ex) {
                            std::cout << "  [simdb] Error occurred while collecting "
                                      << "legacy reports: '" << ex.what() << "'"
                                      << std::endl;
                            continue;
                        } catch (...) {
                        }
                    }
                }
            }
        }
    }

    if (stats_db_ && root_clk_) {
        root_clk_->serializeTo(*stats_db_);
    }
}

void Simulation::dumpMetaParameterTable(std::ostream& out) const
{
    if(!meta_){
        return;
    }

    std::cout << "Meta-Parameters:" << std::endl;

    ParameterSet* pset = meta_->getChildAs<sparta::ParameterSet>("params");
    for(const TreeNode* n : pset->getChildren()){
        const ParameterBase* pb = dynamic_cast<const ParameterBase*>(n);
        if(pb){
            out << "  " << pb->getName() << ": " << pb->getValueAsString() << std::endl;
        }
    }
}

uint32_t Simulation::countNonDefaultParameters_(TreeNode* root)
{
    sparta_assert(root);
    std::vector<TreeNode*> nodes;
    root->findChildrenByTag(ParameterBase::PARAMETER_NODE_TAG,
                            nodes);

    uint32_t non_defaults = 0;
    for(TreeNode* n : nodes){
        ParameterBase* p = dynamic_cast<ParameterBase*>(n);
        sparta_assert(p,
                    "Found node tagged as a parameter that was not a ParameterBase: " << n->getLocation());
        non_defaults += (uint32_t)!p->isDefault();
    }

    return non_defaults;
}

uint32_t Simulation::dumpNonDefaultParameters_(TreeNode* root, std::ostream& out)
{
    sparta_assert(root);
    std::vector<TreeNode*> nodes;
    root->findChildrenByTag(ParameterBase::PARAMETER_NODE_TAG,
                            nodes);

    std::cout << "Non-Default Parameters below " << root->getLocation() << std::endl;
    uint32_t non_defaults = 0;
    for(TreeNode* n : nodes){
        ParameterBase* p = dynamic_cast<ParameterBase*>(n);
        sparta_assert(p,
                    "Found node tagged as a parameter that was not a ParameterBase: " << n->getLocation());
        if(!p->isDefault()){
            non_defaults += 1;
            out << "  " << p->getLocation() << ":" << p->getValueAsString()
                << " (default: " << p->getDefaultAsString() << ")" << std::endl;
        }
    }

    return non_defaults;
}

uint32_t Simulation::reapplyVolatileParameters_(TreeNode* root)
{
    sparta_assert(root);

    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << "ReapplyVolatileParameters at:" << root->getLocation();
    }

    std::vector<TreeNode*> nodes;
    root->findChildrenByTag(ParameterSet::PARAMETER_SET_NODE_TAG,
                            nodes);

    uint32_t updates = 0;
    for(TreeNode* n : nodes){
        ParameterSet* pset = dynamic_cast<ParameterSet*>(n);
        sparta_assert(pset,
                    "Found node tagged as a parameter set that was not a ParameterSet: " << n->getLocation());
        updates += pset->readVolatileParamValuesFromVirtualTree();
    }

    return updates;
}

uint32_t Simulation::reapplyAllParameters_(TreeNode* root)
{
    sparta_assert(root);

    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << "ReapplyAllParameters at:" << root->getLocation();
    }

    // Filter configs by this node
    ConfigApplicator::ApplyFilter filter(ConfigApplicator::LocationFilter::AT_OR_BELOW_NODE, root);

    for(auto& cfg : user_configs_){
        //std::cout << "  [in] Configuration: " << cfg->stringize() << std::endl;
        // Apply to global so params can begin with top (e.g. "top.clusterX.coreX")
        // Ignore failures

        // Configurations should already be applied from unbound trees as params are created
        // Passing in a final config as an input config will demonstrate this since
        // any input config values that the simulation does not consume will cause errors
        cfg->tryApply(root_.getSearchScope(),
                      ConfigApplicator::ApplySuccessCondition::ASC_IGNORE, filter, sim_config_->verbose_cfg);
    }
    return 0;
}

void Simulation::addTreeNodeExtensionFactory_(const std::string & extension_name,
                                              std::function<TreeNode::ExtensionsBase*()> factory)
{
    if (tree_node_extension_factories_.find(extension_name) == tree_node_extension_factories_.end()) {
        tree_node_extension_factories_.emplace(extension_name, factory);
    }
    getRoot()->addExtensionFactory(extension_name, factory);
}

bool Simulation::dumpDebugContent_(std::string& debug_filename,
                                  const std::string& error_reason,
                                  const std::string& bt) noexcept
{
    // Open the debug dump file. If that fails, use cerr
    std::ostream* out = &std::cerr; // Output stream pointer
    std::ofstream out_file;
    try{
        out_file.open(debug_filename);
        out = &out_file;
    }catch(...){
        debug_filename = ""; // implies stderr
        std::cerr << SPARTA_CMDLINE_COLOR_WARNING "Warning: Failed to open debug dump file \""
                  << debug_filename
                  << "\". Debug state will be written to stderr instead" SPARTA_CMDLINE_COLOR_NORMAL
                  << std::endl;
    }

    // Write simulation info
    sparta::SimulationInfo::getInstance().write(*out, "", "\n");
    *out << std::endl;

    const SimulationConfiguration::PostRunDebugDumpOptions debug_dump_opts =
        (sim_config_ ? sim_config_->debug_dump_options :
         SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_EVERYTHING);

    // Write exception
    *out << "\nError:\n";
    *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
    *out << error_reason << std::endl;
    *out << "\n";

    *out << "\nBacktrace:\n";
    *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
    if(bt.size() != 0){
        *out << bt << std::endl;
    }else{
        if (debug_dump_opts != SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_NOTHING) {
            *out << "<No backtrace available. Exception may not have been a SpartaException>" << std::endl;
        } else {
            *out << "<Backtrace was explicitly disabled for error dumping>" << std::endl;
        }
    }
    *out << "\n";

#if 0 // Let's not do this anymore -- it's getting pretty darn big
    *out << "\nParameters:\n";
    *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
    auto filter_parameters = [](const TreeNode* n) -> bool {
        return dynamic_cast<const ParameterBase*>(n) != nullptr;
    };
    *out << getRoot()->renderSubtree(-1,                 // max_depth (unlimited)
                                     true,               // show_builtins
                                     false,              // names_only
                                     true,               // hide_hidden
                                     filter_parameters);
#endif

    // Write scheduler info
    *out << "\nScheduler:  " << std::endl;
    *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
    *out << "Current Tick:  " << scheduler_->getCurrentTick() << std::endl;
    *out << "Num Fired:     " << scheduler_->getNumFired() << std::endl;
    *out << "Current Phase: " << scheduler_->getCurrentSchedulingPhase() << std::endl;
    *out << "Current Event: " << (scheduler_->getCurrentFiringEvent() ?
                                  scheduler_->getCurrentFiringEvent()->getLabel() :
                                  "<null>") << std::endl;
    *out << "\n";

    if (debug_dump_opts == SimulationConfiguration::PostRunDebugDumpOptions::DEBUG_DUMP_EVERYTHING) {
        try{
            scheduler_->printNextCycleEventTree(*out,
                                                               0, // Current Group (show all)
                                                               scheduler_->getCurrentFiringEventIdx(),
                                                               0); // future=0
            if(scheduler_->getNextContinuingEventTime() != scheduler_->getCurrentTick()) {
                *out << "\nScheduler's Last Scheduled Continuing Event: "
                     << scheduler_->getNextContinuingEventTime() << std::endl;
                        scheduler_->printNextCycleEventTree(*out,
                                                                           0, // Current Group (show all)
                                                                           scheduler_->getCurrentFiringEventIdx(),
                                                                           scheduler_->getNextContinuingEventTime() -
                                                                           scheduler_->getCurrentTick()); // future
            }
        }catch(...){
            *out << "ERROR: exception while printing scheduler next-cycle event tree" << std::endl;
        }
    }
    else {
        *out << "<Scheduler event tree was explicitly disabled for error dumping>" << std::endl;
    }
    *out << std::endl;

#if 0 // Let's not do this anymore -- it's getting pretty darn big
    // Dump DAG
    *out << "\nDAG:  " << std::endl;
    *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
    try{
        scheduler_->getDAG()->print(*out);
    }catch(...){
        *out << "ERROR: exception while printing DAG" << std::endl;
    }
    *out << std::endl;
#endif

    // Dump content, catch any errors
    bool error = true;
    std::stringstream err_str;
    try{
#if 0 // Let's not do this anymore -- it's getting pretty darn big
        *out << "Device tree:\n";
        *out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
        *out << root_.getSearchScope()->renderSubtree(-1, // No depth limit
                                                      true,// Show builtins
                                                      false, // Do not limit output to names-only
                                                      false); // Do not hide hidden

        *out << "\n";
#endif
        root_.dumpDebugContent(*out);
        error = false;
    }catch(std::exception& e){
        err_str << "Warning: suppressed exception in dumpDebugContent on root node: "
                << root_ << ":\n"
                << e.what() << std::endl;
    }catch(...){
        err_str << "Warning: suppressed unknown exception in dumpDebugContent at "
                << root_
                << std::endl;
    }

    // Print error/wraning to stderr and debug dump (if different)
    if(error){
        std::cerr << err_str.str();
        if(out != &std::cerr){
            *out << err_str.str();
        }
    }

    if(out != &std::cerr){
        *out << "\n" << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
        *out << debug_filename << " EOF\n";
    }

    return !error;
}

void Simulation::setupControllerTriggers_()
{
    if (!sim_config_) {
        return;
    }

    if (!sim_config_->getControlFiles().empty() && controller_ == nullptr) {
        throw SpartaException(
            "A control file was supplied at the command prompt (--control <file>) "
            "but no controller instance was given to sparta::app::Simulation::"
            "setSimulationController_()");
    }

    if (controller_ == nullptr) {
        return;
    }

    using trigger_kvp = app::TriggerKeyValues;

    trigger_kvp kv_pairs;
    auto merge_kv = [&kv_pairs](const trigger_kvp & merge_with) {
        for (const auto & kvp : merge_with) {
            if (kv_pairs.find(kvp.first) != kv_pairs.end()) {
                throw sparta::SpartaException(
                    "Duplicate trigger event found (") << kvp.first << ")";
            }
            kv_pairs[kvp.first] = kvp.second;
        }
    };

    std::string control_filename;
    for (const auto & fname : sim_config_->getControlFiles()) {
        SimControlFileParserYAML yaml(fname);
        const trigger_kvp expressions = yaml.getTriggerExpressions(getRoot());
        merge_kv(expressions);
    }

    auto get_expression = [&kv_pairs](const std::string & key,
                                      std::string & expression) -> bool {
        auto iter = kv_pairs.find(key);
        if (iter != kv_pairs.end()) {
            expression = iter->second;
            kv_pairs.erase(iter);
            return true;
        }
        return false;
    };

    std::string pause_expression;
    std::string resume_expression;
    std::string terminate_expression;

    if (get_expression("pause", pause_expression)) {
        controller_triggers_.emplace_back(new trigger::ExpressionTrigger(
            "SimulationPause",
            CREATE_SPARTA_HANDLER(Simulation, pause_),
            pause_expression,
            getRoot(),
            nullptr));
    }

    if (get_expression("resume", resume_expression)) {
        controller_triggers_.emplace_back(new trigger::ExpressionTrigger(
            "SimulationResume",
            CREATE_SPARTA_HANDLER(Simulation, resume_),
            resume_expression,
            getRoot(),
            nullptr));
    }

    if (get_expression("term", terminate_expression)) {
        controller_triggers_.emplace_back(new trigger::ExpressionTrigger(
            "SimulationTerminate",
            CREATE_SPARTA_HANDLER(Simulation, terminate_),
            terminate_expression,
            getRoot(),
            nullptr));
    }

    trigger::ExpressionTrigger::StringPayloadTrigCallback cb = std::bind(
        &Simulation::customEvent_, this, std::placeholders::_1);

    for (const auto & trigger_kvp : kv_pairs) {
        std::unique_ptr<trigger::ExpressionTrigger> trigger(new trigger::ExpressionTrigger(
            "SimulationCustomEvent",
            CREATE_SPARTA_HANDLER_WITH_DATA(Simulation, customEvent_, std::string),
            trigger_kvp.second,
            getRoot(),
            nullptr));

        trigger->switchToStringPayloadCallback(cb, trigger_kvp.first);
        controller_triggers_.emplace_back(trigger.release());
    }

    controller_->sim_status_ = SimulationController::SimulationStatus::Simulating;
}

void Simulation::validateDescriptorCanBeAdded_(
    const ReportDescriptor & rd,
    const bool using_pyshell) const
{
    if (rep_descs_.contains(rd.dest_file) && rd.dest_file != "1") {
        throw SpartaException(
            "You may not configure multiple reports to have "
            "the same dest_file ('") << rd.dest_file << "')";
    } else if (using_pyshell && rd.dest_file == "1") {
        throw SpartaException(
            "Specifying stdout as a report dest_file ('1') is "
            "currently not supported when using --python-shell");
    }
}

void Simulation::setupReports_()
{
    validateReportDescriptors_(rep_descs_);

    //If the SimDB report validation post-processing step is
    //enabled, let's use this simulator's unique database filename
    //to generate a temporary subfolder we can put our report files
    //in. We'll move those files to their original intended location
    //at the end of simulation, after we have performed the validation.
    namespace bfs = boost::filesystem;
    std::string dest_file_subfolder;
    if (isReportValidationEnabled_()) {
        dest_file_subfolder = db::ReportVerifier::getVerifResultsDir();
        dest_file_subfolder += "/";

        bfs::path path = stats_db_->getDatabaseFile();
        dest_file_subfolder += path.filename().string();

        auto dot = dest_file_subfolder.find_last_of(".");
        sparta_assert(dot != std::string::npos);
        sparta_assert(dest_file_subfolder.substr(dot) == ".db");
        dest_file_subfolder = dest_file_subfolder.substr(0, dot);
    }

    // Set up reports now that the entire device tree is finalized
    for(auto& rd : rep_descs_){
        //Report descriptors may have been disabled from Python during
        //the report configuration stage. Skip over them if so.
        if (!rd.isEnabled()) {
            continue;
        }
        std::vector<sparta::TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        if(rd.loc_pattern == ReportDescriptor::GLOBAL_KEYWORD){
            roots.push_back(root_.getSearchScope());
            replacements.push_back({});
        }else{
            root_.getSearchScope()->findChildren(rd.loc_pattern,
                                                 roots,
                                                 replacements);
        }

        //When using SimDB, point the report files to a new subfolder
        //under the verification subdirectory in the current pwd, so
        //we get a directory structure like this:
        //
        //   ./<verif_dir>
        //       abc-123/
        //           foo.csv
        //           bar.json
        //       def-456/
        //           foo.csv
        //           bar.json
        //           baz.txt
        //
        //This allows us to have deterministic post-simulation report
        //verification by putting 'this' simulation's report dest_file's
        //in a subfolder that does not clash with other concurrently
        //running simulations.
        if (rd.dest_file != "1" && isReportValidationEnabled_()) {
            auto first_char = rd.dest_file.find_first_not_of("/");
            if (first_char != std::string::npos) {
                rd.dest_file = rd.dest_file.substr(first_char);
            }
            rd.dest_file =  dest_file_subfolder + "/" + rd.dest_file;

            //A descriptor's dest_file can be given as something like:
            //     def_file:  simple_stats.yaml
            //     dest_file: /tmp/out.json
            //     format:    json_reduced
            //
            //We need to create the subfolder /tmp inside the verif
            //subfolder when this is the case, or std::ofstream
            //will throw.
            auto last_slash = rd.dest_file.find_last_of("/");
            sparta_assert(last_slash != std::string::npos);
            const std::string rd_dest_dir = rd.dest_file.substr(0, last_slash);

            try {
                bfs::create_directories(rd_dest_dir);
            } catch (const std::exception & ex) {
                std::cout << "  [simdb] An exception was encountered when "
                          << "attempting to create a report verification "
                          << "subfolder '" << rd_dest_dir << "'. The "
                          << "exception message was '" << ex.what()
                          << "'." << std::endl;
                continue;
            }
        }

        sparta::ReportRepository::DirectoryHandle directoryH =
            report_repository_->createDirectory(rd);

        size_t idx = 0;
        for(sparta::TreeNode* r : roots){
            attachReportTo_(directoryH, rd, r, replacements.at(idx));
            ++idx;
        }

        report_repository_->commit(&directoryH);
    }

    const SimulationConfiguration::AutoSummaryState auto_summary_state =
        (sim_config_ ? sim_config_->auto_summary_state : SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_NORMAL);
    if(auto_summary_state != SimulationConfiguration::AutoSummaryState::AUTO_SUMMARY_OFF){
        // Set up the default report
        auto_summary_report_.reset(new Report("Automatic Simulation Summary", root_.getSearchScope()));
        // Have the descriptor track this report but not own it
        auto subreport_gen_fxn = [](const TreeNode* tn,
                                    std::string& rep_name,
                                    bool& make_child_sr,
                                    uint32_t report_depth) -> bool {
            (void) report_depth;

            make_child_sr = true;

            // Note: Cannot currently test for DynamicResourceTreeNode without
            // knowing its template types. DynamicResourceTreeNode will need to
            // have a base class that is not TreeNode which can be used here.
            if(dynamic_cast<const ResourceTreeNode*>(tn) != nullptr
               || dynamic_cast<const RootTreeNode*>(tn) != nullptr
               || tn->hasChild(StatisticSet::NODE_NAME)){
                rep_name = tn->getLocation(); // Use location as report name
                return true;
            }
            return false;
        };
        auto_summary_report_->addSubtree(root_.getSearchScope(), // Subtree (including) n
                                         subreport_gen_fxn,      // Generate subtrees at specific nodes
                                         nullptr,                // Do not filter branches
                                         nullptr,                // Do not filter leaves
                                         true,                   // Add Counters
                                         true,                   // Add StatisticDefs
                                         -1);                    // Max recursion depth
    }
#ifdef SPARTA_PYTHON_SUPPORT
    //When using the Python shell, tell all descriptors to log their
    //statistics values to a binary archive / stream. This archive
    //can be accessed during simulation from Python.
    if (pyshell_) {
        statistics::StatisticsArchives * archives = report_repository_->getStatsArchives();
        pyshell_->publishStatisticsArchives(archives);
        statistics::StatisticsStreams * streams = report_repository_->getStatsStreams();
        pyshell_->publishStatisticsStreams(streams);
    }
#endif

    //Report configuration is locked down. Attempts to add or remove
    //descriptors either from C++ or Python will throw an exception.
    report_config_->disallowChangesToDescriptors_();

    //One-time inspection of the simulator's feature values (if any)
    report_repository_->inspectSimulatorFeatureValues(feature_config_);

#ifdef SPARTA_PYTHON_SUPPORT
    //If we are using the Python shell and the "simdb" feature is
    //featured on, publish our simulation database object to the
    //Python namespace now. This lets users run queries against the
    //database during simulation and access raw data (including .csv
    //report generation if requested).
    if (pyshell_ && stats_db_) {
        pyshell_->publishSimulationDatabase(stats_db_);
    }
#endif
}

void Simulation::setupDatabaseTriggers_()
{
    if (!sim_config_ || !sim_db_accessor_) {
        return;
    }

    const auto & access_opt_files = sim_config_->getDatabaseAccessOptsFiles();
    for (const auto & opt_file : access_opt_files) {
        sim_db_accessor_->setAccessOptsFromFile_(opt_file);
    }
}

ReportDescVec Simulation::expandReportDescriptor_(const ReportDescriptor & rd) const
{
    auto expand_rd = [](const ReportDescriptor & rd_in,
                        std::vector<ReportDescriptor> & rds_out)
    {
        std::string no_whitespace(rd_in.format);
        boost::erase_all(no_whitespace, " ");

        std::vector<std::string> formats;
        boost::split(formats, no_whitespace, boost::is_any_of(","));

        //Only comma-separated formats need to expand the descriptor
        if (formats.size() == 1) {
            rds_out.emplace_back(rd_in);
            return;
        }

        //Expand the descriptor as follows (example):
        //   Desc
        //     format:    csv, csv_cumulative
        //     dest_file: out.csv
        //
        //   ExpandedDesc1
        //     format:    csv
        //     dest_file: out.csv
        //
        //   ExpandedDesc2
        //     format:    csv_cumulative
        //     dest_file: out_cumulative.csv
        for (const auto & fmt : formats) {
            ReportDescriptor expanded(rd_in);
            expanded.format = fmt;
            auto underscore_idx = fmt.find("_");
            if (underscore_idx != std::string::npos) {
                boost::filesystem::path dest_file_path(expanded.dest_file);
                auto dot_idx = expanded.dest_file.find(".");
                const std::string stem = expanded.dest_file.substr(0, dot_idx);
                const std::string ext = boost::filesystem::extension(dest_file_path);
                expanded.dest_file = stem + fmt.substr(underscore_idx) + ext;
            }
            rds_out.emplace_back(expanded);
        }
    };

    std::vector<ReportDescriptor> final_rds;
    std::vector<ReportDescriptor> expanded_rds;
    expand_rd(rd, expanded_rds);
    std::swap(expanded_rds, final_rds);

    if (sim_config_->shouldGenerateStatsMapping()) {
        auto create_stats_mapping_rd = [](const ReportDescriptor & rd_in,
                                          std::vector<ReportDescriptor> & rd_out)
        {
            if (rd_in.format == "stats_mapping") {
                rd_out.emplace_back(rd_in);
                return;
            }

            //From an input filename "foo.csv", create an expanded
            //filename "foo_stats_mapping.json"
            auto dot_idx = rd_in.dest_file.find(".");
            sparta_assert(dot_idx != std::string::npos);
            const std::string mapping_fname =
                rd_in.dest_file.substr(0, dot_idx) + "_stats_mapping.json";

            //Expanded descriptor for the statistics mapping
            rd_out.emplace_back(rd_in.loc_pattern,
                                rd_in.def_file,
                                mapping_fname,
                                "stats_mapping");

            //Original descriptor
            rd_out.emplace_back(rd_in);
        };

        expanded_rds.clear();
        for (const auto & lrd : final_rds) {
            create_stats_mapping_rd(lrd, expanded_rds);
        }
        std::swap(final_rds, expanded_rds);
    }

    return final_rds;
}

void Simulation::setupProfilers_()
{
    if (!sim_config_) {
        return;
    }

    auto & def_file = sim_config_->getMemoryUsageDefFile();
    if (def_file.empty()) {
        return;
    }

#ifdef SPARTA_TCMALLOC_SUPPORT
    memory_profiler_.reset(new MemoryProfiler(def_file, getRoot(), this));
#else
    throw sparta::SpartaException(
      "Invalid use of --log-memory-usage command "
      "line option. Required library 'tcmalloc' was "
      "not found, so SPARTA memory profiling was disabled.");
#endif
}

void Simulation::setupStreamControllers_()
{
    //If report statistics are being streamed out of this simulation, share
    //the run controllers's stream controller object with each of the statistics
    //stream root nodes.

    //Here is the controller being shared with everybody:
    std::shared_ptr<statistics::StreamController> controller = rc_->getStreamController();

    //Get to the root of each report stream...
    statistics::StatisticsStreams * streams = report_repository_->getStatsStreams();
    const std::vector<std::string> stream_root_names = streams->getRootNames();
    for (const auto & name : stream_root_names) {
        statistics::StreamNode * stream_root = streams->getRootByName(name);

        //And share the controller with that report stream:
        stream_root->setStreamController(controller);
    }

#ifdef SPARTA_PYTHON_SUPPORT
    //Share the database thread object with the Python module
    auto db_task_thread = stats_db_ ? stats_db_->getTaskQueue() : nullptr;
    if (pyshell_ && db_task_thread != nullptr) {
        pyshell_->publishDatabaseController(db_task_thread);
    }
#endif
}

void Simulation::delayedPeventStart_()
{
    auto ctr = pevent_start_trigger_->getCounter();
    auto clk = pevent_start_trigger_->getClock();
    std::cout << "     [trigger] Now starting all reports after warmup delay of " << pevent_warmup_icount_
              << " on counter: " << ctr << ". Ocurred at tick "
              << scheduler_->getCurrentTick() << " and cycle "
              << clk->currentCycle() << " on clock " << clk << std::endl;

    // We just create a temporary trigger to start running pevents immidietely
    sparta::trigger::PeventTrigger trigger(getRoot());
    trigger.go();
}

void Simulation::rootDescendantAdded_(const sparta::TreeNode& node_added)
{
    // Install taps that are not triggered.  Currently this is all of them.
    if(sim_config_ &&
       (sim_config_->trigger_on_type == SimulationConfiguration::TriggerSource::TRIGGER_ON_NONE))
    {
        // Check each tap pattern against the node location
        for(const log::TapDescriptor& td : sim_config_->getTaps()){
            bool match = false;
            try{
                match = node_added.locationMatchesPattern(td.getLocation(), root_.getSearchScope());
            }catch(SpartaException& ex){
                // Suppressed exception. Possibly an invalid location string used for pattern matching.
                // Either way, ignore it.
                if(!td.hasBadPattern()){
                    std::cerr << "Warning: suppressed exception from tap " << td.stringize() << ": "
                              << ex.what() << std::endl;
                    td.setBadPattern(true);
                }
            }

            if(match){
                sparta::TreeNode* node = TreeNodePrivateAttorney::getChild(root_.getSearchScope(), node_added.getLocation());
                attachTapTo_(td, node);
            }
        }
    }
}

void Simulation::attachTapTo_(const log::TapDescriptor& td, sparta::TreeNode* n)
{
    sparta_assert(n != nullptr);
    std::cout << "  [out] placing tap on node " << n->getLocation()
              << " for: " << td.stringize() << std::endl;
    sparta::log::Tap* t;
    if(td.getDestination() == utils::COUT_FILENAME){
        t = new sparta::log::Tap(n, td.getCategory(), std::cout);
    }else if(td.getDestination() == utils::CERR_FILENAME){
        t = new sparta::log::Tap(n, td.getCategory(), std::cerr);
    }else{
        t = new sparta::log::Tap(n, td.getCategory(), td.getDestination());
    }
    td.incrementUsageCount();
    taps_to_del_.emplace_back(t);
}

void Simulation::attachReportTo_(sparta::ReportRepository::DirectoryHandle directoryH,
                                 const ReportDescriptor & rd,
                                 sparta::TreeNode* n,
                                 const std::vector<std::string>& replacements)
{
    sparta_assert(n != nullptr);

    const std::string def_file = rd.def_file;
    bool auto_expand_context_counter_stats = false;
    auto iter = rd.extensions_.find("expand-cc");
    if (iter != rd.extensions_.end()) {
        auto_expand_context_counter_stats = boost::any_cast<bool>(iter->second);
    }

    std::stringstream rep_name;
    rep_name << def_file << " on " << n->getLocation();
    std::unique_ptr<Report> r(new Report(rep_name.str(), n)); // Construct with context
    sparta_assert(r);

    if (auto_expand_context_counter_stats) {
        r->enableContextCounterStatsAutoExpansion();
    }

    if(def_file == "@"){
        // Defer to autopopulate so that report-all matches behavior of
        // reporting with a autopopulate block having default options
        std::vector<std::string> captures;
        r->autoPopulate(n,
                        "",  // Filter nothing
                        captures,
                        -1,  // max_recurs_depth
                        -1); // max_report_depth
    }else{
        std::vector<std::string> search_paths = sim_config_->getReportDefnSearchPaths();
        std::string definition_file = def_file;

        while (!boost::filesystem::exists(definition_file) && !search_paths.empty()) {
            boost::filesystem::path p(search_paths.back());
            search_paths.pop_back();
            p /= def_file;
            definition_file = p.string();
        }

        if (!boost::filesystem::exists(definition_file) && search_paths.empty()) {
            const std::vector<std::string> & command_line_search_paths =
                sim_config_->getReportDefnSearchPaths();

            std::ostringstream oss;
            oss << "Report definition file '" << def_file << "' was not found. ";
            if (!command_line_search_paths.empty()) {
                oss << "The following directories were searched (--report-search-dir):\n";
                for (const auto & dir : boost::adaptors::reverse(command_line_search_paths)) {
                    oss << "\t" << dir << std::endl;
                }
            } else {
                oss << "If this definition file exists in another directory, you may add "
                       "to the simulation's search path with the --report-search-dir command "
                       "line option." << std::endl;
            }
            throw SpartaException(oss.str());
        }

        r->addFileWithReplacements(definition_file,
                                   replacements,
                                   sim_config_->verbose_cfg);
    }

    report_repository_->addReport(directoryH, std::move(r));
}

void Simulation::checkAllVirtualParamsRead_(const ParameterTree& pt)
{
    std::vector<const ParameterTree::Node*> unread_nodes;
    pt.getUnreadValueNodes(&unread_nodes);

    if(unread_nodes.size() > 0){
        uint32_t errors = 0;
        std::stringstream err_list;
        for(auto node : unread_nodes){
            const std::string path = node->getPath();
            // Parameter is still unbound if there is no corresponding node in the tree.
            // In the guture, this should actually look to see it was consumed by that parameter
            // node, which it will be when the phase of reading from a config file directly to a
            // parameter node is removed and the ubound tree is used instead.
            std::vector<TreeNode*> found;
            root_.getSearchScope()->findChildren(path, found);
            bool ok = false;
            for (auto n : found){
                // Ensure found node is an actual parameter
                if(dynamic_cast<const sparta::ParameterBase*>(n) != nullptr){
                    ok = true;
                    break;
                }
            }
            if(!ok){
                if(node->isRequired()){
                    errors++;
                    err_list << "    ERROR: unread unbound parameter: \"" << path << "\" from: \""
                              << node->getOrigin() << "\". value: \"" << node->getValue() << "\". Path exists in tree up to: \""
                              << root_.getSearchScope()->getDeepestMatchingPath(path) << "\"" << std::endl;
                }else if(!sim_config_->suppress_unread_parameter_warnings) {
                    std::cerr << "    NOTE: unread optional unbound parameter: \"" << path << "\" from: \""
                              << node->getOrigin() << "\". value: \"" << node->getValue() << "\". Path exists in tree up to: \""
                              << root_.getSearchScope()->getDeepestMatchingPath(path) << "\"" << std::endl;
                }
            }
        }
        if(errors > 0){
            SpartaException ex("");
            ex << "Found " << errors << " unread unbound parameters. These "
                  "parameter were specified by a configuration file or the command line but do not "
                  "correspond to any Parameter nodes in the device tree and were never directly "
                  "read from the unbound tree:\n";
            ex << err_list.str();
            ex << "\n This can be the result of supplying an archicture yaml that sets an expected topology followed by -c/-p options that change that topology. \n"
                "\tIn this case, consider a new architecture or supply the architecture yaml file as a '-c' option instead of the '--arch' option";
            throw ex;
        }
    }
}

void Simulation::setSimulationController_(
    std::shared_ptr<SimulationController> controller)
{
    controller_ = controller;
}

void Simulation::pause_()
{
    if (controller_ != nullptr) {
        controller_->pause();
    }
}

void Simulation::resume_()
{
    if (controller_ != nullptr) {
        controller_->resume();
    }
}

void Simulation::terminate_()
{
    if (controller_ != nullptr) {
        controller_->terminate();
    }
}

void Simulation::customEvent_(const std::string & event_name)
{
    if (controller_ != nullptr) {
        controller_->invokeNamedEvent(event_name);
    }
}

void Simulation::SimulationController::pause()
{
    if (sim_status_ != SimulationController::SimulationStatus::Simulating) {
        return;
    }

    verifyFinalized_();
    pause_(sim_);
    sim_status_ = SimulationController::SimulationStatus::Paused;
}

void Simulation::SimulationController::resume()
{
    if (sim_status_ != SimulationController::SimulationStatus::Paused) {
        return;
    }

    verifyFinalized_();
    resume_(sim_);
    sim_status_ = SimulationController::SimulationStatus::Simulating;
}

void Simulation::SimulationController::terminate()
{
    if (sim_status_ != SimulationController::SimulationStatus::Simulating) {
        return;
    }

    verifyFinalized_();
    terminate_(sim_);
    sim_status_ = SimulationController::SimulationStatus::Terminated;
}

void Simulation::SimulationController::invokeNamedEvent(
    const std::string & event_name)
{
    if (sim_status_ != SimulationController::SimulationStatus::Simulating) {
        return;
    }

    verifyFinalized_();
    auto iter = callbacks_.find(event_name);
    if (iter == callbacks_.end()) {
        if (invoked_callbacks_.find(event_name) == invoked_callbacks_.end()) {
            throw SpartaException("A simulation event named '") << event_name << "' was "
                "encountered in a control file (--control) but there was no such callback "
                "given to the sparta::app::Simulation::SimulationController base class by "
                "that name";
        }
        return;
    }
    iter->second();
    callbacks_.erase(iter);
    invoked_callbacks_.insert(event_name);
}

void Simulation::SimulationController::addNamedCallback_(
    const std::string & event_name,
    SpartaHandler callback_method)
{
    if (callbacks_.find(event_name) != callbacks_.end()) {
        throw sparta::SpartaException("There is an event named '") << event_name
            << "' already registered with this controller";
    }
    if (sim_->getRoot()->isFinalizing() || sim_->getRoot()->isFinalized()) {
        throw sparta::SpartaException(
            "Cannot add a new named callback to a simulation "
            "controller after the device tree has been finalized");
    }
    callbacks_.insert(std::make_pair(event_name, callback_method));
}

void Simulation::SimulationController::verifyFinalized_() const
{
    if (!sim_->getRoot()->isFinalized()) {
        throw sparta::SpartaException(
            "You may not invoke simulation controller callbacks "
            "until after the device tree has been finalized");
    }
}

    } // namespace sparta
} // namespace app
