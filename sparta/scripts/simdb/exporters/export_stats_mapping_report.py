from collections import OrderedDict
import json

class StatsMappingReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        # Get the root report ID
        cmd = f"SELECT Id FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = 0"
        cursor = db_conn.cursor()
        cursor.execute(cmd)
        row = cursor.fetchone()
        if row is None:
            raise ValueError(f"Report descriptor {descriptor_id} does not have a root report")

        root_report_id = row[0]
        mapping = {}
        self.__RecursivelyGetStatsMapping(db_conn, root_report_id, descriptor_id, "", mapping)

        reverse_mapping = {}
        for key, value in mapping.items():
            reverse_mapping[value] = key

        headers2stats = OrderedDict()
        stats2headers = OrderedDict()

        headers = list(mapping.keys())
        headers.sort()

        for header in headers:
            stat_loc = mapping[header]
            headers2stats[header] = stat_loc

        stat_locs = list(reverse_mapping.keys())
        stat_locs.sort()
        for stat_loc in stat_locs:
            header = reverse_mapping[stat_loc]
            stats2headers[stat_loc] = header

        out_json = OrderedDict([
            ("Column-header-to-statistic", headers2stats),
            ("Statistic-to-column-header", stats2headers),
            ("report_metadata", { "report_format": "stats_mapping" })
        ])

        with open(dest_file, "w") as fout:
            json.dump(out_json, fout, indent=4)

    def __RecursivelyGetStatsMapping(self, db_conn, report_id, descriptor_id, prefix, mapping):
        # Get all statistics for this report
        cursor = db_conn.cursor()
        cmd = f"SELECT StatisticName, StatisticLoc FROM StatisticInsts WHERE ReportID = {report_id}"
        cursor.execute(cmd)

        for name, loc in cursor.fetchall():
            if name:
                mapping[prefix + name] = loc
            else:
                mapping[prefix + loc] = loc

        # Recursive into subreports
        cmd = f"SELECT Id, Name FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = {report_id}"
        cursor = db_conn.cursor()
        cursor.execute(cmd)

        for row in cursor.fetchall():
            sub_report_id = row[0]
            sub_report_name = row[1]
            prefix = sub_report_name + "."
            self.__RecursivelyGetStatsMapping(db_conn, sub_report_id, descriptor_id, prefix, mapping)
