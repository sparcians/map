from collections import OrderedDict
import os, zlib, struct, math
from .utils import FormatNumber

class StatisticInstance:
    def __init__(self, name, location, desc, vis, stat_value_getter):
        self.name = name
        self.location = location
        self.desc = desc
        self.vis = vis
        self.stat_value_getter = stat_value_getter

    def GetName(self):
        return self.name
    
    def GetLocation(self):
        return self.location

    def GetDesc(self, unused=False):
        assert unused == False
        return self.desc

    def GetVis(self):
        return self.vis

    def GetValue(self):
        return self.stat_value_getter.GetValueByLocation(self.location)

class StatValueGetter:
    def __init__(self, stats_values, stat_values_by_loc):
        self.stats_values = stats_values
        self.stat_values_by_loc = stat_values_by_loc
        self.index = 0

    def GetNext(self):
        if self.index >= len(self.stats_values):
            raise IndexError("No more values in stats blob")
        value = self.stats_values[self.index]
        self.index += 1
        return value

    def GetValueByLocation(self, loc):
        if loc not in self.stat_values_by_loc:
            raise KeyError(f"Location {loc} not found in stats blob")
        return self.stat_values_by_loc[loc]

def GetStatsValuesGetter(cursor, dest_file, replace_nan_with_nanstring=False, replace_inf_with_infstring=False):
    dest_file_name = os.path.basename(dest_file)
    cmd = f"SELECT Data, IsCompressed FROM CollectionRecords WHERE Notes='{dest_file_name}'"
    cursor.execute(cmd)
    stats_blob, is_compressed = cursor.fetchone()
    if is_compressed:
        stats_blob = zlib.decompress(stats_blob)

    # Turn the stats blob (byte vector) into a vector of doubles.
    assert len(stats_blob) % (2+8) == 0, "Invalid stats blob length"
    stats_values = []
    coll_ids = []
    for i in range(0, len(stats_blob), 2+8):
        val = struct.unpack("d", stats_blob[i+2:i+10])[0]
        if replace_nan_with_nanstring and math.isnan(val):
            val = "nan"
        elif replace_inf_with_infstring and math.isinf(val):
            val = "inf"
        else:
            val = FormatNumber(val, as_string=False)

        stats_values.append(val)
        cid = struct.unpack("H", stats_blob[i:i+2])[0]
        coll_ids.append(cid)

    # The above stats_values (linear list) is to be used when the entire stat
    # collection is to be read one stat at a time using the GetNext() method.
    # Some uses need to access stat values by location.
    #
    # We can use the collectable ID to get the location of the stat since
    # the location/collectable ID pair is stored in the CollectableTreeNodes
    # table.
    stat_locs_by_coll_id = {}
    cmd = "SELECT ElementTreeNodeID, Location FROM CollectableTreeNodes"
    cursor.execute(cmd)
    for coll_id, loc in cursor.fetchall():
        stat_locs_by_coll_id[coll_id] = loc

    stat_values_by_coll_id = {}
    for cid, val in zip(coll_ids, stats_values):
        stat_values_by_coll_id[cid] = val

    stat_values_by_loc = {}
    for cid, val in stat_values_by_coll_id.items():
        loc = stat_locs_by_coll_id[cid]
        stat_values_by_loc[loc] = val

    return StatValueGetter(stats_values, stat_values_by_loc)

def HasStatistics(cursor, descriptor_id, report_id, recursive=False):
    def Impl(cursor, descriptor_id, report_id, recursive):
        cmd = f"SELECT COUNT(*) FROM StatisticInsts WHERE ReportID = {report_id}"
        cursor.execute(cmd)
        count = cursor.fetchone()[0]
        if count > 0:
            return True
        if not recursive:
            return False

        for subrep_id in GetSubreportIDs(cursor, descriptor_id, report_id):
            if Impl(cursor, descriptor_id, subrep_id, recursive):
                return True

        return False

    return Impl(cursor, descriptor_id, report_id, recursive)

def GetReportStatDicts(cursor, report_id):
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

def GetReportStatInsts(cursor, report_id, stat_value_getter=None):
    cmd = f"SELECT StatisticName, StatisticLoc, StatisticDesc, StatisticVis FROM StatisticInsts WHERE ReportID = {report_id}"
    cursor.execute(cmd)

    statistics = []
    for name, loc, desc, vis in cursor.fetchall():
        statistics.append(StatisticInstance(name, loc, desc, vis, stat_value_getter))

    return statistics

def GetReportName(cursor, report_id):
    cmd = f"SELECT Name FROM Reports WHERE Id = {report_id}"
    cursor.execute(cmd)
    row = cursor.fetchone()

    if not row:
        return None

    report_name = row[0]
    return report_name

def GetSubreportIDs(cursor, descriptor_id, parent_report_id):
    cmd = f"SELECT Id FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = {parent_report_id}"
    cursor.execute(cmd)

    subrep_ids = []
    for row in cursor.fetchall():
        subrep_ids.append(row[0])

    return subrep_ids

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

            report_stats = GetReportStatDicts(cursor, report_id)
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

def GetReportStyle(cursor, report_id, descriptor_id, style_name, style_default=None):
    cmd = f"SELECT StyleValue FROM ReportStyles WHERE ReportDescID = {descriptor_id} AND ReportID = {report_id} AND StyleName = '{style_name}'"
    cursor.execute(cmd)

    style_value = cursor.fetchone()
    return style_value[0] if style_value else style_default
