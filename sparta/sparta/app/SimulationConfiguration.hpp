// <SimulationConfiguration> -*- C++ -*-


/*!
 * \file SimulationConfiguration.hpp
 * Class for configuring a simulation
 */

#ifndef __SIMULATION_CONFIGURATION_H__
#define __SIMULATION_CONFIGURATION_H__

#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include <memory>
#include <list>
#include <ostream>
#include <type_traits>
#include <utility>

#include "sparta/sparta.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/app/MetaTreeNode.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/app/ConfigApplicators.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
namespace app {

constexpr char NoPipelineCollectionStr[] = "NOPREFIX_";

/*!
 * \brief Optional default values for the simulator which can be customized
 * and provided by anyone instantiating the simulator at construction-time.
 */
class DefaultValues
{
public:
    /*!
     * Default for architecture search directory (--arch-search-dir)
     */
    std::vector<std::string> arch_search_dirs = {""};

    /*!
     * Default for --arch.  This is not processed by
     * SimulationConfiguration as a user might supply a different arch
     * on the command line
     */
    std::string arch_arg_default = "";

    /*!
     * is an --arch option required for command line parsing to succeed?
     */
    bool non_empty_arch_arg_required = false;

    /*!
     * Default name of clock used to run by cycles (-r) if not
     * overridden by users
     */
    std::string run_time_clock = "";

    /*!
     * Default name of instruction counter used to run by icount and
     * instruction-based debug triggers
     */
    std::string inst_counter = "";

    /*!
     * Meta-parameters. Simulators can provide a list of parameter
     * templates which will be added to the meta parameters set.
     */
    std::list<ParameterTemplate> other_meta_params = {};

    /*!
     * Set the default value for the auto_summary flag.
     * Values:
     *    "on" or "normal" -- write summary after simulation run
     *    "verbose"        -- write summary after simulation run with description
     *    "off"            -- do nothing
     */
    std::string auto_summary_default = "off";

    /*!
     * From "top.core*", what is the path to the counter specifying
     * retired instructions on a given core? For instance,
     *
     *    Retired instruction counters:
     *       "top.core0.rob.stats.total_number_retired"
     *       "top.core1.rob.stats.total_number_retired"
     *
     *    Then the path_to_retired_inst_counter is:
     *       "rob.stats.total_number_retired"
     */
    enum class RetiredInstPathStrictness {
        Strict, Relaxed
    };
    std::pair<std::string, RetiredInstPathStrictness> path_to_retired_inst_counter =
        std::make_pair("rob.stats.total_number_retired",
                       RetiredInstPathStrictness::Relaxed);
};

/*!
 * \class SimulationConfiguration
 * \brief Configuration applicator class that is used for configuring
 * a simulator. Works in conjunction with sparta::app::Simulation
 *
 * This class will help a user set up a derived sparta::app::Simulation
 * object, but nothing else.  It's up the user to determine how to run
 * simulation, either by calling sparta::app::Simulation::run directly
 * or via the sparta::SysCSpartaSchedulerAdapter::run method if the
 * simulation is embedded in a cosimulation environment where the
 * SystemC scheduler is master of simulation.
 *
 */
class SimulationConfiguration
{
private:

    //! Defaults
    const DefaultValues defaults_;

public:

    //! Create a SimulationConfiguration
    SimulationConfiguration(const DefaultValues & defaults = DefaultValues());

    //! Handle an individual parameter
    //! \param pattern The path to the parameter node to modify
    //! \param value The value to be written
    //! \param ignore Is this an optional parameter?
    void processParameter(const std::string & pattern, const std::string & value,
                          bool optional = false);

    //! Consume a configuration (.yaml) file
    //! \param pattern The node to apply the given yaml file.  Use "" for top
    //! \param filename The yaml file to consume
    //! \param is_final Is this a final configuration file
    void processConfigFile(const std::string & pattern, const std::string & filename,
                           bool is_final = false);

    //! Configure simulator for specific architecture.  Will look in
    //! the directories supplied in arch_search_paths
    void processArch(const std::string & pattern,
                     const std::string & filename);

    //! Enable logging on a specific node, for a specific category,
    //! and redirect output to the given destination
    void enableLogging(const std::string & pattern,
                       const std::string & category,
                       const std::string & destination);

    //! Was a final configuration file provided?
    bool hasFinalConfig() const { return !final_config_file_.empty(); }

    //! Consume an extension (.yaml) file
    void processExtensionFile(const std::string & filename);

    //!  Set the filename for the State Tracking file
    void setStateTrackingFile(const std::string & filename);

    //! Get the State Tracking filename
    const std::string& getStateTrackingFilename() const;

    //! Consume a simulation control file
    void addControlFile(const std::string & filename);

    //! Get all control files for this simulation
    const std::set<std::string> & getControlFiles() const;

    //! Add run metadata as a string
    void addRunMetadata(const std::string & name,
                        const std::string & value);

    //! Get run metadata as key-value pairs
    const std::vector<std::pair<std::string, std::string>> & getRunMetadata() const;

    //! Get run metadata stringized as "name1=value1,name2=value2,..."
    std::string stringizeRunMetadata() const;

    //! Get the final config file name
    std::string getFinalConfigFile() const { return final_config_file_; }

    //! Set filename which contains heap profiler settings
    void setMemoryUsageDefFile(const std::string & def_file);

    //! Get filename for heap profiler configuration
    const std::string & getMemoryUsageDefFile() const;

    //! Auto-generate mappings from report column headers to statistic names
    void generateStatsMapping();

    //! Get statistics mapping "enabled" flag
    bool shouldGenerateStatsMapping() const;

    //! Disable pretty printing for the given file format ("json", "html", ...)
    //! Note that not all formats differentiate between pretty print and
    //! non-pretty print, so calling this method will have no effect for
    //! those reports
    void disablePrettyPrintReports(const std::string & format);

    //! Get all report file extensions which have had their pretty printing disabled
    const std::set<std::string> & getDisabledPrettyPrintFormats() const;

    //! Specify that a given report format (NOT a report extension) is
    //! to omit StatisticInstance's that have value 0. To be clear,
    //! report format is something like "json" or "json_reduced", while
    //! the report extension for both of these formats is just "json".
    void omitStatsWithValueZeroForReportFormat(const std::string & format);

    //! Get all report formats which are to omit all SI values that are zero
    const std::set<utils::lowercase_string> &
        getReportFormatsWhoOmitStatsWithValueZero() const;

    /*!
     * Look for any tree node extensions from the arch / config
     * ParameterTree's, and merge those extensions into the extensions
     * ParameterTree.
     */
    void copyTreeNodeExtensionsFromArchAndConfigPTrees();

    /*!
     * Returns a ParameterTree containing an unbound set of parameter
     * values which can be read and later applied. Some of these
     * parameters will be applied to Parameter TreeNodes at some point
     * and others will not
     */
    ParameterTree& getUnboundParameterTree() { return ptree_; }

    /*!
     * Returns a ParameterTree (const version) containing an unbound
     * set of parameter values which can be read and later
     * applied. Some of these parameters will be applied to Parameter
     * TreeNodes at some point and others will not
     */
    const ParameterTree& getUnboundParameterTree() const { return ptree_; }

    /*!
     * Returns an architectural ParameterTree containing an unbound
     * set of parameter values which can be read and later applied as
     * defaults to newly-constructed parameters.
     */
    ParameterTree& getArchUnboundParameterTree() { return arch_ptree_; }

    /*!
     * Returns an architectural ParameterTree (const version)
     * containing an unbound set of parameter values which can be read
     * and later applied as defaults to newly-constructed parameters.
     */
    const ParameterTree& getArchUnboundParameterTree() const { return arch_ptree_; }

    /*!
     * \brief Returns a ParameterTree containing an unbound set of
     * named tree node extensions and their parameter value(s).
     */
    ParameterTree& getExtensionsUnboundParameterTree() {
        return extensions_ptree_;
    }

    /*!
     * \brief Returns a ParameterTree (const version) containing an
     * unbound set of named tree node extensions and their parameter
     * value(s).
     */
    const ParameterTree& getExtensionsUnboundParameterTree() const {
        return extensions_ptree_;
    }

    /*!
     * Was an arch file provided in this configuration?
     */
    bool archFileProvided() const { return arch_applicator_ != nullptr; }

    //! Print the ArchNodeConfigFileApplicator for informative messages
    void printArchConfigurations(std::ostream & os) const {
        if(archFileProvided()) {
            os << arch_applicator_->stringize();
        }
        else {
            os << "<not provided>";
        }
    }

    /*!
     * Append to the front of the architectural directory paths list
     * the given directory
     */
    void addArchSearchPath(const std::string & dir) {
        arch_search_paths_.emplace(arch_search_paths_.begin(), dir);
    }

    /*!
     * Append to the front of the configuration directory path list
     * the given directory
     */
    void addConfigSearchPath(const std::string & dir) {
        config_search_paths_.emplace(config_search_paths_.begin(), dir);
    }

    /*!
     * Append to the front of the report definitions directory path list
     * the given directory
     */
    void addReportDefnSearchPath(const std::string & dir) {
        report_defn_search_paths_.emplace(report_defn_search_paths_.begin(), dir);
    }

    /*!
     * Return all report definition search paths applied to this simulation
     * (command line option --report-search-path)
     */
    const std::vector<std::string> & getReportDefnSearchPaths() const {
        return report_defn_search_paths_;
    }

    //! Print the generic configurations for informative messages
    void printGenericConfigurations(std::ostream & os) const {
        for(auto & cp : config_applicators_) {
            os << "    " << cp->stringize() << '\n';
        }
    }

    /*!
     * Set the location of the SimDB file produced during simulation
     */
    void setSimulationDatabaseLocation(const std::string & loc) {
        simdb_location_ = loc;
    }

    /*!
     * Add a YAML options file specifying which components are allowed
     * to access the simulation database, and (optionally) when that
     * access is to be granted or removed during simulation.
     *
     *     \code
     *         stats:
     *           components:
     *             top.cpu.core0.rob
     *             root.clocks
     *         bpred:
     *           start: notif.dbaccess == 1
     *           stop:  notif.dbaccess == 0
     *     \endcode
     *
     * In the above example, this would mean that for the "stats"
     * database namespace, components "top.cpu.core0.rob" and
     * "root.clocks" are the only ones that can access the database,
     * and they can access it at any time.
     *
     * For the "bpred" namespace however, all components / device
     * tree locations can access the database when the notification
     * channel "dbaccess" has emitted a value of 1, but they are
     * restricted from accessing the database when the same channel
     * emits a value of 0.
     *
     * This YAML file could also list specific components that can
     * access the database, as well as start/stop triggers for finer
     * control over when the database is available for those components.
     * For example:
     *
     *     \code
     *         bpred:
     *           components:
     *             top.cpu.core0.rob
     *           start: notif.dbaccess == 1
     *           stop:  notif.dbaccess == 0 || notif.earlyterm == 1
     *     \endcode
     *
     * The syntax for the "start" and "stop" database triggers
     * is the same as report triggers found in YAML descriptor
     * files (--report <file.yaml>)
     *
     * \note The expression parser for these YAML files does not
     * support trigger tags. The following example YAML would
     * fail to parse:
     *
     *     \code
     *         stats:
     *           start: top.core0.rob.stats.total_number_retired >= 1000
     *           tag:   t0
     *         bpred:
     *           stop:  t0.start
     *     \endcode
     */
    void addSimulationDatabaseAccessOptsYaml(
        const std::string & opts_file)
    {
        simdb_enabled_components_opts_files_.emplace_back(opts_file);
    }

    /*!
     * Set the root directory where all of this simulation's legacy
     * reports should be copied to. Say the root directory was set
     * to "/tmp" and the simulation was configured to produce the
     * following report files:
     *
     *      Filename         Format
     *     -------------    -----------
     *      foo.csv          csv_cumulative
     *      foo.json         json_reduced
     *      bar.json         json_reduced
     *      baz.json         json_detail
     *
     * If the SimDB file ended up being "abcd-1234.db", then the
     * simulation would *copy* (not move) the four legacy reports
     * listed above into a directory structure that looks like:
     *
     *     /tmp
     *       /abcd-1234
     *         /csv_cumulative
     *           foo.csv
     *         /json_reduced
     *           foo.json
     *           bar.json
     *         /json_detail
     *           baz.json
     *
     * \param reports_root_dir Root directory where the reports
     * will be copied. Directory will be created if needed.
     *
     * \param collected_formats Specific report formats you want
     * to collect. If left empty, all formats will be collected.
     */
    void setLegacyReportsCopyDir(const std::string & reports_root_dir,
                                 const std::set<std::string> & collected_formats = {})
    {
        simdb_legacy_reports_copy_dir_ = reports_root_dir;
        for (const auto & fmt : collected_formats) {
            const utils::lowercase_string lower_fmt = fmt;
            simdb_legacy_reports_collected_formats_.insert(lower_fmt.getString());
        }
    }

    /*!
     * Get the simulation database location configured for this
     * simulation. This will return an empty string if a non-default
     * location was never set at the command line.
     */
    const std::string & getSimulationDatabaseLocation() const {
        return simdb_location_;
    }

    /*!
     * Get the list of SimDB access YAML files that specify which
     * components have been granted access to the simulation database,
     * and (optionally) when they that access is to be enabled / disabled
     * using trigger expressions.
     */
    const std::vector<std::string> & getDatabaseAccessOptsFiles() const {
        return simdb_enabled_components_opts_files_;
    }

    /*!
     * Get the root directory where all of this simulation's legacy
     * reports were copied to.
     * \note Intended for internal / developer use.
     */
    const std::string & getLegacyReportsCopyDir() const {
        return simdb_legacy_reports_copy_dir_;
    }

    /*!
     * Get the specific legacy report formats that are being collected.
     * If empty, either all formats are being collected, or the report
     * collection command line option is not being used.
     * \note Intended for internal / developer use.
     */
    const std::set<std::string> & getLegacyReportsCollectedFormats() const {
        return simdb_legacy_reports_collected_formats_;
    }

    /*!
     * \brief Controls installation of signal handlers.
     */
    enum class SignalMode
    {
        DISABLE_BACKTRACE_SIGNALS,
        ENABLE_BACKTRACE_SIGNALS
    };

    /*!
     * Behavior of post-run debug dumping
     */
    enum class PostRunDebugDumpPolicy {
        DEBUG_DUMP_ALWAYS = 0, //!< Dump debug data in all cases after running
        DEBUG_DUMP_NEVER  = 1, //!< Never dump debug data after running
        DEBUG_DUMP_ERROR  = 2, //!< Dump debug data only if there is an error while running
        DEBUG_DUMP_MAX    = 3  //!< Invalid enum value
    };

    /*!
     * Content of post-run debug dumping
     */
    enum class PostRunDebugDumpOptions {
        DEBUG_DUMP_EVERYTHING,
        DEBUG_DUMP_NOTHING,
        DEBUG_DUMP_BACKTRACE_ONLY
    };

    /*!
     * Behavior of auto-summary writing
     */
    enum class AutoSummaryState {
        AUTO_SUMMARY_OFF     = 0, //!< Do not write summary
        AUTO_SUMMARY_NORMAL  = 1, //!< Write normal summary
        AUTO_SUMMARY_VERBOSE = 2, //!< Write verbose summary
        AUTO_SUMMARY_MAX     = 3  //!< Invalid enum value
    };

    /*!
     * Type of trigger to create using trigger_on_value for enabling
     * pipeout collection, logging, and pevents
     */
    enum class TriggerSource {
        TRIGGER_ON_NONE = 0,
        TRIGGER_ON_CYCLE,
        TRIGGER_ON_INSTRUCTION
    };

    /*!
     * Should log messages in the category log::categories::WARN be
     * written to stderr
     */
    bool warn_stderr = true;

    /*!
     * Show verbose messages while parsing input files (e.g. reports
     * parameters).
     */
    bool verbose_cfg = false;

    /*!
     * Show verbose report trigger messages
     */
    bool verbose_report_triggers = false;

    /*!
     * Should simulator-framework debug messages be written
     */
    bool debug_sim = false;

    /*!
     * Write reports o error
     */
    bool report_on_error = true;

    /*!
     * Signal configuration -- enable signal catching or not.  By
     * default, SPARTA will catch all signals thrown by a running
     * process.  This can be difficult however, for EDA tools that
     * install their own.
     */
    SignalMode signal_mode{SignalMode::ENABLE_BACKTRACE_SIGNALS};

    /*!
     * Automatic summary state. Determines what to do with the
     * automatic summary after running.  Valid values are found in
     * sparta::app""Simulation.
     */
    AutoSummaryState auto_summary_state{AutoSummaryState::AUTO_SUMMARY_OFF};

    /*!
     * Debug dumping policy
     */
    PostRunDebugDumpPolicy debug_dump_policy{PostRunDebugDumpPolicy::DEBUG_DUMP_NEVER};

    /*!
     * Debug dumping options
     */
    PostRunDebugDumpOptions debug_dump_options{PostRunDebugDumpOptions::DEBUG_DUMP_EVERYTHING};

    /*!
     * Filename of error/final dump. Can be "" to auto-generate the name
     */
    std::string dump_debug_filename{"error-dump.dbg"};

    /*!
     * Cycle count or instruction value to start the debugging
     */
    uint64_t trigger_on_value = 0;

    //! How to interpret the trigger_on_value number -- cycle or
    //! instruction number
    TriggerSource trigger_on_type{TriggerSource::TRIGGER_ON_NONE};

    //! The clock to set the trigger on if trigger_on_type is set to
    //! TRIGGER_ON_CYCLE
    std::string trigger_clock;

    /*!
     * Should the simulator validate itself after running
     */
    bool validate_post_run = false;

    /*!
     * File to which warnings should be logged by default
     */
    std::string warnings_file{""};

    /*!
     * During simulation configuring, dump the contents of the DAG
     */
    bool show_dag = false;

    /*!
     * The default pipeline collection prefix for pipeline collection
     * (-z option).  Empty string means no prefix
     */
    std::string pipeline_collection_file_prefix = NoPipelineCollectionStr;

    /*!
     * Additional report descriptions
     */
    ReportDescVec reports;

    /*!
     * Scheduler control: When a user calls sparta::Simulation::run()
     * with an amount of time _other than_ the default, the Scheduler
     * can do one of two things:
     *
     * 1. It can advance the internal ticks and elapsed_tick counts by
     *    the exact amount regardless of whether it has work to do.
     *
     * 2. It can ignore the amount of time to advance and just return.
     *
     * If this boolean is set to true, the Scheduler will perform the
     * first action.  If set to false it will perform the second action.
     */
    bool scheduler_exacting_run = false;

    /*!
     * Scheduler control: When a user calls sparta::Simulation::run(),
     * should the Scheduler measure its own performance?  This is
     * expensive (and pointless) if a user of SPARTA is doing a run that
     * is less than infinite.
     */
    bool scheduler_measure_run_time = true;

    //! From "top.core*", what is the path to the counter specifying
    //! retired instructions on a given core?
    std::string parsed_path_to_retired_inst_counter_;
    std::pair<std::string, app::DefaultValues::RetiredInstPathStrictness> path_to_retired_inst_counter =
        std::make_pair(defaults_.path_to_retired_inst_counter.first,
                       app::DefaultValues::RetiredInstPathStrictness::Relaxed);

    //! Get the defaults this configuration was made with
    const DefaultValues & getDefaults() const { return defaults_; }

    //! Get the taps vector
    const log::TapDescVec & getTaps() const { return taps_; }

    //! Called by app::Simulation to indicate that this configuration
    //! has been consumed.  Const since this class is passed as a
    //! const object to app::Simulation
    void setConsumed() const { is_consumed_ = true; }

private:

    //! Set to true if this configuration has been consumed
    mutable bool is_consumed_ = false;

    ////////////////////////////////////////////////////////////////////////////////
    // Simulation configuration, your -c, -p, --arch flags from Command line

    //! The file to read from for reading in a final config file
    std::string final_config_file_ = "";

    //! Heap profiler configuration file
    std::string memory_usage_def_file_;

    //! Flag saying if the simulator should produce report files which
    //! map report column headers to statistics names
    bool generate_stats_mapping_ = false;

    //! List of all report file formats ("json", "html", etc.) for whichpp
    //! pretty printing has been disabled
    std::set<std::string> disabled_pretty_print_report_formats_;

    //! List of report formats ("json", "json_reduced", etc.) for which
    //! statistics that have value 0 are to be omitted
    std::set<utils::lowercase_string> zero_values_omitted_report_formats_;

    //! Unbound (pre-application) Architectural Parameter Tree (parameter defaults)
    ParameterTree arch_ptree_;

    //! Unbound (pre-application) Parameter Tree
    ParameterTree ptree_;

    //! Unbound (pre-application) Extensions Tree
    ParameterTree extensions_ptree_;

    //! Vector of arch file search directories
    std::vector<std::string> arch_search_paths_;

    //! Vector of configuration search directories
    std::vector<std::string> config_search_paths_ = {"./"};

    //! Vector of report definition file search directories
    std::vector<std::string> report_defn_search_paths_;

    //! Vector of simulation control files
    std::set<std::string> simulation_control_filenames_;

    //! Vector of run metadata as key-value pairs
    std::vector<std::pair<std::string, std::string>> run_metadata_;

    //! The created arch applicator for informative messages
    std::unique_ptr<ArchNodeConfigFileApplicator> arch_applicator_;

    //! The created Node/Parameter applicators for informative messages
    ConfigVec     config_applicators_;

    //
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // Simulation logging
    /*!
     * Vector of Tap descriptors to instantiate on the simulator
     */
    log::TapDescVec taps_;

    //! The name of the State Tracking file.
    std::string state_tracking_file_;

    //
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // Simulation database

    //! Location of the SimDB file produced during simulation
    std::string simdb_location_;

    //! SimDB access options are given in YAML files containing
    //! component locations, such as "top.cpu.core0.rob" or
    //! "root.clocks", which can also specify trigger definitions
    //! that say when a component is granted access to the database.
    std::vector<std::string> simdb_enabled_components_opts_files_;

    //! Root directory where legacy reports generated from this
    //! simulation are copied to. For internal / developer use.
    std::string simdb_legacy_reports_copy_dir_;

    //! Specific formats of the legacy reports we are collecting,
    //! if any. For internal / developer use.
    std::set<std::string> simdb_legacy_reports_collected_formats_;

    //
    ////////////////////////////////////////////////////////////////////////////////
};

} // namespace app
} // namespace sparta

#endif // #ifndef __SIMULATION_CONFIGURATION_H__
