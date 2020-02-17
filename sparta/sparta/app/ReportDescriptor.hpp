
/**
 * \file   ReportDescriptor.hpp
 * \brief  Describes reports to instantiate and tracks their instantiations
 */

#ifndef __REPORT_DESCRIPTOR_H__
#define __REPORT_DESCRIPTOR_H__


#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/any.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/report/format/BaseFormatter.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "simdb_fwd.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace simdb {
class AsyncTaskEval;
class ObjectManager;
}  // namespace simdb

namespace sparta::app {
class SimulationConfiguration;
}  // namespace sparta::app

namespace sparta::trigger {
class SkippedAnnotatorBase;
}  // namespace sparta::trigger

namespace sparta {

namespace report {
    namespace format {
        class BaseFormatter;
        class FormatterFactory;
    }
}

class Report;
class RootTreeNode;
class Scheduler;
class TreeNode;

namespace statistics {
    class ReportStatisticsArchive;
    class StreamNode;
}

namespace async {
    class AsyncTimeseriesReport;
    class AsyncNonTimeseriesReport;
}
namespace db {
    class ReportHeader;
}

namespace app {

class Simulation;

typedef std::unordered_map<std::string, boost::any> NamedExtensions;
typedef std::unordered_map<std::string, std::string> TriggerKeyValues;

/*!
 * \brief Describes one or more report to instantiate
 */
class ReportDescriptor
{
    //! \brief Instantiation type
    typedef std::pair<Report*, report::format::BaseFormatter*> inst_t;

    /*!
     * \brief Set of reports with triggered behavior (these are NOT
     * separate report instantiations; the ReportDescriptor just needs
     * to track triggered vs. non-triggered reports internally)
     */
    std::set<const Report*> triggered_reports_;

    /*!
     * \brief Set of "idle" reports. These reports said "no" when last
     * asked if they were active or not. Used for forced flushing when
     * this object gets destroyed and there are still pending writes.
     *
     * Same as with the triggered_reports_ variable above, these are not
     * separate reports. This is internal bookkeeping only.
     */
    std::set<const Report*> idle_reports_;

    /*!
     * \brief Cached report formatters
     */
    std::map<std::string, std::shared_ptr<report::format::BaseFormatter>> formatters_;

    /*!
     * \brief Aggregated statistics archive which stores all of this
     * report's statistics values in binary format
     */
    std::shared_ptr<statistics::ReportStatisticsArchive> report_archive_;

    /*!
     * \brief Root node of the stream tree if this descriptor is
     * configured to stream report statistics data out to listeners
     */
    std::shared_ptr<statistics::StreamNode> streaming_stats_root_;

    /*!
     * \brief Ask a report if it is currently active for writes and
     * updates. Regardless of answer, the internals of the report
     * descriptor will track if a forced flush is needed prior to
     * destruction and will take care of it.
     */
    bool updateReportActiveState_(const Report * r);

    /*!
     * \brief Reports instantiated based on this descriptor
     * \note These reports are not owned by this descriptor
     */
    std::vector<inst_t> instantiations_;

    /*!
     * \brief Formatter Factory for all reports generated from this
     * descriptor. Determined at construction. Do not delete, borrowed
     * pointer from sparta::report::format::BaseFormatter::FACTORY
     */
    const sparta::report::format::FormatterFactory* fact_;

    /*!
     * \brief Number of times this descriptor has had its instantiated reports
     * written
     */
    uint32_t writes_;

    /*!
     * \brief Number of times this descriptor has had its instantiated reports
     * updated
     */
    uint32_t updates_;

    /*!
     * \brief Utility class to track when (what tick) the last update occured
     */
    class DescUpdateTracker {
    public:
        void enable(const Scheduler * scheduler);
        bool checkIfDuplicateUpdate();
    private:
        utils::ValidValue<bool> enabled_;
        utils::ValidValue<uint64_t> last_update_at_tick_;
        const Scheduler * scheduler_ = nullptr;
    };

    /*!
     * \brief Track when (what tick) the last update occured (checking for
     * repeated tick updates / duplicates is disabled by default)
     */
    DescUpdateTracker update_tracker_;

    /*!
     * \brief This annotation object is used in conjunction with 'skipOutput()'
     * calls to print something to reports in place of skipped updates
     */
    std::shared_ptr<sparta::trigger::SkippedAnnotatorBase> skipped_annotator_;

    /*!
     * \brief Flag to early return (ignore) requests to update the report stats
     */
    bool report_stopped_ = false;

    /*!
     * \brief Flag telling the simulation whether to include this
     * descriptor for report generation or to skip us
     */
    bool enabled_ = true;

    /*!
     * \brief We hold onto our original dest_file that appeared in the
     * user's yaml file. This may be overwritten to point to another
     * directory if this simulation is using the database backend,
     * but we want to stringize 'this' descriptor using the user-
     * facing dest_file.
     */
    std::string orig_dest_file_;

    /*!
     * \brief Timeseries object which sends this report's metadata
     * and SI data values to a database.
     *
     * Use of this database is featured off by default for now.
     */
    std::shared_ptr<async::AsyncTimeseriesReport> db_timeseries_;

    /*!
     * \brief Wrapper which writes non-timeseries SI data values
     * into a database.
     *
     * Use of this database is featured off by default for now.
     */
    std::shared_ptr<async::AsyncNonTimeseriesReport> db_non_timeseries_;

    /*!
     * \brief The "simdb" feature has a number of modes it can run:
     *
     *     1. SI compression disabled, row-major SI ordering
     *     2. SI compression disabled, column-major SI ordering
     *     3. SI compression enabled, row-major SI ordering
     *     4. SI compression enabled, column-major SI ordering
     *
     * If the feature is enabled, feature options may be given
     * at the command line. These are optional however, so we
     * will default to compressed, row-major SI blobs in the
     * absence of any such options.
     */
    const FeatureConfiguration::FeatureOptions * simdb_feature_opts_ = nullptr;

    friend class ReportDescriptorCollection;

public:

    /*!
     * \brief Global search scope node keyword for report locations
     */
    static const char* GLOBAL_KEYWORD;

    /*!
     * \brief Node location string (pattern) on which report should be
     * generated. Typically "" or top
     */
    std::string loc_pattern;

    /*!
     * \brief Filename of report definition. If '@', auto generates reports
     * with all counters and stats of loc_pattern
     */
    std::string def_file;

    /*!
     * \brief Destination filename to which report will be written. Later
     * this will represent other types of destination besides files.
     */
    std::string dest_file;

    /*!
     * \brief Optional formatting string (how to write to the file). This is
     * converted to lower-case at construction
     */
    std::string format;

    /*!
     * \brief Key-value extensions used by parsers to bind opaque report
     * configurations to descriptors.
     */
    NamedExtensions extensions_;

    /*!
     * \brief Calling this method causes the simulation to skip this
     * descriptor when it is setting up its reports. This operation
     * cannot be undone.
     */
    void disable() {
        enabled_ = false;
    }

    /*!
     * \brief See if this descriptor is enabled or not. Disabling a
     * descriptor means that it will be filtered from report generation.
     */
    bool isEnabled() const {
        return enabled_;
    }

    /*!
     * \brief Check if this descriptor holds only one report
     * instantiation, and that it is a timeseries report (.csv)
     */
    bool isSingleTimeseriesReport() const;

    /*!
     * \brief Check if this descriptor holds only one report
     * instantiation, and that it is *not* a timeseries report.
     * For example, .html, .json, .txt, and so on.
     */
    bool isSingleNonTimeseriesReport() const;

    /*!
     * \brief Switch this descriptor's timeseries report generation
     * from synchronous .csv generation to asynchronous database
     * persistence.
     *
     * The task queue object passed in is the worker thread object,
     * which is shared among all report descriptors in the simulation.
     *
     * The simulation database passed in is the object with the
     * actual connection to the physical database. This object
     * is shared with the Simulation class and other descriptors.
     *
     * The Scheduler passed in is the one our simulation is running on,
     * and the Clock passed in is the simulation's root clock. Both
     * of these objects are used in order to get the "current time"
     * value at each report update (current cycle, simulated time,
     * etc.)
     */
    void configureAsyncTimeseriesReport(
        simdb::AsyncTaskEval * task_queue,
        simdb::ObjectManager * sim_db,
        const Clock & root_clk);

    /*!
     * \brief Switch this descriptor's report generation from
     * synchronous to asynchronous database persistence. This
     * method is intended only for descriptors that have just
     * one *non-timeseries* report format.
     *
     * Call "isSingleNonTimeseriesReport()" first before calling
     * this method to be sure, otherwise this method may throw.
     */
    void configureAsyncNonTimeseriesReport(
        simdb::AsyncTaskEval * task_queue,
        simdb::ObjectManager * sim_db);

    /*!
     * \brief Give access to the database timeseries header. This
     * will return null when this descriptor is used for any non-
     * timeseries report format (json, json_reduced, txt, etc.)
     * or when the "simdb" feature has been disabled.
     */
    db::ReportHeader * getTimeseriesDatabaseHeader();

    /*!
     * \brief Do any post-simulation post processing steps needed.
     * This is typically used for final wrap-up this descriptor
     * needs to do in the simulation database, so the two inputs
     * are the SimDB and the AsyncTaskEval objects that belong
     * to the app::Simulation and sparta::ReportRepository objects,
     * but other non-database work may need to be completed post-
     * simulation as well.
     */
    void doPostProcessing(
        simdb::AsyncTaskEval * task_queue,
        simdb::ObjectManager * sim_db);

    /*!
     * \brief Provide access to the formatters we have been using
     * so they can coordinate with the reporting infrastructure to
     * write various metadata to the database.
     */
    std::map<std::string, std::shared_ptr<
        report::format::BaseFormatter>> getFormattersByFilename() const;

    /*!
     * \brief Determines if format is a valid name for formatter. This
     * allows the command line parser to disregard tokens at the end of a
     * report command which are not formatters and instead interpret them as
     * positional arguments
     */
    static bool isValidFormatName(const std::string& format);

    /*!
     * \brief Construct a report decsriptor
     * \param _loc_pattern Location patern identifying 1 or more nodes on
     * which to construct a report
     * \param _def_file Report definition file path relative to current
     * working directory. Note, this may also be '@' to generate reports
     * which each contain all counters and stats in the subtree(s) of the
     * node(s) described by _loc_pattern.
     * \param _dest_file File to which these reports should be written. This
     * may contain report name wildcards to identify output files based on
     * replacements made in \a _loc_pattern. Output formatter is chosen by
     * the file extension of this path unless explicitly set in \a _format.
     * If sparta::utils::COUT_FILENAME, will write to stdout. If
     * sparta::utils::CERR_FILENAME, will write
     * to stderr. Note that A formatter which supports ostream writing must
     * be chosen to support this or an exception will be thrown
     * \param _format Optional explicit format specificier. Extensions
     * allowed are all those defined in
     * sparta::report::format::BaseFormatter::FACTORIES. If omitted, deduces
     * the format based on _dest_file.
     * \throw SpartaException if the _dest_file is 1 or 2 and the formatter
     * chosen (by _format if specified or the secondarily _dest_file) is
     * not an ostream formatter (is not a
     * sparta::report::format::BaseOstreamFormatter). This is tested in
     * construction so that no time is wasted on failure - as opposed to
     * checkout formatters when finally writing reports
     */
    ReportDescriptor(const std::string& _loc_pattern,
                     const std::string& _def_file,
                     const std::string& _dest_file,
                     const std::string& _format="text");

    //! \brief Allow construction
    ReportDescriptor(const ReportDescriptor&) = default;

    //! \brief Allow assignment
    ReportDescriptor& operator=(const ReportDescriptor&) = default;

    //! \brief Destructor (note that triggered reports are automatically flushed)
    ~ReportDescriptor();

    //! \brief Getter for this descriptor's pattern, e.g. "_global"
    const std::string & getDescriptorPattern() const {
        return loc_pattern;
    }

    //! \brief Getter for this descriptor's def_file, e.g. "simple_stats.yaml"
    const std::string & getDescriptorDefFile() const {
        return def_file;
    }

    //! \brief Getter for this descriptor's dest_file, e.g. "out.json"
    const std::string & getDescriptorDestFile() const {
        return dest_file;
    }

    //! \brief Getter for this descriptor's format, e.g. "json_reduced"
    const std::string & getDescriptorFormat() const {
        return format;
    }

    //! \brief When SimDB has automatic report verification enabled,
    //! this descriptor may have had its dest_file changed when the
    //! Simulation::setupReports() method was called. The report file
    //! will still end up in the dest_file that you gave the descriptor,
    //! but this getter is added if you need to ask this descriptor
    //! what its immutable dest_file was from the beginning.
    const std::string & getDescriptorOrigDestFile() const {
        return orig_dest_file_;
    }

    /*!
     * \brief Tell the descriptor to send all of its writeOutput / updateOutput
     * statistics values to a binary archive. Returns the archive object that
     * will be responsible for feeding the data into the archive. Returns null
     * if unable to connect to the database.
     */
    std::shared_ptr<statistics::ReportStatisticsArchive> logOutputValuesToArchive(
        const std::string & dir);

    /*!
     * \brief Create and return a StreamNode object that sits at the top of
     * a tree hierarchy that describes the Report/Subreport/StatisticInstance
     * layout of this descriptor's report.
     */
    std::shared_ptr<statistics::StreamNode> createRootStatisticsStream();

    //! \brief Give the descriptor a chance to see the --feature values
    //! that were set at the command line, if any. This is called just
    //! prior to the main simulation loop.
    void inspectSimulatorFeatureValues(
        const app::FeatureConfiguration * feature_config);

    //! \brief Report descriptors may be triggered to stop early - ensure no
    //! further updates are written to disk
    void ignoreFurtherUpdates() {
        report_stopped_ = true;
    }

    /*!
     * \brief Represents this descriptor as a string
     */
    std::string stringize() const {
        std::stringstream ss;
        ss << "Report def \"" << def_file << "\" on node \"" << loc_pattern
           << "\" -> \"" << (orig_dest_file_.empty() ? dest_file : orig_dest_file_) << "\"";
        if(format.size() != 0){
            ss << " (format=" << format << ")";
        }
        return ss.str();
    }

    /*!
     * \brief Tracks a report instantiated based on this descriptor and
     * allocates a new formatter for it. Later, these tracked reports can be
     * iterated and saved to different destinations (or
     * the same) depending on their order.
     * \param r Report to track. Caller maintains ownership of this. Must
     * outlive this instance of ReportDescriptor. Must be fully populated
     * because the formatter created by this fuction may be written to
     * immediately
     * \param fmt The output formatter associated with this report. Caller
     * maintains ownership of this. Must outlive this instance of
     * ReportDescriptor
     * \param out Output stream to write actions to. If nullptr, no actions are
     * written
     * \return Newly allocated BaseFormatter. Caller is resposible for
     * deallocation. Allocated through
     * sparta::report::format::FormatterFactory::factory method which allocates
     * using 'new'. This new formatter will read the input report and be
     * initialized based on its content. The header of the report may be written
     * to the formatter immediately
     */
    report::format::BaseFormatter* addInstantiation(Report* r,
                                                    Simulation* sim,
                                                    std::ostream* out=nullptr);

    /*!
     * \brief Returns the vector of instantiated reports based on this
     * descriptor
     */
    std::vector<inst_t> getInstantiations() const {
        return instantiations_;
    }

    /*!
     * \brief Returns a vector of reports that have not been instantiated
     * yet, but will be when this report descriptor's start trigger fires.
     */
    std::vector<Report*> getPendingInstantiations() const {
        auto iter = extensions_.find("pending-reports");
        if (iter != extensions_.end()) {
            const std::vector<Report*> & pending_reports =
                boost::any_cast<const std::vector<Report*>&>(iter->second);

            return pending_reports;
        }
        return {};
    }

    /*!
     * \brief Returns all report instantiations, including those already
     * instantiated (no report start trigger) and pending report instantiations
     * (they have a start trigger, and it hasn't fired yet)
     */
    std::vector<Report*> getAllInstantiations() const {
        std::vector<inst_t> current = getInstantiations();
        std::vector<Report*> pending = getPendingInstantiations();

        std::unordered_set<Report*> all(pending.begin(), pending.end());
        for (const auto cur : current) {
            all.insert(cur.first);
        }
        return std::vector<Report*>(all.begin(), all.end());
    }

    /*!
     * \brief Saves all of the instantiations whose formatters do not support
     * 'update' to their respective
     * destinations. Returns the number of reports written in full in this call
     * \param out Print reports being written to out unless nulltr
     */
    uint32_t writeOutput(std::ostream* out=nullptr);

    /*!
     * \brief Updates all of the instantiations whose formatters support
     * 'update', possibly by writing to the destinations. Returns the number of
     * reports updated in this call
     * \param out Print reports being written to out unless nulltr
     */
    uint32_t updateOutput(std::ostream* out=nullptr);

    /*!
     * \brief Let the descriptor know to skip over one update of data
     */
    void skipOutput();

    /*!
     * \brief Instruct this descriptor to automatically ignore any "duplicate"
     * updates that occur at the exact same tick
     */
    void capUpdatesToOncePerTick(const Scheduler * scheduler);

    /*!
     * \brief Give this descriptor a specific annotator subclass for printing
     * skipped update information to reports
     */
    void setSkippedAnnotator(std::shared_ptr<sparta::trigger::SkippedAnnotatorBase> annotator);

    /*!
     * \brief Clears all destination files that will be filled with
     * instances of this report descriptor.
     *
     * This is done at simulation startup so that reports are empty until
     * written. After simulation, reports are appended to these files
     */
    void clearDestinationFiles(const Simulation& sim);

    /*!
     * \brief Returns the usage count (incremented by addInstantiation)
     */
    uint32_t getUsageCount() const { return instantiations_.size(); }

    /*!
     * \brief Returns the number of writes done on this report descriptor's
     * instantiated report formatters
     */
    uint32_t getNumWrites() const { return writes_; }

    /*!
     * \brief Returns the number of updates done on this report descriptor's
     * instantiated report formatters
     */
    uint32_t getNumUpdates() const { return updates_; }

    /*!
     * \brief Computes the filename to which this reportdescriptor will be
     * saved using any necessary variables in the name based on its
     * instantiation context and \a sim_name
     * \param r Report whose filename should be computed.
     * \param idx Index of the report in this ReportDescriptor's instantiations
     * This is required to fill in wildcards in the report name where multiple
     * reports are generated from one descriptor. Each can have its own index in
     * its name.
     * \see computeOutputFilename
     * \note Does not attempt to open the filename
     */
    std::string computeFilename(const Report* r,
                                const std::string& sim_name,
                                uint32_t idx) const;
};

typedef std::vector<ReportDescriptor> ReportDescVec;
typedef std::vector<std::pair<std::string, std::string>> ReportYamlReplacements;

/*!
 * \brief This collection of ReportDescriptors is designed to
 * never deallocate memory once it has been allocated. References
 * returned by the 'getDescriptorByName()' method are guaranteed
 * to be valid for the life of this collection object.
 *
 * If you want to "remove" a descriptor from the collection, call
 * the 'removeDescriptorByName()' method.
 */
class ReportDescriptorCollection
{
public:
    ReportDescriptorCollection() = default;
    ~ReportDescriptorCollection() = default;

    ReportDescriptorCollection(const ReportDescriptorCollection &) = delete;
    ReportDescriptorCollection & operator=(const ReportDescriptorCollection &) = delete;

    //While not copyable, moves are enabled to support std::swap
    ReportDescriptorCollection(ReportDescriptorCollection &&) = default;
    ReportDescriptorCollection & operator=(ReportDescriptorCollection &&) = default;

    //! Add one report descriptor to this collection
    void push_back(const ReportDescriptor & rd) {
        //Internal book-keeping is only for Python shell workflows.
        //We also do not perform any book-keeping for descriptors
        //whose dest_file is "1" (print to stdout). That is the
        //only use case where duplicate dest_file's is actually
        //okay. We will add the descriptor to the collection, but
        //any attempts to access this descriptor later (i.e. to
        //change its parameters or to disable it from Python) will
        //throw an exception that the descriptor "1" cannot be found.
        if (rd.dest_file != "1") {
            const std::string desc_name = getDescriptorName_(rd);
            auto iter = indices_by_descriptor_name_.find(desc_name);
            if (iter != indices_by_descriptor_name_.end()) {
                //Descriptor found by this name. Throw if it is already
                //enabled. Users can only replace disabled descriptors.
                if (rep_descs_.at(iter->second).isEnabled()) {
                    throw SpartaException("Report descriptor named '") <<
                        desc_name << "' already exists in this configuration";
                }
            }
            indices_by_descriptor_name_[desc_name] = rep_descs_.size();
        }

        //This incoming 'rd' variable could be a reference to a
        //previously disabled descriptor:
        //
        //   >>> rd = report_config.descriptors.foo_csv
        //   >>> report_config.removeReport('foo_csv')
        //   >>> report_config.addReport(rd)
        //
        //This would still count as an enabled descriptor.
        rep_descs_.push_back(rd);
        rep_descs_.back().enabled_ = true;
    }

    //! Add one report descriptor to this collection
    template <class... Args>
    void emplace_back(Args&&... args) {
        ReportDescriptor rd(std::forward<Args>(args)...);
        push_back(rd);
    }

    //! Remove (disable) all descriptors from this collection
    void clear() {
        for (auto &rd : rep_descs_) {
            rd.disable();
        }
    }

    //! Get the number of enabled descriptors in this collection
    size_t size() const {
        size_t sz = 0;
        for (const auto & rep_desc : rep_descs_) {
            if (rep_desc.isEnabled()) {
                ++sz;
            }
        }
        return sz;
    }

    //! See if there are any enabled descriptors in this collection
    bool empty() const {
        return size() == 0;
    }

    //! See if the given report descriptor name (ReportDescriptor::dest_file)
    //! exists in this collection
    bool contains(const std::string & desc_name) const {
        auto iter = indices_by_descriptor_name_.find(desc_name);
        if (iter == indices_by_descriptor_name_.end()) {
            //The name may have been given to us with a period, not
            //an underscore, separating the file stem and the extension
            const auto replaced_desc_name = replaceDotsWithUnderscores_(desc_name);
            iter = indices_by_descriptor_name_.find(replaced_desc_name);
        }

        //If this name is not even found in our map, we do not
        //contain this descriptor
        if (iter == indices_by_descriptor_name_.end()) {
            return false;
        }

        //But even if the descriptor is physcially here, we have
        //to also ask it if is is still enabled
        return rep_descs_.at(iter->second).isEnabled();
    }

    //! Access a descriptor by the app::ReportDescriptor's "dest_file"
    ReportDescriptor & getDescriptorByName(const std::string & desc_name) {
        auto idx = getDescriptorIndexByName_(desc_name);
        return rep_descs_.at(idx);
    }

    //! Remove (disable) one descriptor whose "dest_file" matches
    //! the given "desc_name" argument
    void removeDescriptorByName(const std::string & desc_name) {
        getDescriptorByName(desc_name).disable();
    }

    //! Get all "dest_file" strings for the report descriptors
    //! that are enabled in this collection
    std::vector<std::string> getAllDescriptorNames() const {
        std::vector<std::string> names;
        for (const auto & rd : indices_by_descriptor_name_) {
            if (rep_descs_.at(rd.second).isEnabled()) {
                names.push_back(rd.first);
            }
        }
        return names;
    }

    //! Iterator access
    std::deque<ReportDescriptor>::iterator begin() {
        return rep_descs_.begin();
    }

    //! Iterator access
    std::deque<ReportDescriptor>::const_iterator begin() const {
        return rep_descs_.cbegin();
    }

    //! Iterator access
    std::deque<ReportDescriptor>::iterator end() {
        return rep_descs_.end();
    }

    //! Iterator access
    std::deque<ReportDescriptor>::const_iterator end() const {
        return rep_descs_.cend();
    }

private:
    std::string getDescriptorName_(const ReportDescriptor & rd) const {
        return replaceDotsWithUnderscores_(rd.dest_file);
    }

    std::string replaceDotsWithUnderscores_(const std::string & str) const {
        std::string replaced = str;
        boost::replace_all(replaced, ".", "_");
        return replaced;
    }

    size_t getDescriptorIndexByName_(const std::string & desc_name) const {
        auto iter = indices_by_descriptor_name_.find(desc_name);
        if (iter == indices_by_descriptor_name_.end()) {
            //The name may have been given to us with a period, not
            //an underscore, separating the file stem and the extension
            const auto replaced_desc_name = replaceDotsWithUnderscores_(desc_name);
            iter = indices_by_descriptor_name_.find(replaced_desc_name);

            //If it still is not found, error out
            if (iter == indices_by_descriptor_name_.end()) {
                throw SpartaException(
                    "No descriptor named '") << desc_name << "' exists";
            }
        }

        //The descriptor exists in this collection, but we still need
        //to throw if it has been disabled, since we are not supposed
        //to be using those descriptors for any reason anymore.
        if (!rep_descs_.at(iter->second).isEnabled()) {
            throw SpartaException("The descriptor named '") << desc_name
                << "' has already been disabled";
        }
        return iter->second;
    }

    std::deque<ReportDescriptor> rep_descs_;
    std::unordered_map<std::string, size_t> indices_by_descriptor_name_;
};

/*!
 * \brief Configuration applicator class that is used for
 * configuring a simulator's reports. Works in conjunction
 * with sparta::app::ReportDescriptorCollection.
 *
 * \note Once the simulation framework is finalized, attempting
 * to add or remove descriptors from the configuration will
 * throw an exception. You may only ask the report configuration
 * object to print its contents to stdout ("serialize to yaml" /
 * "show all report info") and call the CONST 'getDescriptors()'
 * method.
 */
class ReportConfiguration {
public:
    ReportConfiguration(SimulationConfiguration * sim_config,
                        ReportDescriptorCollection * collection,
                        RootTreeNode * root);

    ReportConfiguration() = delete;
    ~ReportConfiguration() = default;

    ReportConfiguration(const ReportConfiguration &) = delete;
    ReportConfiguration & operator=(const ReportConfiguration &) = delete;

    //! Add one report descriptor to this collection
    void addReport(const ReportDescriptor & rd);

    //! Parse the given yaml file relative to the simulation's
    //! root tree node, and add the parsed descriptors to this
    //! report collection
    void addReportsFromYaml(const std::string & yaml_file);

    //! Remove (filter) a report by the ReportDescriptor's
    //! "dest_file" name
    void removeReportByName(const std::string & rd_name);

    //! Set up memory usage report from a yaml file configuration
    //! (this is the same as --log-memory-usage <yaml file> at the
    //! command line). This yaml file will be parsed relative to the
    //! simulation's root tree node.
    void addMemoryReportsFromYaml(const std::string & yaml_file);

    //! Pretty print information about all app::ReportDescriptor's
    void showAllReportDescriptorInfo();

    //! Print out the YAML equivalent of all report descriptors in
    //! the simulation
    void serializeAllDescriptorsToYaml();

    //! Access the underlying report descriptors
    ReportDescriptorCollection * getDescriptors();

    //! Access the underlying report descriptors
    const ReportDescriptorCollection * getDescriptors() const;

private:
    //When using the Python shell, adding/removing reports
    //from the configuration will refresh the appropriate
    //variable(s) in the Python workspace
    void republishReportCollection_();

    //Remove report config variable from the Python namespace
    void finishPythonInteraction_();

    //Finalize any changes to the descriptors from here on out
    void disallowChangesToDescriptors_();

    //Give the Simulation base class access to the above private
    //methods related to Python shell interaction. Note that the
    //sim_config_, collection_, and root_ member variables are
    //const pointers, so even with friend access, the Simulation
    //class still won't be able to change those variables for any
    //reason.
    friend class Simulation;

    SimulationConfiguration *const sim_config_;
    ReportDescriptorCollection *const collection_;
    RootTreeNode *const root_;
    bool allow_descriptor_changes_ = true;
};

/*!
 * \brief Parse a YAML file containing key-value pairs into a
 * single ReportYamlReplacements data structure.
 */
ReportYamlReplacements createReplacementsFromYaml(
    const std::string & replacements_yaml);

/*!
 * \brief Given a multi-report definition YAML file, parse it out
 * into individual descriptors, one for each report defined in the
 * file
 */
ReportDescVec createDescriptorsFromFile(
    const std::string & def_file,
    TreeNode * context);

/*!
 * \brief This method is similar to "createDescriptorsFromFile()",
 * except that it can be used for report yaml files that have one
 * or more placeholders in them, like this:
 *
 *     content:
 *       report:
 *         pattern:   _global
 *         def_file:  simple_stats.yaml
 *         dest_file: %TRACENAME%.stats.json
 *         format:    json_reduced
 *         ...
 *
 * Where the TRACENAME placeholder is given its value at the command
 * prompt instead of hard-coded in the yaml file itself.
 *
 * This can be beneficial for users who have auto-generated template
 * yaml files that have placeholders like %TRACENAME%, which they can
 * reuse many times and just supply different placeholder values at
 * the command prompt. This will save disk space in those use cases
 * (one yaml definition file versus many thousands).
 */
ReportDescVec createDescriptorsFromFileWithPlaceholderReplacements(
    const std::string & def_file,
    TreeNode * context,
    const ReportYamlReplacements & placeholder_key_value_pairs);

/*!
 * \brief Given a multi-report definition string, parse it out
 * into individual descriptors
 */
ReportDescVec createDescriptorsFromDefinitionString(
    const std::string & def_string,
    TreeNode * context);

} // namespace app
} // namespace sparta

#endif // #ifndef __REPORT_DESCRIPTOR_H__
