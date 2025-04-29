class JSONReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        # For now, just "touch" each report file. The comparison script will naturally
        # fail which is okay for now.
        with open(dest_file, "w") as fout:
            print (f"Exporting {dest_file}...")
            fout.write("# This is a placeholder file. The SimDB exporter is not implemented yet.\n")

class JSONReducedReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        # For now, just "touch" each report file. The comparison script will naturally
        # fail which is okay for now.
        with open(dest_file, "w") as fout:
            print (f"Exporting {dest_file}...")
            fout.write("# This is a placeholder file. The SimDB exporter is not implemented yet.\n")

class JSONDetailReportExporter:
    def __init__(self):
        pass

    def Export(self, dest_file, descriptor_id, db_conn):
        # For now, just "touch" each report file. The comparison script will naturally
        # fail which is okay for now.
        with open(dest_file, "w") as fout:
            print (f"Exporting {dest_file}...")
            fout.write("# This is a placeholder file. The SimDB exporter is not implemented yet.\n")
