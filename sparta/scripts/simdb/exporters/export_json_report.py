from .json_utils import *
from .stat_utils import *
from .sim_utils import *
from collections import OrderedDict
import json

class JSONReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        cursor = db_conn.cursor()
        report_metadata = GetJsonReportMetadata(cursor, descriptor_id, "json")
        siminfo = GetSimInfo(cursor)
        vis = GetVisibilities(cursor)

        stat_value_getter = GetStatsValuesGetter(cursor, dest_file, replace_nan_with_nanstring=True)
        statistics = GetStatsNestedDict(cursor, descriptor_id, 0, stat_value_getter, "json")

        json_out = OrderedDict([
            ("Statistics", statistics),
            ("vis", vis),
            ("siminfo", siminfo),
            ("report_metadata", report_metadata)
        ])

        with open(dest_file, "w") as fout:
            json.dump(json_out, fout, indent=4)

class JSONReducedReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        cursor = db_conn.cursor()
        report_metadata = GetJsonReportMetadata(cursor, descriptor_id, "json_reduced")
        siminfo = GetSimInfo(cursor)
        vis = GetVisibilities(cursor)

        stat_value_getter = GetStatsValuesGetter(cursor, dest_file, replace_nan_with_nanstring=True)
        statistics = GetStatsNestedDict(cursor, descriptor_id, 0, stat_value_getter, "json_reduced")

        json_out = OrderedDict([
            ("Statistics", statistics),
            ("vis", vis),
            ("siminfo", siminfo),
            ("report_metadata", report_metadata)
        ])

        with open(dest_file, "w") as fout:
            json.dump(json_out, fout, indent=4)

        cmd = f"SELECT MetaName, MetaValue FROM ReportMetadata WHERE ReportDescID = {descriptor_id}"
        cmd += " AND MetaName = 'PrettyPrint'"
        cursor.execute(cmd)

        pretty_print = True
        row = cursor.fetchone()
        if row:
            pretty_print = row[1].lower() == 'true'

        if not pretty_print:
            # If not pretty printing, we need to remove all leading/trailing whitespace
            with open(dest_file, 'r') as fin:
                reformatted_lines = [line.strip() for line in fin.readlines()]

            with open(dest_file, 'w') as fout:
                fout.write('\n'.join(reformatted_lines))

class JSONDetailReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        report_paths_by_id = {}
        cursor = db_conn.cursor()
        self.__RecursivelyGetReportPaths(cursor, descriptor_id, 0, report_paths_by_id, "")

        stat_def_meta_by_si_id = {}
        cmd = "SELECT StatisticInstID, MetaName, MetaValue FROM StatisticDefnMetadata"
        cursor.execute(cmd)

        for si_id, meta_name, meta_value in cursor.fetchall():
            if si_id not in stat_def_meta_by_si_id:
                stat_def_meta_by_si_id[si_id] = OrderedDict()

            try:
                meta_value = int(meta_value)
            except ValueError:
                pass

            stat_def_meta_by_si_id[si_id][meta_name] = meta_value

        report_ids = list(report_paths_by_id.keys())
        detail_json_map = self.__GetStatDetails(cursor, report_ids, report_paths_by_id, stat_def_meta_by_si_id)

        report_metadata = GetJsonReportMetadata(cursor, descriptor_id, "json_detail")
        report_json = {
            "_id": " ",
            "report_metadata": report_metadata,
            "stat_info": OrderedDict()
        }

        si_names = list(detail_json_map.keys())
        si_names.sort()

        for si_name in si_names:
            report_json["stat_info"][si_name] = detail_json_map[si_name]

        with open(dest_file, "w") as fout:
            json.dump(report_json, fout, indent=2)

    def __RecursivelyGetReportPaths(self, cursor, descriptor_id, parent_report_id, report_paths_by_id, prefix):
        cmd = f"SELECT Id, Name FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = {parent_report_id}"
        cursor.execute(cmd)

        for report_id, name in cursor.fetchall():
            local_name = ""
            if prefix == "" or prefix.startswith("@ on _SPARTA_global_node_"):
                local_name = name.split(".")[-1]
            else:
                local_name = prefix + "." + name.split(".")[-1]

            report_paths_by_id[report_id] = local_name
            self.__RecursivelyGetReportPaths(cursor, descriptor_id, report_id, report_paths_by_id, local_name)

    def __GetStatDetails(self, cursor, report_ids, report_paths_by_id, stat_def_meta_by_si_id):
        cmd = "SELECT Id, ReportID, StatisticName, StatisticDesc, StatisticVis, StatisticClass FROM StatisticInsts"
        cmd += " WHERE ReportID IN (" + ",".join(map(str, report_ids)) + ")"
        cursor.execute(cmd)

        detail_json_map = {}
        for si_id, report_id, si_name, si_desc, si_vis, si_class in cursor.fetchall():
            if not si_name:
                continue

            if report_id not in report_ids:
                continue

            si_details = OrderedDict([
                ("name", report_paths_by_id[report_id] + "." + si_name),
                ("desc", si_desc),
                ("vis", str(si_vis)),
                ("class", str(si_class))
            ])

            if si_id in stat_def_meta_by_si_id:
                for meta_name, meta_value in stat_def_meta_by_si_id[si_id].items():
                    si_details[meta_name] = meta_value

            if si_name not in detail_json_map:
                detail_json_map[si_name] = []

            detail_json_map[si_name].append(si_details)

        return detail_json_map
