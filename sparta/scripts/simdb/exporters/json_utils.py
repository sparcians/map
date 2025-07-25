from collections import OrderedDict

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
