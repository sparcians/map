# Note the use of OrderedDict is to ensure we can match the rapidjson C++
# legacy formatters exactly.
from collections import OrderedDict
import os, json, zlib, struct
from .utils import FormatNumber

class StatValueGetter:
    def __init__(self, stats_values):
        self.stats_values = stats_values
        self.index = 0

    def GetNext(self):
        if self.index >= len(self.stats_values):
            raise IndexError("No more values in stats blob")
        value = self.stats_values[self.index]
        self.index += 1
        return value

def GetStatsValuesGetter(cursor, dest_file):
    dest_file_name = os.path.basename(dest_file)
    cmd = f"SELECT Data, IsCompressed FROM CollectionRecords WHERE Notes='{dest_file_name}'"
    cursor.execute(cmd)
    stats_blob, is_compressed = cursor.fetchone()
    if is_compressed:
        stats_blob = zlib.decompress(stats_blob)

    # Turn the stats blob (byte vector) into a vector of doubles.
    assert len(stats_blob) % (2+8) == 0, "Invalid stats blob length"
    stats_values = []
    for i in range(0, len(stats_blob), 2+8):
        # The first value is the collectable ID, the second is the value.
        # We don't need the collectable ID here.
        val = struct.unpack("d", stats_blob[i+2:i+10])[0]
        val = FormatNumber(val, as_string=False)
        stats_values.append(val)

    return StatValueGetter(stats_values)

def GetJsonReportMetadata(cursor, descriptor_id, json_format):
    cmd = f"SELECT MetaName, MetaValue FROM ReportMetadata WHERE ReportDescID = {descriptor_id}"
    cmd += " AND MetaName NOT IN ('OmitZeros', 'PrettyPrint')"
    cursor.execute(cmd)

    report_metadata = OrderedDict([
        ("report_format", json_format)
    ])
    for name, value in cursor.fetchall():
        report_metadata[name] = value

    return report_metadata

def GetSimInfo(cursor):
    cmd = "SELECT SimName, SimVersion, SpartaVersion, ReproInfo FROM SimulationInfo"
    cursor.execute(cmd)

    sim_name, sim_version, sparta_version, repro_info = cursor.fetchone()
    json_version = "2.1"
    siminfo = OrderedDict([
        ("name", sim_name),
        ("sim_version", sim_version),
        ("sparta_version", sparta_version),
        ("json_report_version", json_version),
        ("reproduction", repro_info)
    ])

    return siminfo

def GetVisibilities(cursor):
    cmd = "SELECT Hidden, Support, Detail, Normal, Summary, Critical FROM Visibilities"
    cursor.execute(cmd)
    hidden, support, detail, normal, summary, critical = cursor.fetchone()
    vis = OrderedDict([
        ("hidden", hidden),
        ("support", support),
        ("detail", detail),
        ("normal", normal),
        ("summary", summary),
        ("critical", critical)
    ])

    return vis

def GetReportStats(cursor, report_id):
    cmd = f"SELECT StatisticName, StatisticLoc, StatisticDesc, StatisticVis FROM StatisticInsts WHERE ReportID = {report_id}"
    cursor.execute(cmd)

    stats = []
    for stat_name, stat_loc, stat_desc, stat_vis in cursor.fetchall():
        if not stat_name:
            stat_name = stat_loc

        stats.append({
            "name": stat_name,
            "loc": stat_loc,
            "desc": stat_desc,
            "vis": stat_vis
        })

    return stats

def GetStatsNestedDict(cursor, descriptor_id, parent_report_id, stat_value_getter, format):
    omit_zeros = False
    if format == "json_reduced":
        cmd = f"SELECT MetaValue FROM ReportMetadata WHERE ReportDescID = {descriptor_id}"
        cmd += " AND MetaName == 'OmitZeros'"
        cursor.execute(cmd)

        row = cursor.fetchone()
        if row:
            omit_zeros = row[0].lower() == "true"

    def Impl(cursor, descriptor_id, parent_report_id, ordered_dict, stat_value_getter, omit_zeros):
        cmd = f"SELECT Id, Name FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = {parent_report_id}"
        cursor.execute(cmd)

        for report_id, name in cursor.fetchall():
            flattened_name = name.split(".")[-1]
            ordered_dict[flattened_name] = OrderedDict()

            report_stats = GetReportStats(cursor, report_id)
            ordered_keys = []
            for stat in report_stats:
                stat_name = stat["name"]
                stat_val = stat_value_getter.GetNext()

                if omit_zeros and stat_val == 0:
                    continue

                if format == "json":
                    stat_desc = stat["desc"]
                    stat_vis = stat["vis"]

                    stat_dict = OrderedDict([
                        ("desc", stat_desc),
                        ("vis", stat_vis),
                        ("val", stat_val)
                    ])

                    ordered_dict[flattened_name][stat_name] = stat_dict
                    ordered_keys.append(stat_name)
                elif format == "json_reduced":
                    ordered_dict[flattened_name][stat_name] = stat_val
                else:
                    raise ValueError(f"Unknown format: {format}")

            if ordered_keys:
                ordered_dict[flattened_name]["ordered_keys"] = ordered_keys

            Impl(cursor, descriptor_id, report_id, ordered_dict[flattened_name], stat_value_getter, omit_zeros)

    nested_dict = OrderedDict()
    Impl(cursor, descriptor_id, parent_report_id, nested_dict, stat_value_getter, omit_zeros)
    return nested_dict

class JSONReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        cursor = db_conn.cursor()
        report_metadata = GetJsonReportMetadata(cursor, descriptor_id, "json")
        siminfo = GetSimInfo(cursor)
        vis = GetVisibilities(cursor)

        stat_value_getter = GetStatsValuesGetter(cursor, dest_file)
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

        stat_value_getter = GetStatsValuesGetter(cursor, dest_file)
        statistics = GetStatsNestedDict(cursor, descriptor_id, 0, stat_value_getter, "json_reduced")

        json_out = OrderedDict([
            ("Statistics", statistics),
            ("vis", vis),
            ("siminfo", siminfo),
            ("report_metadata", report_metadata)
        ])

        with open(dest_file, "w") as fout:
            json.dump(json_out, fout, indent=4)

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
