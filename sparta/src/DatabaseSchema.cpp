// <DatabaseSchema> -*- C++ -*-

#include <ctype.h>
#include <math.h>
#include <cstddef>
#include <boost/algorithm/string/trim.hpp>
#include <algorithm>
#include <istream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "simdb/schema/Schema.hpp"
#include "simdb/schema/TableSummaries.hpp"
#include "sparta/report/db/Schema.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/report/DatabaseInterface.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/report/SubContainer.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

//! Static initializers for SimDB-related classes
namespace sparta {
    std::unordered_set<const RootTreeNode*> DatabaseAccessor::all_simulation_accessors_;
    bool DatabaseAccessor::static_simdb_accessor_invoked_ = false;
}

namespace sparta {
namespace db {

//! Build a SimDB schema object that can hold all report artifacts
//! and StatisticInstance values for SPARTA simulators. This schema
//! can be given to a simdb::ObjectManager to instantiate a physical
//! database connection.
void buildSimulationDatabaseSchema(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    //Statistics databases are comprised of run/simulation
    //metadata, and SI values stored as blobs.

    {
        //Let's start by creating the metadata table.
        schema.addTable("ReportHeader")
            .addColumn("TimeseriesID", dt::fkey_t)
                ->index()
                ->setDefaultValue(0)
            .addColumn("ReportName",   dt::string_t)
            .addColumn("StartTime",    dt::uint64_t)
                ->setDefaultValue(0)
            .addColumn("EndTime",      dt::uint64_t)
                ->setDefaultValue(-1)
            .addColumn("WarmupInsts",  dt::uint64_t)
            .addColumn("DestFile",     dt::string_t)
            .addColumn("SILocations",  dt::string_t)
            .addColumn("NumStatInsts", dt::int32_t)
            .addColumn("SIRootNodeID", dt::fkey_t);

        //Records from two or more tables may need to be linked
        //together in some way. We have this general-purpose table
        //which holds nothing but ObjectManager UUID's to help
        //accomplish this. The ObjectManager class has a getId()
        //method, and records from different tables can be linked
        //via this UUID. As an example:
        //
        //   |=======================================================|
        //   |  SimInfo                                              |
        //   | ------------------------------------------------------|
        //   |  Id        | Name    | SimulatorVersion    | ObjMgrID |
        //   | ------------------------------------------------------|
        //   |  1         | "MySim" |  "2.0"              | 14       |
        //   |  2         | "DVM"   |  "2.3"              | 16       |
        //   | ------------------------------------------------------|
        //
        //   |=====================================|
        //   |  ReportNodeHierarchy                |
        //   | ------------------------------------|
        //   |  Id        | Name    | ParentNodeID |
        //   | ------------------------------------|
        //   |  8         |   "top" |            0 |
        //   |  9         | "core0" |            8 |
        //   |  10        |   "rob" |            9 |
        //   | ------------------------------------|
        //
        //If we wanted to write an API which takes a report node
        //database ID (say, 10) and create a report from the SI
        //data stored in the database, we will need a quick way
        //to get from any report node ID to a row in the SimInfo
        //table. Perhaps we write our code like this:
        //
        //     1) Take the report node ID of 10 and keep running
        //        queries on the ReportNodeHierarchy table until
        //        we are at the top report node in this specific
        //        SI hierarchy. In this case, 10->9->8, and the
        //        root report node has an ID of 8.
        //     2) Say we had another table which mapped root-
        //        level report node ID's to their corresponding
        //        ObjMgrID, which looks something like this:
        //
        //   |===========================================|
        //   |  RootReportObjMgrIDs                      |
        //   | ------------------------------------------|
        //   |  Id        | RootReportNodeID  | ObjMgrID |
        //   | ------------------------------------------|
        //   |  1         |                8  |       16 |
        //   | ------------------------------------------|
        //
        //Now our API could be implemented like this:
        //
        //   void makeReportFromDatabaseNode(int node_db_id)
        //   {
        //       //Say node_db_id = 10
        //       auto root_db_id = getRootDbIdFrom(node_db_id);
        //
        //       //Now root_db_id = 8
        //       obj_mgr_id = getObjMgrIdForRootReportNode(root_db_id);
        //
        //       //Now obj_mgr_id = 16. The final pseudo-code:
        //       auto sim_info = eval_sql(
        //           "SELECT * FROM SimInfo WHERE ObjMgrID = 16");
        //
        //       std::cout << sim_info.Name << " ran with simulator version "
        //                 << sim_info.SimulatorVersion << std::endl;
        //
        //       //  " DVM ran with simulator version 2.3 "  //
        //   }
        schema.addTable("ObjectManagersInDatabase")
            .addColumn("ObjMgrID", dt::fkey_t);

        //Table for SimulationInfo. These records are linked
        //to root-level nodes in the ReportNodeHierarchy table
        //via the ObjectManager UUID they both share.
        schema.addTable("SimInfo")
            .addColumn("Name",               dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Cmdline",            dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("WorkingDir",         dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Exe",                dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("SimulatorVersion",   dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("SpartaVersion",        dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Repro",              dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Start",              dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Other",              dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("ObjMgrID",           dt::fkey_t)
                ->setDefaultValue(0);

        //Table which describes report/subreport node hierarchy
        schema.addTable("ReportNodeHierarchy")
            .addColumn("ParentNodeID",    dt::fkey_t)
                ->index()
            .addColumn("Name",            dt::string_t)
            .addColumn("IsLeafSI",        dt::int32_t)
                ->noSummary()
                ->setDefaultValue(-1)
            .addColumn("LeftmostSIIndex", dt::int32_t)
                ->noSummary()
                ->setDefaultValue(-1);

        //Table which describes sub-statistics node hierarchies.
        //Used to serialize the basic layout of ContextCounter's
        //in a given report.
        schema.addTable("SubStatisticsNodeHierarchy")
            .addColumn("ReportNodeID",   dt::fkey_t)
                ->index()
            .addColumn("SINodeID",       dt::fkey_t)
            .addColumn("ParentSINodeID", dt::fkey_t);

        //This table is used to tell the SimDB->report code which
        //sub-statistics are "unprintable". This supports Context
        //Counter's which have special treatment in the JSON formatter
        //code. We won't have any sparta::ContextCounter<T> objects
        //available when we regenerate reports just from records
        //in a database, so this table helps mimic what the original
        //simulator's legacy json* formatters would have done during
        //simulation.
        schema.addTable("UnprintableSubStatistics")
            .addColumn("ReportNodeID", dt::fkey_t)
                ->index();

        //Table which stores all metadata that is common to all
        //report/subreport nodes
        schema.addTable("ReportNodeMetadata")
            .addColumn("Author",           dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("InfoString",       dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("StartTick",        dt::uint64_t)
                ->setDefaultValue(0)
            .addColumn("EndTick",          dt::uint64_t)
                ->setDefaultValue(-1)
            .addColumn("ReportNodeID",     dt::fkey_t)
                ->setDefaultValue(-1)
                ->index();

        //Unlike the ReportNodeMetadata table, this table stores
        //metadata that is common to all report nodes in a given
        //report/subreport hierarchy.
        schema.addTable("RootReportNodeMetadata")
            .addColumn("ReportNodeID", dt::fkey_t)
                ->indexAgainst("Name")
            .addColumn("Name",         dt::string_t)
            .addColumn("Value",        dt::string_t);

        //Table which stores all style metadata for
        //a given report/subreport
        schema.addTable("ReportStyle")
            .addColumn("StyleName",    dt::string_t)
            .addColumn("StyleValue",   dt::string_t)
            .addColumn("ReportNodeID", dt::fkey_t)
                ->index();

        //SI metadata used in report generation (all formats)
        constexpr int vs_default = static_cast<int>(
            sparta::StatisticDef::ValueSemantic::VS_INVALID);
        constexpr int vis_default = static_cast<int>(
            sparta::InstrumentationNode::VIS_NORMAL);
        constexpr int cls_default = static_cast<int>(
            sparta::InstrumentationNode::DEFAULT_CLASS);

        schema.addTable("SIMetadata")
            .addColumn("Location",             dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("Desc",                 dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("ExprString",           dt::string_t)
                ->setDefaultValue("unset")
            .addColumn("ValueSemantic",        dt::int32_t)
                ->noSummary()
                ->setDefaultValue(vs_default)
            .addColumn("Visibility",           dt::int32_t)
                ->noSummary()
                ->setDefaultValue(vis_default)
            .addColumn("Class",                dt::int32_t)
                ->noSummary()
                ->setDefaultValue(cls_default)
            .addColumn("ReportNodeID",         dt::fkey_t)
                ->setDefaultValue(-1)
                ->index();

        //Make a 1-to-1 link from all root-level report nodes
        //to the ID of the ObjectManager they came from
        schema.addTable("RootReportObjMgrIDs")
            .addColumn("RootReportNodeID", dt::fkey_t)
            .addColumn("ObjMgrID",         dt::fkey_t);

        //The above report metadata columns are for the most
        //common pieces of metadata found in statistics reports.
        //Let's use a catch-all string metadata table that any
        //generic name/value pair can go into. We don't need
        //a dedicated wrapper API around every possible metadata
        //we can think of.
        schema.addTable("StringMetadata")
            .addColumn("ReportHeaderID",       dt::fkey_t)
                ->indexAgainst("MetadataName")
            .addColumn("MetadataName",         dt::string_t)
            .addColumn("MetadataValue",        dt::string_t);

        //Create an SI hierarchy table. Say there was an SI
        //tree that looked like this (assume just 1 timeseries):
        //
        //                      top (id 1)
        //             -----------------------------
        //              |                         |
        //          foo (id 2)                bar (id 3)
        //     --------------------      --------------------
        //      |       |        |        |                |
        //    leafA   leafB    leafC    leafD            leafE
        //    (id 4)  (id 5)   (id 6)   (id 7)           (id 8)
        //
        //This SINodeHierarchy table would look like this:
        //
        //  Id     ParentNodeID     RelativeSIIndex     NodeName
        //  ----   --------------   -----------------   ----------
        //  1      0                0                   top
        //  2      1                0                   foo
        //  3      1                3                   bar
        //  4      2                0                   leafA
        //  5      2                1                   leafB
        //  6      2                2                   leafC
        //  7      3                3                   leafD
        //  8      3                4                   leafE
        //
        //The "RelativeSIIndex" column answers the question:
        //"If I traveled from this SI node to the first leaf
        //SI node I encountered in a depth-first traversal,
        //what would be that leaf SI's index?" Where leaf SI
        //indexes go from 0 to N-1, N being the number of SI's
        //in this entire report/SI hierarchy (0 is leftmost
        //SI index, N-1 is rightmost SI index).
        schema.addTable("SINodeHierarchy")
            .addColumn("TimeseriesID",          dt::fkey_t)
            .addColumn("ParentNodeID",          dt::fkey_t)
                ->indexAgainst("TimeseriesID")
            .addColumn("NodeName",              dt::string_t)
            .addColumn("RelativeSIIndex",       dt::int32_t)
                ->noSummary();

        //Clock hierarchies. Simulations will serialize the
        //hieararchy from the root clock down through any
        //children it has.
        schema.addTable("ClockHierarchy")
            .addColumn("ParentClockID", dt::fkey_t)
            .addColumn("Name",          dt::string_t)
            .addColumn("Period",        dt::uint32_t)
            .addColumn("FreqMHz",       dt::double_t)
            .addColumn("RatioToParent", dt::double_t);
    }

    {
        //Create the Timeseries table
        schema.addTable("Timeseries")
            .addColumn("ReportHeaderID", dt::fkey_t);

        //Create the TimeseriesChunk table
        schema.addTable("TimeseriesChunk")
            .addColumn("TimeseriesID", dt::fkey_t)
                ->indexAgainst({
                    "StartPS",
                    "EndPS",
                    "StartCycle",
                    "EndCycle"
                  })
            .addColumn("StartPS",    dt::uint64_t)
                ->noSummary()
            .addColumn("EndPS",      dt::uint64_t)
                ->noSummary()
            .addColumn("StartCycle", dt::uint64_t)
                ->noSummary()
            .addColumn("EndCycle",   dt::uint64_t)
                ->noSummary();

        //Create the StatInstValues table
        schema.addTable("StatInstValues")
            .addColumn("TimeseriesChunkID", dt::fkey_t)
                ->index()
            .addColumn("RawBytes",      dt::blob_t)
            .addColumn("NumPts",        dt::int32_t)
                ->noSummary()
            .addColumn("WasCompressed", dt::int32_t)
                ->noSummary()
            .addColumn("MajorOrdering", dt::int32_t)
                ->noSummary()
                ->setDefaultValue(
                      static_cast<int>(db::MajorOrdering::ROW_MAJOR));

        //Hold SI value blobs for single-update, non-
        //timeseries report formats in a separate table.
        //Reports like json_reduced and html are stored
        //in this table.
        schema.addTable("SingleUpdateStatInstValues")
            .addColumn("RootReportNodeID", dt::fkey_t)
                ->index()
                ->setDefaultValue(-1)
            .addColumn("RawBytes",      dt::blob_t)
            .addColumn("NumPts",        dt::int32_t)
                ->noSummary()
            .addColumn("WasCompressed", dt::int32_t)
                ->noSummary();
    }

    {
        //All of the tables in this section are here to support
        //post-simulation report verification against legacy report
        //files. They are only here for smoke testing, debugging
        //report-related bugs, etc. and may be removed at any point
        //in the future.

        //Maintain a mapping from report database ID to the original
        //descriptor's dest_file and format strings.
        schema.addTable("ReportVerificationMetadata")
            .addColumn("RootReportNodeID", dt::fkey_t)
            .addColumn("DestFile",         dt::string_t)
                ->index()
            .addColumn("Format",           dt::string_t);

        //High-level pass/fail results for each report in this
        //database. Also includes a key to get each reports'
        //accompanying SimInfo record. Useful information for
        //debugging failed verifications can be found in the
        //SimInfo table, such as repro commands.
        schema.addTable("ReportVerificationResults")
            .addColumn("DestFile",       dt::string_t)
            .addColumn("SimInfoID",      dt::fkey_t)
                ->indexAgainst("Passed")
            .addColumn("Passed",         dt::int32_t)
            .addColumn("IsTimeseries",   dt::int32_t)
                ->noSummary();

        //We use the SpartaTester utility class to find any differences
        //between database-produced report files and their baselines.
        //SpartaTester gives us a quick summary of file diff(s) just like
        //you would see printed to stdout while running regression tests.
        //We store those summaries in this table.
        schema.addTable("ReportVerificationFailureSummaries")
            .addColumn("ReportVerificationResultID", dt::fkey_t)
                ->index()
            .addColumn("FailureSummary", dt::string_t)
                ->setDefaultValue("unset");

        //When report verification is enabled, we may store deep copies
        //of the diff'd files when failures occur so we don't have to
        //rely on repro steps found in the SimInfo table. This is costly
        //for regression tests that result in many failed verifications,
        //but these tables are more for developer use / debugging than
        //production simulators.
        schema.addTable("ReportVerificationDeepCopyFiles")
            .addColumn("DestFile", dt::string_t)
                ->index()
            .addColumn("Expected", dt::string_t)
            .addColumn("Actual",   dt::string_t);
    }
}

//! Configure the default TableSummaries object for SPARTA simulation
//! databases. This will provide default implementations for common
//! summary calculations like min/max/average, and possibly others.
void configureDatabaseTableSummaries(simdb::TableSummaries & config)
{
    config.define("min",
                  [](const std::vector<double> & vals) -> double {
                      if (vals.empty()) {
                          return NAN;
                      }
                      return *std::min_element(vals.begin(), vals.end());
                  })
          .define("max",
                  [](const std::vector<double> & vals) -> double {
                      if (vals.empty()) {
                          return NAN;
                      }
                      return *std::max_element(vals.begin(), vals.end());
                  })
          .define("avg",
                  [](const std::vector<double> & vals) -> double {
                      if (vals.empty()) {
                          return NAN;
                      }
                      size_t n = 0;
                      double mean = 0;
                      for (auto val : vals) {
                          const double delta = (val - mean);
                          mean += delta / ++n;
                      }
                      return mean;
                  });
}

} // namespace db

DatabaseAccessor::AccessTrigger::AccessTrigger(
    DatabaseAccessor * db_accessor,
    const std::string & db_namespace,
    const std::string & start_expr,
    const std::string & stop_expr,
    RootTreeNode * rtn,
    std::shared_ptr<SubContainer> & sub_container)
{
    if (!start_expr.empty() || !stop_expr.empty()) {
        sparta_assert(!db_namespace.empty());
        sparta_assert(rtn != nullptr);
    }

    db_accessor_ = db_accessor;
    db_namespace_ = db_namespace;

    if (!start_expr.empty()) {
        const std::string trig_name = "GrantAccess_" + db_namespace;
        auto handler = CREATE_SPARTA_HANDLER(
            DatabaseAccessor::AccessTrigger, grantAccess_);

        start_ = std::make_shared<trigger::ExpressionTrigger>(
            trig_name,
            handler,
            start_expr,
            rtn->getSearchScope(),
            sub_container);
    }

    if (!stop_expr.empty()) {
        const std::string trig_name = "RevokeAccess_" + db_namespace;
        auto handler = CREATE_SPARTA_HANDLER(
            DatabaseAccessor::AccessTrigger, revokeAccess_);

        stop_ = std::make_shared<trigger::ExpressionTrigger>(
            trig_name,
            handler,
            stop_expr,
            rtn->getSearchScope(),
            sub_container);
    }
}

void DatabaseAccessor::setAccessOptsFromFile_(
    const std::string & opt_file)
{
    if (!sub_container_) {
        sub_container_ = std::make_shared<SubContainer>();
    }

    std::ifstream fin(opt_file);
    if (!fin) {
        return;
    }

    struct NamespaceAccess {
        void beginNewNamespace(const std::string & line) {
            components_.clear();
            start_trig_expr_.clear();
            stop_trig_expr_.clear();
            is_parsing_components_ = false;

            auto sep = line.find(":");
            ns_name_ = line.substr(0, sep);
            boost::trim(ns_name_);
            std::transform(ns_name_.begin(), ns_name_.end(),
                           ns_name_.begin(), ::tolower);
        }

        const std::string & getNamespaceName() const {
            return ns_name_;
        }

        void beginComponents() {
            is_parsing_components_ = true;
        }

        bool isParsingComponents() const {
            return is_parsing_components_;
        }

        void addComponent(const std::string & component) {
            components_.emplace_back(component);
        }

        const std::vector<std::string> & getComponents() const {
            return components_;
        }

        void setStartTrigExpr(const std::string & expr) {
            start_trig_expr_ = expr;
        }

        void setStopTrigExpr(const std::string & expr) {
            stop_trig_expr_ = expr;
        }

        const std::string & getStartTrigExpr() const {
            return start_trig_expr_;
        }

        const std::string & getStopTrigExpr() const {
            return stop_trig_expr_;
        }

    private:
        std::string ns_name_;
        std::vector<std::string> components_;
        std::string start_trig_expr_;
        std::string stop_trig_expr_;
        bool is_parsing_components_ = false;
    };

    auto is_namespace = [](const std::string & line) {
        if (line.find("components:") != std::string::npos) {
            return false;
        }
        if (line.find("start:") != std::string::npos) {
            return false;
        }
        if (line.find("stop:") != std::string::npos) {
            return false;
        }
        return line.find(":") != std::string::npos;
    };

    auto is_component = [](const std::string & line) {
        std::string line_(line);
        boost::trim(line_);
        return line_ == "components:";
    };

    auto is_start_trig = [](const std::string & line) {
        std::string line_(line);
        boost::trim(line_);
        return line_.find("start:") == 0;
    };

    auto is_stop_trig = [](const std::string & line) {
        std::string line_(line);
        boost::trim(line_);
        return line_.find("stop:") == 0;
    };

    NamespaceAccess accessor;
    std::vector<NamespaceAccess> finalized_accessors;
    for (std::string line; std::getline(fin, line); ) {
        if (is_namespace(line)) {
            finalized_accessors.emplace_back(accessor);
            accessor.beginNewNamespace(line);
        } else if (is_component(line)) {
            accessor.beginComponents();
        } else if (is_start_trig(line)) {
            accessor.setStartTrigExpr(line.substr(line.find(":") + 1));
        } else if (is_stop_trig(line)) {
            accessor.setStopTrigExpr(line.substr(line.find(":") + 1));
        } else if (accessor.isParsingComponents()) {
            accessor.addComponent(line);
        }
    }
    finalized_accessors.emplace_back(accessor);

    for (const auto & access : finalized_accessors) {
        if (!access.getNamespaceName().empty()) {
            access_triggers_.emplace_back(new AccessTrigger(
                this,
                access.getNamespaceName(),
                access.getStartTrigExpr(),
                access.getStopTrigExpr(),
                root_,
                sub_container_));
        }

        for (const auto & comp : access.getComponents()) {
            enableComponentAtLocation_(access.getNamespaceName(), comp);
        }
    }
}

} // namespace sparta
