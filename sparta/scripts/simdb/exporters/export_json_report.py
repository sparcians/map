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
    def Impl(cursor, descriptor_id, parent_report_id, ordered_dict, stat_value_getter):
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

            Impl(cursor, descriptor_id, report_id, ordered_dict[flattened_name], stat_value_getter)

    nested_dict = OrderedDict()
    Impl(cursor, descriptor_id, parent_report_id, nested_dict, stat_value_getter)
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
        report_metadata = GetJsonReportMetadata(cursor, descriptor_id, "json")
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
        # For now, just "touch" each report file. The comparison script will naturally
        # fail which is okay for now.
        with open(dest_file, "w") as fout:
            print (f"Exporting {dest_file}...")
            fout.write("# This is a placeholder file. The SimDB exporter is not implemented yet.\n")
