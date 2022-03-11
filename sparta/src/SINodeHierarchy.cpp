// <SINodeHierarchy> -*- C++ -*-

#include "sparta/statistics/db/SINodeHierarchy.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/report/Report.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {
namespace statistics {

//! Create a database record in the SINodeHierarchy table.
//! This record describes one SI node and its place in the
//! larger report it belongs to.
//!
//! Call SINodeCreator::createLeafSINode() for StatisticInstance
//! nodes at the leaves of an SI/Report tree.
//!
//! Call SINodeCreator::createMidLevelSINode() for mid-level and
//! top-level nodes of an SI/Report tree.
class SINodeCreator
{
public:
    static std::unique_ptr<simdb::ObjectRef> createLeafSINode(
        const simdb::ObjectManager & obj_mgr,
        const simdb::DatabaseID timeseries_id,
        const simdb::DatabaseID parent_node_id,
        const std::string & si_node_name,
        int & si_relative_index)
    {
        auto leaf = SINodeCreator::createSINode_(
            obj_mgr, timeseries_id, parent_node_id,
            si_node_name, si_relative_index);

        ++si_relative_index;
        return leaf;
    }

    static std::unique_ptr<simdb::ObjectRef> createMidLevelSINode(
        const simdb::ObjectManager & obj_mgr,
        const simdb::DatabaseID timeseries_id,
        const simdb::DatabaseID parent_node_id,
        const std::string & si_node_name,
        const int si_relative_index)
    {
        return SINodeCreator::createSINode_(
            obj_mgr, timeseries_id, parent_node_id,
            si_node_name, si_relative_index);
    }

private:
    static std::unique_ptr<simdb::ObjectRef> createSINode_(
        const simdb::ObjectManager & obj_mgr,
        const simdb::DatabaseID timeseries_id,
        const simdb::DatabaseID parent_node_id,
        const std::string & si_node_name,
        const int si_relative_index)
    {
        std::unique_ptr<simdb::TableRef> si_node_tbl =
            obj_mgr.getTable("SINodeHierarchy");

        return si_node_tbl->createObjectWithArgs(
            "TimeseriesID", timeseries_id,
            "ParentNodeID", parent_node_id,
            "NodeName", si_node_name,
            "RelativeSIIndex", si_relative_index);
    }
};

//! Recursively create records in the SINodeHierarchy to
//! describe the overall SI tree in a report. This walks
//! the report's SI tree in a depth-first fashion.
void recursCreateSubreportSIHierarchy(
    const simdb::ObjectManager & obj_mgr,
    const simdb::ObjectRef & parent_hier_node,
    const simdb::DatabaseID timeseries_id,
    const Report & report,
    int & si_relative_index)
{
    const auto & stats = report.getStatistics();
    const auto & subreports = report.getSubreports();

    if (!stats.empty()) {
        for (const auto & stat : stats) {
            //Create leaf SI nodes for all StatisticInstance's
            //we encounter. For example, "top.core0.rob.ipc"
            const std::string name = !stat.first.empty() ?
                stat.first : stat.second.getLocation();

            SINodeCreator::createLeafSINode(
                obj_mgr, timeseries_id,
                parent_hier_node.getId(),
                name, si_relative_index);
        }
    }

    if (!subreports.empty()) {
        for (const auto & sr : subreports) {
            std::vector<std::string> dot_delimited;
            boost::split(dot_delimited, sr.getName(), boost::is_any_of("."));
            const std::string & name = dot_delimited.back();

            //Create another record in the SINodeHierarchy table
            //for this mid-level report/subreport node. For example,
            //"top.core0.rob"
            std::unique_ptr<simdb::ObjectRef> sr_node =
                SINodeCreator::createMidLevelSINode(
                    obj_mgr, timeseries_id,
                    parent_hier_node.getId(),
                    name, si_relative_index);

            //Recursively call this method to handle the next
            //subreport level of nodes.
            recursCreateSubreportSIHierarchy(
                obj_mgr, *sr_node, timeseries_id,
                sr, si_relative_index);
        }
    }
}

//! Implementation class for SINodeHierarchy. Populates the
//! database with metadata describing the overall SI tree
//! (names of nodes, how the nodes are grouped i.e. parent/
//! child, etc.)
class SINodeHierarchy::Impl
{
public:
    Impl(db::ReportTimeseries & db_timeseries,
         const Report & report) :
      db_timeseries_(db_timeseries),
      report_(report)
    {}

    //! Serialize our ReportTimeseries' SI tree into the
    //! provided ObjectManager.
    simdb::DatabaseID serializeHierarchy(simdb::ObjectManager & obj_mgr)
    {
        simdb::DatabaseID root_report_id = 0;

        obj_mgr.safeTransaction([&]() {
            int si_relative_index = 0;

            std::unique_ptr<simdb::ObjectRef> si_root =
                SINodeCreator::createMidLevelSINode(
                    obj_mgr, db_timeseries_.getId(), 0,
                    report_.getName(), si_relative_index);

            auto & db_header_obj_ref = db_timeseries_.getHeader().getObjectRef();
            db_header_obj_ref.setPropertyInt32("SIRootNodeID", si_root->getId());

            recursCreateSubreportSIHierarchy(
                obj_mgr, *si_root, db_timeseries_.getId(),
                report_, si_relative_index);

            root_report_id = si_root->getId();
        });

        return root_report_id;
    }

private:
    db::ReportTimeseries & db_timeseries_;
    const Report & report_;
};

SINodeHierarchy::SINodeHierarchy(db::ReportTimeseries & db_timeseries,
                                 const Report & report) :
    impl_(new SINodeHierarchy::Impl(db_timeseries, report))
{
}

simdb::DatabaseID SINodeHierarchy::serializeHierarchy(simdb::ObjectManager & obj_mgr)
{
    return impl_->serializeHierarchy(obj_mgr);
}

} // namespace statistics
} // namespace sparta
