from .sim_utils import *
from .stat_utils import *
from .json_utils import *
from collections import OrderedDict
import json

class JSReportExporter:
    def __init__(self):
        self.db_conn = None
        self.cursor = None
        self.stat_values_getter = None
        self.leaf_nodes = set()
        self.parents_of_leaf_nodes = set()

    def Export(self, dest_file, descriptor_id, db_conn, cmdline_args):
        self.db_conn = db_conn
        self.cursor = db_conn.cursor()

        cmd = "SELECT ReportName, IsParentOfLeafNodes FROM JsJsonLeafNodes"
        cursor = db_conn.cursor()
        cursor.execute(cmd)
        for report_name, is_parent in cursor.fetchall():
            assert is_parent in (0,1)
            if is_parent == 1:
                self.parents_of_leaf_nodes.add(report_name)
            else:
                self.leaf_nodes.add(report_name)

        # Get the root report ID
        cmd = f"SELECT Id FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = 0"
        cursor = db_conn.cursor()
        cursor.execute(cmd)
        row = cursor.fetchone()
        if row is None:
            raise ValueError(f"Report descriptor {descriptor_id} does not have a root report")

        report_id = row[0]

        # decimal_places_ = strtoul(report_->getStyle("decimal_places", "2").c_str(), nullptr, 0);
        if cmdline_args.v2:
            decimal_places = -1
        else:
            decimal_places = int(GetReportStyle(cursor, report_id, descriptor_id, "decimal_places", "2"))
        self.stat_values_getter = GetStatsValuesGetter(self.cursor, dest_file, True, True, decimal_places)

        # out << "{\n";
        # out << "  \"units\" : {\n";
        out = OrderedDict([("units", OrderedDict())])

        # std::vector<std::string> all_key_names;
        # writeReport_(out, *report_, all_key_names);
        all_key_names = []
        self.__WriteReport(out, report_id, descriptor_id, all_key_names)

        # out << "    \"ordered_keys\" : [    \n";
        # for (uint32_t idx = 0; idx < all_key_names.size(); idx++) {
        #     out << "      \"" << all_key_names[idx] << "\"";
        #     if (idx != (all_key_names.size() - 1)) {
        #         out << ", ";
        #     }
        #     out << "\n";
        # }
        # out << "    ]\n";
        # out << "  },\n";
        out["units"]["ordered_keys"] = all_key_names

        # out << "  \"vis\" : {\n";
        # out << "    \"hidden\"  : " << sparta::InstrumentationNode::VIS_HIDDEN << ",\n";
        # out << "    \"support\" : "  << sparta::InstrumentationNode::VIS_SUPPORT << ",\n";
        # out << "    \"detail\"  : "  << sparta::InstrumentationNode::VIS_DETAIL << ",\n";
        # out << "    \"normal\"  : "  << sparta::InstrumentationNode::VIS_NORMAL << ",\n";
        # out << "    \"summary\" : "  << sparta::InstrumentationNode::VIS_SUMMARY << "\n";
        # out << "  },\n";
        out["vis"] = GetVisibilities(cursor)
        if "critical" in out["vis"]:
            del out["vis"]["critical"]

        # out << "  \"siminfo\" : {\n";
        # out << "    \"name\" : \"" << SimulationInfo::getInstance().sim_name << "\",\n";
        # out << "    \"sim_version\" : \"" << SimulationInfo::getInstance().simulator_version << "\",\n";
        # out << "    \"sparta_version\" : \"" << SimulationInfo::getInstance().getSpartaVersion() << "\",\n";
        # out << "    \"reproduction\" : \"" << SimulationInfo::getInstance().reproduction_info << "\"\n";
        # out << "  },\n";
        out["siminfo"] = GetSimInfo(cursor)
        if "json_report_version" in out["siminfo"]:
            del out["siminfo"]["json_report_version"]

        # out << "  \"report_metadata\": ";
        # if (metadata_kv_pairs_.empty()) {
        #     out << "{}\n";
        # } else {
        #     out << "{\n";
        #     const size_t num_metadata = metadata_kv_pairs_.size();
        #     size_t num_written_metadata = 0;
        #     for (const auto & md : metadata_kv_pairs_) {
        #         out << std::string(4, ' ') << "\"" << md.first << "\": \"" << md.second << "\"";
        #         ++num_written_metadata;
        #         if (num_written_metadata <= num_metadata - 1) {
        #             out << ",\n";
        #         }
        #     }
        #     out << "  }\n";
        # }
        #
        # out << "}\n";
        # out << std::endl;
        out["report_metadata"] = GetJsonReportMetadata(cursor, descriptor_id, "js_json")

        with open(dest_file, "w") as fout:
            json.dump(out, fout, indent=2)

    def __WriteReport(self, out, report_id, descriptor_id, all_unit_names):
        # // Early out - no stats in this report or any sub-reports
        # if (report.getRecursiveNumStatistics() == 0) {
        #     return;
        # }
        if not HasStatistics(self.cursor, descriptor_id, report_id, recursive=True):
            return

        # bool merge_subreports = isLeafNode_(report);
        merge_subreports = self.__IsLeafNode(report_id)

        # // If we need to start a new report
        # if (merge_subreports || report.getNumStatistics() > 0) {
        if merge_subreports or HasStatistics(self.cursor, descriptor_id, report_id):
            # std::string unit_name = getReportName_(report);
            # out << "    \"" << unit_name << "\": {\n";
            # all_unit_names.push_back(unit_name);
            #
            # std::vector<std::string> all_stat_names;
            # writeStats_(out, report, "", all_stat_names);
            unit_name = GetReportName(self.cursor, report_id)
            out["units"][unit_name] = OrderedDict()
            all_unit_names.append(unit_name)

            all_stat_names = []
            self.__WriteStats(out["units"][unit_name], report_id, descriptor_id, "", all_stat_names)

            # if (merge_subreports) {
            #     mergeReportList_(out, report.getSubreports(), getReportName_(report), all_stat_names);
            # }
            if merge_subreports:
                subrep_ids = GetSubreportIDs(self.cursor, report_id)
                self.__MergeReportList(out["units"][unit_name], subrep_ids, unit_name, all_stat_names)

            # out << "      \"ordered_keys\": [\n";
            # for (uint32_t idx = 0; idx < all_stat_names.size(); idx++) {
            #     out << "        \"" << all_stat_names[idx] << "\"";
            #     if (idx != (all_stat_names.size() - 1)) {
            #         out << ", ";
            #     }
            #     out << "\n";
            # }
            # out << "      ]\n";
            # out << "    },\n";
            out["units"][unit_name]["ordered_keys"] = all_stat_names

        # if (!merge_subreports) {
        #     writeReportList_(out, report.getSubreports(), all_unit_names);
        # }
        if not merge_subreports:
            subrep_ids = GetSubreportIDs(self.cursor, descriptor_id, report_id)
            self.__WriteReportList(out, subrep_ids, descriptor_id, all_unit_names)

    def __WriteReportList(self, out, subrep_ids, descriptor_id, all_unit_names):
        # for (const auto & r : reports) {
        #     writeReport_(out, r, all_unit_names);
        # }
        for report_id in subrep_ids:
            self.__WriteReport(out, report_id, descriptor_id, all_unit_names)

    def __MergeReport(self, out, report_id, descriptor_id, merge_top_name, all_stat_names):
        # const std::string & report_name = getReportName_(report);
        report_name = GetReportName(self.cursor, report_id)

        # sparta_assert_context(report_name.length() > merge_top_name.length(),
        #     "Expected the current report name (" << report_name << ") " <<
        #     " to be long than the top-level report name (" << merge_top_name << ")");
        if len(report_name) <= len(merge_top_name):
            raise ValueError(f"Expected the current report name ({report_name}) to be longer " \
                              "than the top-level report name ({merge_top_name})")

        # uint32_t substr_idx = 0;
        # for (substr_idx = 0; substr_idx < merge_top_name.length(); substr_idx++) {
        #     if (merge_top_name[substr_idx] != report_name[substr_idx]) {
        #         break;
        #     }
        # }
        substr_idx = 0
        for substr_idx in range(len(merge_top_name)):
            if merge_top_name[substr_idx] != report_name[substr_idx]:
                break

        # substr_idx++;  // skip over the '.'
        # sparta_assert(substr_idx < report_name.length());
        substr_idx += 1
        if substr_idx >= len(report_name):
            raise ValueError(f"Substring index {substr_idx} is out of range for report name {report_name}")

        # std::string stat_prefix = report_name.substr(substr_idx);
        # writeStats_(out, report, stat_prefix, all_stat_names);
        stat_prefix = report_name[substr_idx:]
        self.__WriteStats(out, report_id, descriptor_id, stat_prefix, all_stat_names)

        # if (report.getNumSubreports() > 0) {
        #     mergeReportList_(out, report.getSubreports(), merge_top_name, all_stat_names);
        # }
        subrep_ids = GetSubreportIDs(self.cursor, report_id)
        if len(subrep_ids) > 0:
            self.__MergeReportList(out, subrep_ids, descriptor_id, merge_top_name, all_stat_names)

    def __MergeReportList(self, out, subrep_ids, descriptor_id, merge_top_name, all_stat_names):
        # for (const auto & r : reports) {
        #     mergeReport_(out, r, merge_top_name, all_stat_names);
        # }
        for report_id in subrep_ids:
            self.__MergeReport(out, report_id, descriptor_id, merge_top_name, all_stat_names)

    def __WriteStats(self, out, report_id, descriptor_id, stat_prefix, all_stat_names):
        # const statistics::StatisticPairs& stats = report.getStatistics();
        report_stats = GetReportStatInsts(self.cursor, report_id, self.stat_values_getter)

        # for (uint32_t idx = 0; idx < stats.size(); idx++) {
        for idx in range(len(report_stats)):
            # const statistics::stat_pair_t & si = stats[idx];
            si = report_stats[idx]

            # std::string sname = stat_prefix;
            # if (si.first == "") {
            #     // Use location as stat name since report creator did not give it an
            #     // explicit name. Since ths full path is used, do not append the
            #     // stat_prefix because it would make the resulting stat name very
            #     // confusing.
            #     sname = si.second->getLocation();
            # } else {
            #     if (sname.length() > 0) {
            #         sname += ".";
            #     }
            #     sname += si.first;
            # }
            sname = stat_prefix
            if not si.GetName():
                sname = si.GetLocation()
            else:
                if len(sname) > 0:
                    sname += "."
                sname += si.GetName()

            # all_stat_names.push_back(sname);
            # out << "      \"" << sname << "\": { ";
            # double val = si.second->getValue();
            # out << "\"val\" : ";
            # if (isnan(val)){
            #     out << "\"nan\"";
            # } else if (isinf(val)) {
            #     out << "\"inf\"";
            # } else{
            #     out << Report::formatNumber(val, false, decimal_places_);
            # }
            all_stat_names.append(sname)
            out[sname] = OrderedDict([("val", si.GetValue())])

            # out << ", \"vis\" : " << si.second->getVisibility();
            # out << ", \"desc\" : \"" << desc << "\"},";
            # out << "\n";
            out[sname]["vis"] = si.GetVis()
            out[sname]["desc"] = si.GetDesc()

    def __IsLeafNode(self, report_id):
        # std::string report_name = getReportName_(report);
        report_name = GetReportName(self.cursor, report_id)

        # for (const std::string & cmp_name : leaf_nodes_) {
        #     if (report_name == cmp_name) {
        #         return true;
        #     }
        # }
        if report_name in self.leaf_nodes:
            return True

        # report_name += ".";
        # for (const std::string & cmp_name : parents_of_leaf_nodes_) {
        #     if (boost::starts_with(report_name, cmp_name) && (report_name != cmp_name)) {
        #         return true;
        #     }
        # }
        # return false;
        report_name += "."
        for cmp_name in self.parents_of_leaf_nodes:
            if report_name.startswith(cmp_name) and report_name != cmp_name:
                return True

        return False
