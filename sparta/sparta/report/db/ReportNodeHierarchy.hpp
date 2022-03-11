// <ReportNodeHierarchy> -*- C++ -*-

#pragma once

#include "sparta/report/Report.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief This class serializes a sparta::Report's entire
 * report tree (report names, subreport names, etc.) into
 * the ReportNodeHierarchy table in the database object
 * you provide.
 */
class ReportNodeHierarchy
{
public:
    //! Construct with a report object you want to serialize.
    explicit ReportNodeHierarchy(const Report * report) :
        report_(report)
    {
        if (report_ == nullptr) {
            throw SpartaException("Null report given to ReportNodeHierarchy");
        }
    }

    //! Serialize this object's report to the provided SimDB.
    //! Returns the database ID corresponding to the root-level
    //! Report node in this hierarchy.
    simdb::DatabaseID serializeHierarchy(const simdb::ObjectManager & obj_mgr)
    {
        obj_mgr.safeTransaction([&]() {
            //This walks the SI/report tree in a depth-first
            //fashion, creating report nodes in the database
            //table(s) along the way as it serializes the
            //hierarchy.
            int leftmost_si_index = 0;
            const bool is_leaf = false;

            std::unique_ptr<simdb::ObjectRef> root_report_node =
                createReportNode_(report_->getName(), 0,
                                  leftmost_si_index,
                                  is_leaf,
                                  obj_mgr);

            report_node_ids_.emplace_back(report_, root_report_node->getId());
            unordered_report_node_ids_[report_] = root_report_node->getId();

            recursCreateSubreportNode_(*report_,
                                       *root_report_node,
                                       leftmost_si_index,
                                       obj_mgr);

            root_report_node_db_id_ = root_report_node->getId();
            for (const auto & meta : root_report_metadata_) {
                serializeReportGlobalMetadata_(
                    root_report_node_db_id_, meta.first, meta.second, obj_mgr);
            }
            root_report_metadata_.clear();

            //Create any sub-statistics hierarchies that exist in this report.
            //This will be used later to generate report files for reports
            //that had ContextCounter's in them.
            recursCreateSubStatisticsNodeHierarchy_(report_, obj_mgr);

            //Store some information in a separate table that will let
            //the database report recreation code path know when to
            //skip over certain sub-statistics. This lets database-
            //regenerated reports exactly match simulation-produced
            //reports when ContextCounters are found in the simulator.
            markSubStatisticNodesAsUnprintable_(obj_mgr);

            //Create a 1-to-1 link between this root-level report
            //node record and the ObjectManager it came from. This
            //will be needed later on when we are asked to recreate
            //a formatted report from a report database ID.
            //
            //For example, we may need to get the exact SimulationInfo
            //record that was created by the original simulation. Those
            //records live in a table with an ObjMgrID field, which is
            //how we tie these two tables together with low overhead.
            auto report_obj_mgr_linker = obj_mgr.getTable("RootReportObjMgrIDs");
            auto report_obj_mgr_link = report_obj_mgr_linker->createObjectWithArgs(
                "RootReportNodeID", root_report_node_db_id_,
                "ObjMgrID", obj_mgr.getId());
        });

        return root_report_node_db_id_;
    }

    //! Write report metadata to the provided SimDB. This includes
    //! things like report start/stop times.
    void serializeReportNodeMetadata(const simdb::ObjectManager & obj_mgr) const
    {
        obj_mgr.safeTransaction([&]() {
            for (const auto & reports_by_id : report_node_ids_) {
                const Report * report = reports_by_id.first;
                const simdb::DatabaseID report_hier_node_id = reports_by_id.second;
                serializeReportNodeMetadata_(*report, report_hier_node_id, obj_mgr);
            }
        });
    }

    //! Write any style information this object's report had at
    //! the time of the original simulation. Some reports may have
    //! no style metadata at all; it is not an error to call this
    //! method anyway. In those cases, no ReportStyle record will
    //! be created in the database for this report.
    void serializeReportStyles(const simdb::ObjectManager & obj_mgr) const
    {
        obj_mgr.safeTransaction([&]() {
            for (const auto & reports_by_id : report_node_ids_) {
                const Report * report = reports_by_id.first;
                const simdb::DatabaseID report_hier_node_id = reports_by_id.second;
                serializeReportStyle_(*report, report_hier_node_id, obj_mgr);
            }
        });
    }

    //! Add generic name-value pairs of string metadata that applies
    //! to every report/subreport/SI node we are serializing.
    void setMetadataCommonToAllNodes(
        const std::string & name,
        const std::string & value,
        const simdb::ObjectManager & obj_mgr)
    {
        if (root_report_node_db_id_ > 0) {
            serializeReportGlobalMetadata_(
                root_report_node_db_id_, name, value, obj_mgr);
        } else {
            root_report_metadata_[name] = value;
        }
    }

private:
    //! All report/SI nodes are written to the ReportNodeHierarchy
    //! table through this method.
    std::unique_ptr<simdb::ObjectRef> createReportNode_(
        const std::string & name,
        const simdb::DatabaseID node_id,
        int & leftmost_si_index,
        const bool is_leaf,
        const simdb::ObjectManager & obj_mgr) const
    {
        std::unique_ptr<simdb::TableRef> hier_tbl = obj_mgr.getTable("ReportNodeHierarchy");
        std::unique_ptr<simdb::ObjectRef> node_ref = hier_tbl->createObjectWithArgs(
            "Name", name,
            "ParentNodeID", node_id,
            "IsLeafSI", is_leaf ? 1 : 0,
            "LeftmostSIIndex", leftmost_si_index);

        if (is_leaf) {
            ++leftmost_si_index;
        }
        return node_ref;
    }

    //! Some extra metadata only applies to leaf SI's, and not other
    //! hierarchy nodes. Things like SI::getLocation(), SI::getDesc(),
    //! etc. are all written to the database through this method.
    void createLeafSIMetadata_(
        const StatisticInstance * si,
        const simdb::ObjectRef & report_hier_node_ref,
        const simdb::ObjectManager & obj_mgr) const
    {
        std::unique_ptr<simdb::TableRef> si_metadata_tbl = obj_mgr.getTable("SIMetadata");
        if (si_metadata_tbl == nullptr) {
            throw SpartaException("Unable to locate SIMetadata table in SimDB");
        }

        std::unique_ptr<simdb::ObjectRef> si_metadata_ref = si_metadata_tbl->createObjectWithArgs(
            "ReportNodeID", report_hier_node_ref.getId(),
            "Location", si->getLocation(),
            "Desc", si->getDesc(false),
            "ExprString", si->getExpressionString(),
            "ValueSemantic", (int)si->getValueSemantic(),
            "Visibility", (int)si->getVisibility(),
            "Class", (int)si->getClass());

        auto sdef = si->getStatisticDef();
        std::map<std::string, std::string> written_metadata;
        if (sdef) {
            const auto & sdef_metadata = sdef->getMetadata();
            if (!sdef_metadata.empty()) {
                si_metadata_tbl = obj_mgr.getTable("RootReportNodeMetadata");
                for (const auto & meta : sdef_metadata) {
                    si_metadata_tbl->createObjectWithArgs(
                        "ReportNodeID", report_hier_node_ref.getId(),
                        "Name", meta.first,
                        "Value", meta.second);
                    written_metadata[meta.first] = meta.second;
                }
            }
        }

        const auto & si_metadata = si->getMetadata();
        if (!si_metadata.empty()) {
            si_metadata_tbl = obj_mgr.getTable("RootReportNodeMetadata");
            for (const auto & meta : si_metadata) {
                auto iter = written_metadata.find(meta.first);
                sparta_assert(iter == written_metadata.end() ||
                            iter->second == meta.second);

                si_metadata_tbl->createObjectWithArgs(
                    "ReportNodeID", report_hier_node_ref.getId(),
                    "Name", meta.first,
                    "Value", meta.second);
                written_metadata[meta.first] = meta.second;
            }
        }
    }

    //! Report metadata such as start/end tick, author, etc. are written
    //! to the database through this method.
    void serializeReportNodeMetadata_(
        const Report & report_at_node,
        const simdb::DatabaseID report_hier_node_id,
        const simdb::ObjectManager & obj_mgr) const
    {
        std::unique_ptr<simdb::TableRef> metadata_tbl =
            obj_mgr.getTable("ReportNodeMetadata");

        std::unique_ptr<simdb::ObjectRef> metadata_ref = metadata_tbl->createObject();
        metadata_ref->setPropertyInt32("ReportNodeID", report_hier_node_id);
        metadata_ref->setPropertyUInt64("StartTick", report_at_node.getStart());

        if (report_at_node.getEnd() == Scheduler::INDEFINITE) {
            auto sched = report_at_node.getScheduler();
            if (sched) {
                metadata_ref->setPropertyUInt64("EndTick", sched->getCurrentTick());
            } else {
                metadata_ref->setPropertyUInt64("EndTick", report_at_node.getEnd());
            }
        } else {
            metadata_ref->setPropertyUInt64("EndTick", report_at_node.getEnd());
        }

        const auto & author = report_at_node.getAuthor();
        if (!author.empty()) {
            metadata_ref->setPropertyString("Author", author);
        }

        const auto & info_str = report_at_node.getInfoString();
        if (!info_str.empty()) {
            metadata_ref->setPropertyString("InfoString", info_str);
        }
    }

    //! Report style metadata is written to the database through this
    //! method. Some reports have no style metadata at all, and for
    //! those reports, no style records will be added to the database.
    void serializeReportStyle_(
        const Report & report_at_node,
        const simdb::DatabaseID report_hier_node_id,
        const simdb::ObjectManager & obj_mgr) const
    {
        const auto & styles = report_at_node.getAllStyles();
        if (styles.empty()) {
            return;
        }

        std::unique_ptr<simdb::TableRef> style_tbl = obj_mgr.getTable("ReportStyle");
        for (const auto & nv : styles) {
            const std::string & style_name = nv.first;
            const std::string & style_value = nv.second;
            style_tbl->createObjectWithArgs("StyleName", style_name,
                                            "StyleValue", style_value,
                                            "ReportNodeID", report_hier_node_id);
        }
    }

    //! The SI/report tree is recursively serialized to the database
    //! as we walk the tree in a depth-first fashion. This method is
    //! called once for every top- and mid-level report node we encounter.
    void recursCreateSubreportNode_(
        const Report & subreport,
        const simdb::ObjectRef & parent_node_ref,
        int & leftmost_si_index,
        const simdb::ObjectManager & obj_mgr)
    {
        std::set<const void*> sub_stat_internal_pointers;
        const Report::SubStaticticInstances & sub_stats = subreport.getSubStatistics();

        for (const auto & stat : subreport.getStatistics()) {
            const std::string name = !stat.first.empty() ?
                stat.first : stat.second.getLocation();

            const StatisticInstance * stat_inst = &stat.second;
            const StatisticDef * def = stat_inst->getStatisticDef();
            const CounterBase * ctr = stat_inst->getCounter();
            const ParameterBase * prm = stat_inst->getParameter();

            auto sub_stat_iter = sub_stats.find(def);
            const bool valid_stat_def = (def != nullptr);
            const bool has_valid_sub_stats =
                (valid_stat_def && sub_stat_iter != sub_stats.end());

            if (has_valid_sub_stats) {
                //Gather up the 'this' pointers of the sub-statistics' counters
                //or parameters. We'll use this information to mark certain sub-
                //statistic nodes as "unprintable", which means that the database-
                //driven report regeneration code will know when to skip over
                //certain sub-statistics when it is making reports from a SimDB
                //dataset.
                for (const auto sub_stat : sub_stat_iter->second) {
                    if (sub_stat->getCounter() != nullptr) {
                        sub_stat_internal_pointers.insert(sub_stat->getCounter());
                    } else if (sub_stat->getParameter() != nullptr) {
                        sub_stat_internal_pointers.insert(sub_stat->getParameter());
                    }
                }
            }

            const bool is_leaf = true;
            std::unique_ptr<simdb::ObjectRef> leaf_report_node = createReportNode_(
                name, parent_node_ref.getId(),
                leftmost_si_index, is_leaf,
                obj_mgr);

            createLeafSIMetadata_(&stat.second,
                                  *leaf_report_node,
                                  obj_mgr);

            if (sub_stat_internal_pointers.count(ctr) > 0 ||
                sub_stat_internal_pointers.count(prm) > 0)
            {
                const void * sub_stat_this_ptr =
                    ctr ? ((const void*)ctr) : ((const void*)prm);

                sparta_assert(sub_stat_this_ptr != nullptr);

                sdef_sub_stat_ids_[sub_stat_this_ptr].emplace_back(
                    leaf_report_node->getId());
            }

            unordered_si_ids_[&stat.second] = leaf_report_node->getId();
        }

        for (const auto & sr : subreport.getSubreports()) {
            const bool is_leaf = false;
            std::unique_ptr<simdb::ObjectRef> subreport_node_ref =
                createReportNode_(sr.getName(), parent_node_ref.getId(),
                                  leftmost_si_index, is_leaf,
                                  obj_mgr);

            report_node_ids_.emplace_back(&subreport, subreport_node_ref->getId());
            unordered_report_node_ids_[&sr] = subreport_node_ref->getId();

            recursCreateSubreportNode_(sr, *subreport_node_ref,
                                       leftmost_si_index,
                                       obj_mgr);
        }
    }

    //! Recursively walk the report/SI hierarchy and serialize mappings
    //! that describe sub-statistics hierarchies in this report. This
    //! supports ContextCounter sub-hierarchies for SimDB generated
    //! report files after simulation.
    void recursCreateSubStatisticsNodeHierarchy_(
        const Report * r,
        const simdb::ObjectManager & obj_mgr) const
    {
        auto recurse = [&]() {
            for (const auto & sr : r->getSubreports()) {
                recursCreateSubStatisticsNodeHierarchy_(&sr, obj_mgr);
            }
        };

        auto report_node_iter = unordered_report_node_ids_.find(r);
        if (report_node_iter == unordered_report_node_ids_.end()) {
            recurse();
            return;
        }

        const simdb::DatabaseID report_node_id = report_node_iter->second;
        for (const auto & stat : r->getStatistics()) {
            const auto stat_def = stat.second.getStatisticDef();
            if (!stat_def) {
                continue;
            }

            const auto & sub_stats = r->getSubStatistics();
            auto sub_stat_iter = sub_stats.find(stat_def);
            if (sub_stat_iter == sub_stats.end()) {
                continue;
            }

            auto sub_stats_hier_tbl = obj_mgr.getTable("SubStatisticsNodeHierarchy");
            for (const auto sub_stat : sub_stat_iter->second) {
                auto si_node_iter = unordered_si_ids_.find(sub_stat);
                if (si_node_iter == unordered_si_ids_.end()) {
                    continue;
                }

                const simdb::DatabaseID si_node_id = si_node_iter->second;
                auto parent_si_node_iter = unordered_si_ids_.find(&stat.second);
                if (parent_si_node_iter == unordered_si_ids_.end()) {
                    continue;
                }

                const simdb::DatabaseID parent_si_node_id = parent_si_node_iter->second;

                sub_stats_hier_tbl->createObjectWithArgs(
                    "ReportNodeID", report_node_id,
                    "SINodeID", si_node_id,
                    "ParentSINodeID", parent_si_node_id);
            }
        }

        recurse();
    }

    /*!
     * \brief Database-regenerated reports need to exactly match
     * simulation-generated reports. For ContextCounter's in JSON
     * reports, there is a special code path that takes care of
     * writing out "sub-statistics". The outside world - the json,
     * json_reduced, and json_detail legacy formatters - are not
     * supposed to write sub-statistics (ContextCounter internal
     * counters). We have a table called "UnprintableSubStatistics"
     * which lets us have careful control over what the legacy
     * formatters print, and what they don't. This is put into
     * a separate table so we don't impact database size by having
     * a null / zeroed metadata column in the ReportNodeHierarchy
     * table for *all* nodes, when only a relatively small number
     * of nodes in the simulator (ContextCounter internal counters)
     * needs this special treatment.
     */
    void markSubStatisticNodesAsUnprintable_(
        const simdb::ObjectManager & obj_mgr)
    {
        auto sub_stats_unprintable_tbl = obj_mgr.getTable("UnprintableSubStatistics");
        for (const auto & sub_stat_ids : sdef_sub_stat_ids_) {
            for (size_t idx = 0; idx < sub_stat_ids.second.size(); ++idx) {
                const simdb::DatabaseID unprintable_si_node_id = sub_stat_ids.second[idx];
                sub_stats_unprintable_tbl->createObjectWithArgs(
                    "ReportNodeID", unprintable_si_node_id);
            }
        }
    }

    /*!
     * \brief Serialize a metadata name-value pair. Unlike
     * the serializeReportNodeMetadata() method, this method
     * writes metadata that is common / shared with every node
     * in this report hierarchy.
     */
    void serializeReportGlobalMetadata_(
        const simdb::DatabaseID report_id,
        const std::string & name,
        const std::string & value,
        const simdb::ObjectManager & obj_mgr) const
    {
        auto meta_tbl = obj_mgr.getTable("RootReportNodeMetadata");
        if (auto num_rows_affected = meta_tbl->
                updateRowValues("Value", value).
                forRecordsWhere("ReportNodeID", simdb::constraints::equal, report_id,
                                "Name", simdb::constraints::equal, name))
        {
            sparta_assert(num_rows_affected == 1);
            return;
        } else {
            meta_tbl->createObjectWithArgs(
                "ReportNodeID", report_id,
                "Name", name,
                "Value", value);
        }
    }

    const Report * report_ = nullptr;
    std::vector<std::pair<const Report*, simdb::DatabaseID>> report_node_ids_;
    simdb::DatabaseID root_report_node_db_id_ = 0;
    std::unordered_map<const Report*, simdb::DatabaseID> unordered_report_node_ids_;
    std::unordered_map<const StatisticInstance*, simdb::DatabaseID> unordered_si_ids_;
    std::unordered_map<const void*, std::vector<simdb::DatabaseID>> sdef_sub_stat_ids_;
    std::map<std::string, std::string> root_report_metadata_;
};

} // namespace statistics
} // namespace sparta
