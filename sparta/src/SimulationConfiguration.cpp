// <EmbeddedSimulation> -*- C++ -*-


/*!
 * \file SimulationConfiguration.cpp
 * \brief Implementation for EmbeddedSimulation
 */

#include "sparta/app/SimulationConfiguration.hpp"

#include <cstddef>
#include <iostream>

#include "sparta/utils/File.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta {
namespace app {

    SimulationConfiguration::SimulationConfiguration(const DefaultValues & defaults) :
        defaults_(defaults),
        trigger_clock(defaults_.run_time_clock),
        arch_search_paths_(defaults_.arch_search_dirs)
    { }

    //! Handle an individual parameter
    void SimulationConfiguration::processParameter(const std::string & pattern,
                                                   const std::string & value,
                                                   bool optional)
    {
        sparta_assert(!is_consumed_, "You cannot process parameters after simulation has been populated");
        config_applicators_.
            emplace_back(new ParameterApplicator(pattern, value,
                                                 (optional ?
                                                  ConfigApplicator::ApplySuccessCondition::ASC_IGNORE :
                                                  ConfigApplicator::ApplySuccessCondition::ASC_MUST_ASSIGN)));
        config_applicators_.back()->applyUnbound(ptree_, verbose_cfg);
        std::cout << "  [in] Configuration: " << config_applicators_.back()->stringize() << std::endl;
    }

    //! Consume a configuration (.yaml) file
    void SimulationConfiguration::processConfigFile(const std::string & pattern,
                                                    const std::string & filename,
                                                    bool final)
    {
        sparta_assert(!is_consumed_, "You cannot process config files after simulation has been populated");
        config_applicators_.emplace_back(new NodeConfigFileApplicator(pattern, filename, config_search_paths_));
        config_applicators_.back()->applyUnbound(ptree_, verbose_cfg);
        std::cout << "  [in] Configuration: " << config_applicators_.back()->stringize() << std::endl;
        if(final) {
            final_config_file_ = filename;
        }
    }

    //! Configure simulator for specific architecture
    void SimulationConfiguration::processArch(const std::string & pattern,
                                              const std::string & filename)
    {
        sparta_assert(arch_applicator_ == nullptr, "Cannot specify more than one arch option");
        sparta_assert(!is_consumed_, "You cannot process arch files after simulation has been populated");
        std::string found_filename = utils::findArchitectureConfigFile(arch_search_paths_, filename);
        addRunMetadata("arch", filename);
        arch_applicator_.reset(new ArchNodeConfigFileApplicator(pattern, found_filename, arch_search_paths_));
        arch_applicator_->applyUnbound(arch_ptree_, verbose_cfg);
        std::cout << "  [in] Arch Config: " << arch_applicator_->stringize() << std::endl;
    }

    //! Enable logging on a specific node, for a specific category,
    //! and redirect output to the given destination
    void SimulationConfiguration::enableLogging(const std::string & pattern,
                                                const std::string & category,
                                                const std::string & destination)
    {
        taps_.emplace_back(pattern, category, destination);
    }

    void SimulationConfiguration::setStateTrackingFile(const std::string & filename)
    {
        sparta_assert(!is_consumed_, "You cannot set state tracking files after simulation has been populated");
        sparta_assert(state_tracking_file_.empty() || state_tracking_file_ == filename);
        state_tracking_file_ = filename;
    }

    const std::string& SimulationConfiguration::getStateTrackingFilename() const {
        return state_tracking_file_;
    }

    //! Consume a simulation control file
    void SimulationConfiguration::addControlFile(const std::string & filename)
    {
        sparta_assert(!is_consumed_, "You cannot process simulation control files "
                    "after simulation has been populated");
        simulation_control_filenames_.insert(filename);
    }

    //! Get all control files for this simulation
    const std::set<std::string> & SimulationConfiguration::getControlFiles() const
    {
        return simulation_control_filenames_;
    }

    //! Add run metadata as a string
    void SimulationConfiguration::addRunMetadata(const std::string & name,
                                                 const std::string & value)
    {
        run_metadata_.push_back(std::make_pair(name, value));
    }

    //! Get run metadata as key-value pairs
    const std::vector<std::pair<std::string, std::string>> &
        SimulationConfiguration::getRunMetadata() const
    {
        return run_metadata_;
    }

    //! Put all run metadata (key-value pairs) into one string
    std::string SimulationConfiguration::stringizeRunMetadata() const
    {
        std::set<std::string> md_names;
        std::ostringstream oss;

        for (size_t idx = 0; idx < run_metadata_.size(); ++idx) {
            const auto & md = run_metadata_[idx];
            const std::string & name = md.first;
            const std::string & value = md.second;

            if (!md_names.insert(name).second) {
                throw SpartaException("Duplicate metadata found (") << name << ")";
            }

            oss << name << "=" << value;
            if (run_metadata_.size() > 1 && idx != run_metadata_.size() - 1) {
                oss << ",";
            }
        }

        return oss.str();
    }

    //! Set filename which contains heap profiler settings
    void SimulationConfiguration::setMemoryUsageDefFile(const std::string & def_file)
    {
        memory_usage_def_file_ = def_file;
    }

    //! Get filename for heap profiler configuration
    const std::string & SimulationConfiguration::getMemoryUsageDefFile() const
    {
        return memory_usage_def_file_;
    }

    //! Auto-generate mappings from report column headers to statistic names
    void SimulationConfiguration::generateStatsMapping()
    {
        generate_stats_mapping_ = true;
    }

    //! Get statistics mapping "enabled" flag
    bool SimulationConfiguration::shouldGenerateStatsMapping() const
    {
        return generate_stats_mapping_;
    }

    //! Disable pretty printing for the given file format ("json", etc.) if the
    //! format differentiates between pretty and normal printing
    void SimulationConfiguration::disablePrettyPrintReports(const std::string & format)
    {
        sparta_assert(format.size() > 1);
        const std::string format_ = format.at(0) != '.' ? format : format.substr(1);
        disabled_pretty_print_report_formats_.insert(format_);
        disabled_pretty_print_report_formats_.insert("." + format_);
    }

    //! Get all report file extensions which have had their pretty printing disabled
    const std::set<std::string> & SimulationConfiguration::getDisabledPrettyPrintFormats() const
    {
        return disabled_pretty_print_report_formats_;
    }

    //! Specify that a given report format is to omit StatisticInstance's
    //! that have a value of zero
    void SimulationConfiguration::omitStatsWithValueZeroForReportFormat(
        const std::string & format)
    {
        zero_values_omitted_report_formats_.insert(format);
    }

    //! Get all report formats which are to omit statistics that have value 0
    const std::set<utils::lowercase_string> &
        SimulationConfiguration::getReportFormatsWhoOmitStatsWithValueZero() const
    {
        return zero_values_omitted_report_formats_;
    }

    //! Local helper method that recurses down through a ParameterTree,
    //! starting at a particular node in that tree, and collects all
    //! nodes that it finds during the traversal whose hasValue()==true
    void recursFindPTreeNodesWithValue(
        const ParameterTree::Node * this_node,
        std::vector<const ParameterTree::Node*> & has_value_nodes)
    {
        if (this_node == nullptr) {
            return;
        }
        if (this_node->hasValue()) {
            has_value_nodes.emplace_back(this_node);
        }
        const auto children = this_node->getChildren();
        for (const ParameterTree::Node * child : children) {
            recursFindPTreeNodesWithValue(child, has_value_nodes);
        }
    }

} // namespace app
} // namespace sparta
