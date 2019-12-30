// <ArchiveController> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_ARCHIVE_CONTROLLER_H__
#define __SPARTA_STATISTICS_ARCHIVE_CONTROLLER_H__

#include <string>

namespace sparta {
namespace statistics {

class ReportStatisticsArchive;

/*!
 * \brief Base class used by the RootArchiveNode to have
 * some control over its data source, whether it is a live
 * simulation, or an offline archive (no simulation)
 */
class ArchiveController
{
public:
    virtual ~ArchiveController() {}

    //! Some archive sources/sinks may buffer data or perform
    //! asynchronous operations, which can lead to non-deterministic
    //! behavior when accessing data. For example, ofstream buffers
    //! that have not been flushed will appear to be missing data
    //! in the archive. A synchronization in that case would flush
    //! the ofstream's. Other implementations may have different
    //! notions of synchronization.
    virtual bool synchronize() = 0;

    //! Save (or re-save) the entire archive to a new directory.
    //! This does not simply point the archive streams to put
    //! new data in this directory; the archives will still
    //! be putting data into the original directory (such as
    //! the temp dir), whereas calling saveTo() is a deep copy
    //! of whatever is currently archived.
    virtual void saveTo(const std::string & dir) = 0;
};

/*!
 * \brief Controller used when SPARTA simulations are directly
 * feeding data into a tempdir archive
 */
class LiveSimulationArchiveController : public ArchiveController
{
public:
    explicit LiveSimulationArchiveController(ReportStatisticsArchive * live_archive) :
        live_archive_(live_archive)
    {}

    //! Live simulations' data sources buffer data into an
    //! ofstream. Calling this method will flush those buffers
    //! to disk.
    bool synchronize() override;

    //! Make a deep copy of the current archived data into
    //! a new directory.
    void saveTo(const std::string & dir) override;

private:
    ReportStatisticsArchive * live_archive_ = nullptr;
};

/*!
 * \brief Controller used when attaching to an archive outside
 * of any live simulation
 */
class OfflineArchiveController : public ArchiveController
{
public:
    explicit OfflineArchiveController(const std::string & source_archive_dir) :
        source_archive_dir_(source_archive_dir)
    {}

    //! Offline archives (outside of any simulation) are
    //! synchronous. This method does not have any effect.
    bool synchronize() override;

    //! Make a deep copy of the current archived data into
    //! a new directory.
    void saveTo(const std::string & dir) override;

private:
    std::string source_archive_dir_;
};

} // namespace statistics
} // namespace sparta

#endif
