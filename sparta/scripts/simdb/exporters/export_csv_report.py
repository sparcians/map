import os, zlib, struct
from .utils import FormatNumber

class CSVReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        cursor = db_conn.cursor()

        # Start with the CSV header, which looks something like this:
        #   report="autopop_all.yaml on _SPARTA_global_node_",start=0,end=SIMULATION_END,report_format=csv
        cmd = f'SELECT Id,Name,StartTick,EndTick FROM Reports WHERE ReportDescID={descriptor_id} AND ParentReportID=0'
        cursor.execute(cmd)
        base_report_id, report_name, start_tick, end_tick = cursor.fetchone()

        cmd = f'SELECT MetaName, MetaValue FROM ReportMetadata WHERE ReportDescID={descriptor_id} '
        cmd += 'AND MetaName NOT IN (\'OmitZeros\', \'PrettyPrint\')'

        # Sort alphabetically by name to match std::map<string, string> in C++.
        cmd += ' ORDER BY MetaName ASC'

        cursor.execute(cmd)

        meta_kvpairs = []
        for meta_name, meta_value in cursor.fetchall():
            meta_kvpairs.append((meta_name, meta_value))

        with open(dest_file, 'w') as fout:
            self.__WriteHeader(fout, report_name, start_tick, end_tick, meta_kvpairs)

        # Now go through this descriptor's reports/subreports, and get an ordered list of
        # the statistics that are in the report. This is line #2 of the CSV file (column
        # headers).
        stat_headers = []
        self.__RecurseGetStatHeaders(cursor, base_report_id, '', stat_headers)

        with open(dest_file, 'a') as fout:
            # Write the header line.
            fout.write(','.join(stat_headers))
            fout.write('\n')

        # Lastly, deserialize the raw values. Each record in this query corresponds to another
        # row in the CSV report (one SQL record only holds the values for the same report descriptor).
        with open(dest_file, 'a') as fout:
            basename = os.path.basename(dest_file)
            cmd = f'SELECT Data, IsCompressed FROM CollectionRecords WHERE Notes=\'{basename}\' ORDER BY Tick ASC'
            cursor.execute(cmd)
            for data, is_compressed in cursor.fetchall():
                if is_compressed:
                    data = zlib.decompress(data)

                # The data values are stored as:
                #   [elem_id(u16), value(double), elem_id(u16), value(double), ...]
                #
                # We only care about the values, not the element IDs. Everything in these records
                # is already in the order we want, meaning that the values line up with the stat
                # headers we already wrote out.
                #
                # We could assert that the encountered element IDs correspond to the headers,
                # although that would be a bit of a performance hit. The SimDB verification
                # tests would be failing if this was not the case, so we will skip that for now.
                elem_id_size = 2
                value_size = 8

                row_values = []
                for i in range(elem_id_size, len(data), elem_id_size + value_size):
                    value = struct.unpack('d', data[i:i + value_size])[0]
                    row_values.append(FormatNumber(value))

                fout.write(','.join(row_values))
                fout.write('\n')

    def __WriteHeader(self, fout, report_name, start_tick, end_tick, meta_kvpairs):
        if end_tick == -1:
            end_tick = 'SIMULATION_END'
        fout.write(f'# report="{report_name}",start={start_tick},end={end_tick}')

        if not meta_kvpairs:
            fout.write('\n')
            return

        for i, (meta_name, meta_value) in enumerate(meta_kvpairs):
            meta_kvpairs[i] = f'{meta_name}={meta_value}'

        fout.write(',')
        fout.write(','.join(meta_kvpairs))
        fout.write('\n')

    def __RecurseGetStatHeaders(self, cursor, report_id, prefix, stat_headers):
        cmd = f'SELECT StatisticName, StatisticLoc FROM StatisticInsts WHERE ReportID={report_id}'
        cursor.execute(cmd)

        for stat_name, stat_loc in cursor.fetchall():
            if stat_name:
                stat_headers.append(prefix + stat_name)
            else:
                stat_headers.append(prefix + stat_loc)

        # Now, get the subreports and recurse.
        cmd = f'SELECT Id, Name FROM Reports WHERE ParentReportID={report_id}'
        cursor.execute(cmd)
        for subreport_id, subreport_name in cursor.fetchall():
            self.__RecurseGetStatHeaders(cursor, subreport_id, subreport_name+'.', stat_headers)
