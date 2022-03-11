// <ReportStatisticsHierTree> -*- C++ -*-

#pragma once

#include "sparta/report/Report.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief Helper class used to build up a "report statistics hierarchy tree",
 * where the template types are:
 *
 *     NodeT     -- This is the class you want instantiated for each
 *                  sparta::Report node (which includes subreports)
 *     LeafNodeT -- This is the class you want instantiated for each
 *                  sparta::StatisticInstance (at the leaves / no children)
 *
 * For example:
 *     ReportStatisticHierTree<BigNode, LittleNode> tree(r);
 *
 *          // where the report 'r' looks like:
 *
 *                          r
 *              -------------------------
 *              |           |           |
 *             top      scheduler      meta
 *           -------      |   |
 *            |   |      tix mss
 *           foo bar
 *
 * In this use case, your hiearchy tree would be created as:
 *
 *      BigNode 'r'
 *         - children_:
 *                BigNode 'top'
 *                   - children_:
 *                          LittleNode 'foo'
 *                          LittleNode 'bar'
 *                BigNode 'scheduler'
 *                   - children_:
 *                          LittleNode 'tix'
 *                          LittleNode 'mss'
 *                LittleNode 'meta'
 */
template <class NodeT, class LeafNodeT = NodeT>
class ReportStatisticsHierTree
{
public:
    explicit ReportStatisticsHierTree(const Report * r) :
        report_(r)
    {
        sparta_assert(report_, "You may not give a null report to a ReportStatisticsHierTree");
    }

    typedef std::pair<LeafNodeT*, const StatisticInstance*> LeafNodeSI;

    //! Inspect the Report for all SI's, and build the hierarchy tree
    //! for this report. Optionally pass in 'si_locations' if you want
    //! the tree builder to get the "CSV header equivalent" SI location/
    //! name that would appear above the SI values in the .csv file.
    std::vector<LeafNodeSI> buildFrom(std::shared_ptr<NodeT> & root,
                                      std::vector<std::string> * si_locations = nullptr) const {
        return buildFrom(root.get(), si_locations);
    }

    //! Inspect the Report for all SI's, and build the hierarchy tree
    //! for this report. Optionally pass in 'si_locations' if you want
    //! the tree builder to get the "CSV header equivalent" SI location/
    //! name that would appear above the SI values in the .csv file.
    std::vector<LeafNodeSI> buildFrom(NodeT * root,
                                      std::vector<std::string> * si_locations = nullptr) const {
        std::vector<LeafNodeSI> flattened_leaves;
        createSubreportHierTree_(root, *report_, flattened_leaves, si_locations, "");
        return flattened_leaves;
    }

private:
    void createSubreportHierTree_(NodeT * report_node,
                                  const Report & report,
                                  std::vector<LeafNodeSI> & flattened_leaves,
                                  std::vector<std::string> * si_locations,
                                  const std::string & si_location_prefix) const
    {
        const auto & stats = report.getStatistics();
        const auto & subreports = report.getSubreports();
        auto & children = report_node->getChildren();

        if (!stats.empty()) {
            for (const auto & stat : stats) {
                std::string name = !stat.first.empty() ? stat.first : stat.second.getLocation();
                if (si_locations) {
                    si_locations->emplace_back(si_location_prefix + name);
                }
                boost::replace_all(name, ".", "_");

                std::shared_ptr<LeafNodeT> si_node(new LeafNodeT(name, &stat.second));
                si_node->setParent(report_node);
                children.emplace_back(si_node);
                flattened_leaves.emplace_back(std::make_pair(si_node.get(), &stat.second));
            }
        }

        if (!subreports.empty()) {
            for (const auto & sr : subreports) {
                std::vector<std::string> dot_delimited;
                boost::split(dot_delimited, sr.getName(), boost::is_any_of("."));
                const std::string & name = dot_delimited.back();

                std::shared_ptr<NodeT> subreport_node(new NodeT(name, &sr));
                subreport_node->setParent(report_node);
                createSubreportHierTree_(subreport_node.get(), sr,
                                         flattened_leaves,
                                         si_locations,
                                         sr.getName() + ".");

                children.emplace_back(subreport_node);
            }
        }
    }

    const Report * report_ = nullptr;
};

} // namespace statistics
} // namespace sparta
