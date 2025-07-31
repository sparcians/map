import os, zlib, struct
from .utils import FormatNumber
import numpy as np

class CSVReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn, cmdline_args):
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

        # Force the "report_format" metadata to be visible at the top of the CSV file.
        has_report_format = False
        for meta_name, meta_value in meta_kvpairs:
            if meta_name == 'report_format':
                has_report_format = True
                break

        if not has_report_format:
            cmd = f'SELECT Format FROM ReportDescriptors WHERE Id={descriptor_id}'
            cursor.execute(cmd)
            format = cursor.fetchone()[0]
            meta_kvpairs.append(('report_format', format))

        # We write multi-line headers for start/stop/update counter locations.
        trigger_locs = []
        cmd = 'SELECT StartCounter, StopCounter, UpdateCounter FROM Reports WHERE '
        cmd += f'ReportDescID={descriptor_id} AND ParentReportID=0'
        cursor.execute(cmd)

        start_counter_loc, stop_counter_loc, update_counter_loc = cursor.fetchone()
        if start_counter_loc:
            trigger_locs.append(('start_counter', start_counter_loc))
        if stop_counter_loc:
            trigger_locs.append(('stop_counter', stop_counter_loc))
        if update_counter_loc:
            trigger_locs.append(('update_counter', update_counter_loc))

        with open(dest_file, 'w') as fout:
            self.__WriteHeader(fout, report_name, start_tick, end_tick, meta_kvpairs, trigger_locs)

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
            # We need to ensure that we "interleave" the records from the CollectionRecords table
            # and the CsvSkipAnnotations table before writing them to the CSV file.
            csv_row_text_by_tick = {}

            cmd = f'SELECT Tick, DataBlob FROM DescriptorRecords WHERE ReportDescID={descriptor_id}'
            cursor.execute(cmd)
            for tick, data in cursor.fetchall():
                data = zlib.decompress(data)
                dt = np.dtype([('value', '<f8')])
                records = np.frombuffer(data, dtype=dt)
                double_values = records['value']
                row_values = [FormatNumber(value) for value in double_values]
                row_text = ','.join(row_values)
                csv_row_text_by_tick[tick] = row_text

            # Now get all the skipped annotations for this report descriptor, if any.
            cmd = f'SELECT Tick, Annotation FROM CsvSkipAnnotations WHERE ReportDescID={descriptor_id}'
            cursor.execute(cmd)
            for tick, anno in cursor.fetchall():
                anno += ','*(len(stat_headers) - 1)
                csv_row_text_by_tick[tick] = anno

            # Now write all the rows in order.
            ticks = sorted(csv_row_text_by_tick.keys())
            for tick in ticks:
                row_text = csv_row_text_by_tick[tick]
                fout.write(row_text)
                fout.write('\n')

    def __WriteHeader(self, fout, report_name, start_tick, end_tick, meta_kvpairs, trigger_locs):
        start_tick = int(start_tick)
        if end_tick == '18446744073709551615':
            end_tick = 'SIMULATION_END'
        else:
            end_tick = int(end_tick)
        fout.write(f'# report="{report_name}",start={start_tick},end={end_tick}')

        if not meta_kvpairs:
            fout.write('\n')
            return

        for i, (meta_name, meta_value) in enumerate(meta_kvpairs):
            meta_kvpairs[i] = f'{meta_name}={meta_value}'

        fout.write(',')
        fout.write(','.join(meta_kvpairs))
        fout.write('\n')

        # Write the trigger locations if provided
        if trigger_locs:
            fout.write('# ')
            strs = []
            for name, loc in trigger_locs:
                strs.append(f'{name}={loc}')
            fout.write(','.join(strs))
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
