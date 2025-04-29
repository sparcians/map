class CSVReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        cursor = db_conn.cursor()
        cmd = f'SELECT Name,StartTick,EndTick FROM Reports WHERE ReportDescID={descriptor_id} AND ParentReportID=0'
        cursor.execute(cmd)
        report_name, start_tick, end_tick = cursor.fetchone()

        cmd = f'SELECT MetaName, MetaValue FROM ReportMetadata WHERE ReportDescID={descriptor_id} '
        cmd += 'AND MetaName NOT IN (\'OmitZeros\', \'PrettyPrint\')'
        cursor.execute(cmd)

        meta_kvpairs = []
        for meta_name, meta_value in cursor.fetchall():
            meta_kvpairs.append((meta_name, meta_value))

        with open(dest_file, 'w') as fout:
            self.__WriteHeader(fout, report_name, start_tick, end_tick, meta_kvpairs)

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
