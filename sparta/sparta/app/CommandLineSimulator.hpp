// <CommandLineSimulator> -*- C++ -*-


/*!
 * \file CommandLineSimulator.hpp
 * \brief Class for creating a simulator based on command-line arguments
 */

#ifndef __COMMAND_LINE_SIMULATOR_H__
#define __COMMAND_LINE_SIMULATOR_H__

#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdint>
#include <vector>
#include <set>
#include <memory>
#include <string>
#include <unordered_map>

#include "sparta/report/Report.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/MultiDetailOptions.hpp"
#include "sparta/app/AppTriggers.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/pevents/PeventController.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/app/ReportDescriptor.hpp"

namespace boost::program_options {
template <class charT> class basic_parsed_options;
}  // namespace boost::program_options

namespace sparta::app {
class Simulation;
}  // namespace sparta::app

namespace sparta::trigger {
class Trigger;
class Triggerable;
}  // namespace sparta::trigger

namespace po = boost::program_options;
namespace pocls = boost::program_options::command_line_style;

namespace sparta {
class InformationWriter;

namespace app {

const constexpr char DefaultHeartbeat[]        = "0";


/*!
 * \brief Command line simulator front-end class with argument parsing
 * Works in conjunction with sparta::app::Simulation
 *
 * This class is extensible by clients by adding to its construction options
 * before parsing (see parse) or by subclassing.
 *
 * This class is intended to contain redundant sparta-enabled simulator code into
 * a single set of classes to reduce the work necessary when implementing new
 * simulators or maintaining a set of simulators and tests.
 *
 * Additionally, this class helps ensure that the command-line look and feel
 * of simulators and tests is consistent by providing the same argument names
 * and documentation as other simulators
 *
 * This class should mainly contain code relevant to setting up a simulator
 * based on command-line options. Generic simulator setup code that might be
 * invoked from a Python shell, remote interface, or something else does NOT
 * belong in this class. Such common code should be in sparta::app::Simulation.
 * If another simulation interface is desired, the functionality here should not
 * need to be re-implemented.
 */
class CommandLineSimulator
{
public:

    /*!
     * \brief Static DefaultValues for a SPARTA CommandLineSimulator
     */
    static DefaultValues DEFAULTS;

    //! \brief Not default constructable
    CommandLineSimulator() = delete;

    /*!
     * \brief Construct
     * \param usage String describing usage of the simulator. Example:
     * \verbatim
     * "Usage: ./sim_bin -r NUM_CYCLES [--other-args]"
     * \endverbatim
     */
    CommandLineSimulator(const std::string& usage,
                         const DefaultValues& defs=DEFAULTS);

    /*!
     * \brief Destructor
     */
    virtual ~CommandLineSimulator();

    /*!
     * \brief Has this simulator parsed the command line yet
     * Being parsed is required to call for populateSimulation
     * \note set to true after a successful call to parse
     */
    bool isParsed() const {
        return is_parsed_;
    }

    /*!
     * \brief Has this simulator been setup yet?
     * Being set up is required to call runSimulator
     * \note set to true after a successful call to populateSimulation
     */
    bool isSetup() const {
        return is_setup_;
    }

    /*!
     * \brief Gets the usage string specified for this simulator at
     * construction. This string is printed when usage errors are encountered
     */
    const std::string& getUsage() const { return usage_; }

    /*!
     * \brief Gets the sparta-specific options for this simulator.
     * \note This is a read-only result. Simulators must place their
     * application-specific options in the application or advanced options
     * sections
     * \see getApplicationOptions
     * \see getAdvancedOptions
     */
    const MultiDetailOptions& getSpartaOptions() const {
        return sparta_opts_;
    }

    /*!
     * \brief Gets the application-specific options for this simulator
     */
    MultiDetailOptions& getApplicationOptions() {
        return app_opts_;
    }

    /*!
     * \brief Gets the advanced options for this simulator
     */
    MultiDetailOptions& getAdvancedOptions() {
        return advanced_opts_;
    }

    /*!
     * \brief Gets the boost positional options for the parser owned by this
     * simulator
     */
    po::positional_options_description& getPositionalOptions() {
        return positional_opts_;
    }

    /*!
     * \brief Variables map populated by command-line parsing.
     * This map is populated within the parse() method
     */
    const po::variables_map& getVariablesMap() const {
        return vm_;
    }

    /*!
     * \brief Parse command line options
     * \param sim Simulation to configure after parsing
     * \param argc command line arg count
     * \param argv command line parameter string vector
     * \param err_code Reference to value where parsing will store and error
     * code. This is because this method could want the application to exit, but
     * to use a zero error code.
     * \post If successful, isParsed will now return true
     * \post If failed, prints errors to cout
     * \post getVariablesMap() will return the populated variables map
     * \post Reconstructs the command-line string. See getCommandLine
     * \return true if application should continus and false if not. See
     * err_code on failure for exit error code because returning false does not
     * necessarily indicate an error (e.g. -h)
     * \throw SpartaException if anything cannot be parsed
     * \note reparsing is not illegal, but is probably a bad idea
     */
    bool parse(int argc,
               char** argv,
               int& err_code);

    /*!
     * \brief DEPRECATED Shorthand version of other parse method,
     * \note THis is a deprecated signature and returns 0 on success, false on
     * failure
     */
    int parse(int argc,
              char** argv);

    /*!
     * \brief Builds the content of the simulator making it ready to run.
     * \note It is the responsibiltiy of the caller to actually run the
     * simulator manually or through CommandLineSimulator::runSimulator
     * \pre isParsed() must be true
     * \post isSetup() will be true
     */
    void populateSimulation(Simulation* sim);

    /*!
     * \brief Run the simulator for the specified number of cycles
     */
    void runSimulator(Simulation* sim);

    /*!
     * \brief Post-process the results of the simulation if applicable
     * \pre Simulator is done running and cannot be run again
     */
    void postProcess(Simulation* sim);

    /*!
     * Return whether this object is trying to run simulation for a finite
     * amount of time via some command line argument.
     */
    bool isRuntimeFinite() const {

        bool retval = (run_time_cycles_ != sparta::Scheduler::INDEFINITE) ||
                      (run_time_ticks_  != sparta::Scheduler::INDEFINITE);
        return retval;
    }

    /*!
     * Get the internal SimulationConfiguration this CommandLineSimulator uses
     */
    SimulationConfiguration &getSimulationConfiguration() {
        return sim_config_;
    }

    /*!
     * Get the internal SimulationConfiguration this CommandLineSimulator uses
     */
    const SimulationConfiguration &getSimulationConfiguration() const {
        return sim_config_;
    }

protected:

    /*!
     * \brief Implements populateSimulation
     */
    void populateSimulation_(Simulation* sim);

    /*!
     * \brief Implements runSimulator
     */
    void runSimulator_(Simulation* sim);

    /*!
     * \brief Implements postProcess
     */
    void postProcess_(Simulation* sim);

    /*!
     * \brief Callback before interpreting command parsed command line tokens
     * \param opts Parsed options with order information
     */
    virtual void postParse_(po::basic_parsed_options<char>& opts) {
        (void) opts;
    }

    //! Simulation configuration including default values
    SimulationConfiguration sim_config_;

    //! Simulation feature configuration
    FeatureConfiguration feature_config_;

    /*!
     * \brief Usage string specified at construction
     */
    const std::string usage_;

    /*!
     * \brief argc from main
     */
    int argc_ = 0;

    /*!
     * \brief argv from main
     */
    char **argv_ = nullptr;

    /*!
     * \brief Vector of Report descriptors to instantiate on the simulator
     */
    ReportDescVec reports_;

    /*!
     * \brief Vector of report descriptor definition files (YAML)
     */
    std::vector<std::string> report_descriptor_def_files_;

    /*!
     * \brief Vector of yaml placeholder key-value pairs. These will be
     * applied to the yamls given by "--report my_report_info.yaml"
     */
    app::ReportYamlReplacements report_yaml_placeholder_replacements_;

    /*!
     * \brief Map to hold report-specific yaml placeholder key-value pairs.
     * An example usage might be:
     *   <sim> --report foo.yaml rep1.yaml --report bar.yaml rep2.yaml
     * Where 'rep1.yaml' contains key-value replacements specifically
     * for descriptor file 'foo.yaml', and 'rep2.yaml' contains those
     * specifically for descriptor file 'bar.yaml'
     */
    std::unordered_map<std::string, app::ReportYamlReplacements>
        report_specific_yaml_placeholder_replacements_;

    /*!
     * \brief Have the command line options been parsed yet by this class
     * through the parse method
     */
    bool is_parsed_ = false;

    /*!
     * \brief Has the simulator been setup through the populateSimulation method
     */
    bool is_setup_ = false;

    /*!
     * \brief Run-time user parameter
     */
    uint64_t run_time_cycles_ = sparta::Scheduler::INDEFINITE;
    uint64_t run_time_ticks_  = sparta::Scheduler::INDEFINITE;

    /*!
     * \brief Is this simulator in no-run mode where it quits just before
     * finalization
     */
    bool no_run_mode_ = false;

    /*!
     * \brief Destination to which final configuration (before running) will be
     * written ("" if not written)
     */
    std::string final_config_file_;

    /*!
     * \brief The file to read from for reading in a final config file
     */
    std::string read_final_config_ = "";
    /*!
     * \brief number of non-final configuration applications used to modify parameters.
     * A tally of all -p, --arch, --config-file. Does not include --read-final-config
     */
    uint32_t config_applicators_used_ = 0;

    /*!
     * \brief Destination to which power configuration (to be read by power model) will be
     * written ("" if not written)
     */
    std::string power_config_file_;

    /*!
     * \brief Destination to which final configuration (before running) will be
     * written ("" if not written). This file will also include descriptions of
     * each parameter preceeding them with extra newlines
     */
    std::string final_config_file_verbose_;

    /*!
     * \brief Should the trivialities of simulator configuration (e.g what
     * command line options were specified) be hidden?
     */
    bool no_show_config_ = false;

    /*!
     * \brief Display the device tree at every opportunity
     */
    bool show_tree_ = false;

    /*!
     * \brief Display all parameters in the device tree after building
     */
    bool show_parameters_ = false;

    /*!
     * \brief Display all ports in the device tree after finalization
     */
    bool show_ports_ = false;

    /*!
     * \brief Display all counters and stats in the device tree after
     * finalization
     */
    bool show_counters_ = false;

    /*!
     * \brief Display all the clocks in the tree
     */
    bool show_clocks_ = false;

    /*!
     * \brief Display all the pevent types in the tree.
     */
    bool show_pevents_ = false;
    /*!
     * \brief Display all notifications in the device tree after finalization
     * excluding log messages
     */
    bool show_notifications_ = false;

    /*!
     * \brief Display all notifications in the device tree after finalization
     * excluding log messages
     */
    bool show_loggers_ = false;

    /*!
     * \brief Show hidden treenodes when displaying the device tree
     */
    bool show_hidden_ = false;

    /*!
     * \brief Show hidden treenodes when displaying the device tree
     */
    bool disable_colors_ = false;

    /*!
     * \brief Under what conditions should the debug content be dumped at
     * simulator destruction. {always,never,error}
     */
    std::string dump_debug_type_{"error"};

    /*!
     * \brief When a simulation error occurs and error logging is enabled,
     * what content should the error log contain?
     *   - Assert message, backtrace, and tree (default)
     *   - Assert message, backtrace
     *   - Assert message only
     */
    std::string debug_dump_options_{"all"};

    /*!
     * \brief Instance of the pipeline collector if collection enabled
     */
    std::unique_ptr<sparta::trigger::Triggerable> pipeline_collection_triggerable_;
    std::unique_ptr<sparta::trigger::Trigger>     pipeline_trigger_;
    std::unique_ptr<sparta::InformationWriter>    info_out_;

    /*!
     * \brief Heartbeat period of pipeline collection file (before lexical cast
     * or validation)
     */
    std::string pipeline_heartbeat_ = DefaultHeartbeat;

    //! The names of the nodes to be enabled
    std::set<std::string> pipeline_enabled_node_names_;

    /*!
     * \brief The filename to log pevents too.
     */
    std::unique_ptr<sparta::trigger::Triggerable> pevent_trigger_;

    /*!
     * \brief a pevent controller is used to parse in pevent on and off commands,
     * and prepare collection for the specified pevent types, this is different
     * than pevent trigger because the controller only preps collectors, the trigger
     * actually starts the trigger
     */
    pevents::PeventCollectorController pevent_controller_;
    bool run_pevents_ = false;

    //! The runtime clock to use for -r option
    std::string runtime_clock_;

    /*!
     * \brief Automatic summary state. Determines what to do with the automatic
     * summary after running
     */
    std::string auto_summary_;

    /*!
     * \brief Help topic to show.
     */
    std::string help_topic_;

private:

    /*!
     * \brief Run with the Python shell (if support compiled in)
     */
    bool use_pyshell_ = false;

    /*!
     * \brief Prints usage strings to cerr
     */
    void printUsageHelp_() const;

    /*!
     * \brief Prints program options documentation to cerr at a particular level
     */
    void printOptionsHelp_(uint32_t level) const;

    /*!
     * \brief Prints verbose help to cerr
     */
    void showVerboseHelp_() const;

    /*!
     * \brief Prints brief help to cerr
     */
    void showBriefHelp_() const;

    /*!
     * \brief Prints list of help topics to cerr
     */
    void showHelpTopics_() const;

    /*!
     * \brief Open and parse a given ALF file for pipeline node
     * restriction (part of --argos-collection-at command line)
     * \return true if all good; false otherwise
     */
    bool openALFAndFindPipelineNodes_(const std::string & alf_filename);

    /*!
     * \brief Builtin sparta command line options
     */
    MultiDetailOptions sparta_opts_;

    /*!
     * \brief Builtin sparta command line options
     */
    MultiDetailOptions param_opts_;

    /*!
     * \brief Builtin sparta command line options for debug
     */
    MultiDetailOptions debug_opts_;

    /*!
     * \brief Builtin sparta command line options for runtime
     */
    MultiDetailOptions run_time_opts_;

    /*!
     * \brief Builtin sparta command line options
     */
    MultiDetailOptions pipeout_opts_;

    /*!
     * \brief Builtin sparta command line options
     */
    MultiDetailOptions log_opts_;

    /*!
     * \brief Builtin sparta command line options
     */
    MultiDetailOptions report_opts_;

    /*!
     * \brief Builtin sparta command line options for SimDB
     */
    MultiDetailOptions simdb_opts_;

    /*!
     * \brief Builtin sparta command line options for SimDB
     * (internal / developer use only - not visible to
     * command line help printout)
     */
    MultiDetailOptions simdb_internal_opts_;

    /*!
     * \brief Application-specific options
     * \see getApplicationOptions
     */
    MultiDetailOptions app_opts_;

    /*!
     * \brief Feature options. These are not displayed to
     * users in the command-line "help" text.
     */
    MultiDetailOptions feature_opts_;

    /*!
     * \brief Advanced optiosn
     * \see getAdvancedOptions
     */
    MultiDetailOptions advanced_opts_;

    /*!
     * \brief All options (merged sparta_opts_ and app_opts_)
     */
    po::options_description all_opts_;

    /*!
     * \brief
     */
    po::positional_options_description positional_opts_;

    /*!
     * \brief Variables map populated by command-line parsing
     */
    po::variables_map vm_;

    /*!
     * \brief Any unrecognized options encountered on the command line. Some of
     * these will be overlayed on the parameter tree
     */
    std::vector<std::string> unrecognized_opts_;

    /*!
     * \brief A trigger used to turn on debug useful options
     * at a given cycle, right now this only turns on pipeline
     * collection
     */
    std::unique_ptr<trigger::Trigger> debug_trigger_;

};

    } // namespace app
} // namespace sparta

#endif // #ifndef __COMMAND_LINE_SIMULATOR_H__
