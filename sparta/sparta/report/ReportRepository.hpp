// <ReportRepository> -*- C++ -*-

/*!
 * \file ReportRepository.hpp
 *
 * \brief Each simulation may have up to one report repository. Reports are
 * organized into directories, where all reports in the same directory have
 * the exact same:
 *
 *      Definition file         (.yaml)
 *      Destination file        (.txt, .html, ...)
 *      Location pattern        ("_global", "top.core1", ...)
 *      Format                  (optional)
 *
 *      Report start time               (optional expression)
 *      Report stop time                (optional expression)
 *      Report update period            (optional expression)
 *
 * All of this information, optional or not, is in the app::ReportDescriptor
 * class. So in order to checkout a new directory, you must hand over a
 * ReportDescriptor object to the repository. The returned directory handle
 * can be used with other repository methods to add reports, commit them, etc.
 *
 * Any resources needing to be instantiated (such as triggers) will only occur
 * when you commit a directory to the repository.
 *
 * The ReportDescriptor you give the repository when you check out a new directory
 * must have ZERO report instantiations already in it.
 *
 * There is an API that lets you take back all committed reports for any reason at any
 * time, even during simulation. See "ReportRepository::saveReports()" below.
 *
 * However, note that there is no real reason you *must* release the reports. They
 * will start, update, and stop on their own according to the directory configuration
 * (from each directory's app::ReportDescriptor), and will automatically be saved to file
 * at the end of simulation. Releasing reports could be used during exception handling,
 * post-processing, etc. but most of the time, you should not need to release them at all.
 */

#ifndef __REPORT_REPOSITORY_H__
#define __REPORT_REPOSITORY_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sparta::app {
    class ReportDescriptor;
} /* namespace sparta */

namespace sparta::app {
class Simulation;
}  // namespace sparta::app

namespace sparta::report::format {
class BaseFormatter;
}  // namespace sparta::report::format

namespace sparta {
class Report;
class TreeNode;

namespace statistics {
    class StatisticsArchives;
    class StatisticsStreams;
    class StreamController;
}
namespace app {
    class FeatureConfiguration;
}

class ReportRepository
{
public:
    explicit ReportRepository(app::Simulation * sim);
    explicit ReportRepository(TreeNode * context);
    ~ReportRepository() = default;

    typedef void * DirectoryHandle;

    /*!
     * \brief Create a directory from the given report descriptor.
     * \return Handle to the newly created directory.
     */
    DirectoryHandle createDirectory(
        const app::ReportDescriptor & desc);

    /*!
     * \brief Add a report to the given directory.
     */
    void addReport(
        DirectoryHandle handle,
        std::unique_ptr<Report> report);

    /*!
     * \brief When done adding reports, commit a directory into the repository.
     * \return This method returns TRUE if successful, FALSE otherwise.
     * \note In the event of a failed commit, the directory handle will be
     * invalidated. Attempting to use it again with any repository API will
     * issue an exception.
     */
    bool commit(DirectoryHandle * handle);

    /*!
     * \brief Let the repository know that the tree has been built but not
     * yet completely finalized
     */
    void finalize();

    /*!
     * \brief Give the repository a chance to see the --feature values
     * that were set at the command line, if any. This is called just
     * prior to the main simulation loop.
     */
    void inspectSimulatorFeatureValues(
        const app::FeatureConfiguration * feature_config);

    /*!
     * \brief Get the statistics archives for all reports in this simulation
     */
    statistics::StatisticsArchives * getStatsArchives();

    /*!
     * \brief Get the statistics streams for all reports in this simulation
     */
    statistics::StatisticsStreams * getStatsStreams();

    /*!
     * \brief Share the descriptors' BaseFormatter's with the reporting
     * infrastructure. These formatters need to coordinate with the SimDB
     * serializers and the report verification post processing step.
     */
    std::map<std::string, std::shared_ptr<report::format::BaseFormatter>>
        getFormattersByFilename() const;

    /*!
     * \brief Save reports and release them back to the simulation object that
     * owns this repository. After calling this method, the repository is empty.
     *
     * \note Even without explicitly calling this method, reports will be saved
     * anyway. Calling this method should only be considered for things such as
     * exception handling, post-processing, etc. but most of the time you don't
     * need to call this method at all.
     *
     * \note The report repository is meant to keep all triggered behavior contained
     * "behind closed doors" during the simulation. Calling this method *during simulation*
     * (while in the Scheduler's normal sim loop) will kill any triggers associated with
     * all reports, never to be invoked again! The reports will still be intact and valid,
     * but - from a reports perspective - the simulation is OVER.
     *
     * \return All the reports in this repository.
     */
    std::vector<std::unique_ptr<Report>> saveReports();

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace sparta

// __REPORT_REPOSITORY_H__
#endif
