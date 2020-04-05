// <StatisticsStreams> -*- C++ -*-

#pragma once

#include "sparta/statistics/dispatch/StatisticsHierRootNodes.hpp"
#include "sparta/statistics/dispatch/streams/StreamNode.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief Wrapper around the StatisticsHierRootNodes<T> class.
 * This class holds onto root StreamNode's that sit at the top
 * of a report hierarchy. Say we have this hierarchy of two
 * reports:
 *
 *     foo_csv              <-- root StreamNode* at 0xA
 *       top
 *         core0
 *           ...
 *     bar_csv              <-- root StreamNode* at 0xB
 *       rob
 *         ipc
 *
 * So the StatisticsStreams object would have two things in it:
 *
 *     [ "foo_csv" -> 0xA ],
 *     [ "bar_csv" -> 0xB ]
 */
class StatisticsStreams
{
public:
    //! Add a root StreamNode by name. This will throw if there
    //! is already a root node by that name in this collection.
    //! Call the 'getRootByName()' to see if a root already
    //! exists by a given name - it will return null if not.
    void addHierarchyRoot(const std::string & storage_name,
                          std::shared_ptr<StreamNode> & root)
    {
        roots_.addHierarchyRoot(storage_name, root);
    }

    //! Returns a list of the names of the root StreamNode's
    //! in this collection, sorted alphabetically (A -> Z)
    std::vector<std::string> getRootNames() const
    {
        return roots_.getRootNames();
    }

    //! Maintain a mapping from report filenames like 'out.csv' to the
    //! equivalent root name like 'out_csv'. This is to support tab
    //! completion for Python shell users (Python won't allow dots in
    //! node names).
    void mapRootNameToReportFilename(const std::string & root_name,
                                     const std::string & report_filename) const
    {
        roots_.mapRootNameToReportFilename(root_name, report_filename);
    }

    //! Ask for a hierarchy root node by name. The name should be
    //! one that you originally gave to addHierarchyRoot(), or this
    //! method will return null.
    StreamNode * getRootByName(const std::string & root_name)
    {
        return roots_.getRootByName(root_name);
    }

private:
    mutable StatisticsHierRootNodes<StreamNode> roots_;
};

} // namespace statistics
} // namespace sparta

