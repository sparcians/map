import os, sys
import argparse
import shutil
import sqlite3

parser = argparse.ArgumentParser(description="Export SimDB records to formatted report files.")
parser.add_argument("--db-file", type=str, required=True, help="Path to the SimDB database file.")
parser.add_argument("--dest-file", type=str, help="Export only this one destination file (out.csv etc. from the defn yaml).")
parser.add_argument("--export-dir", type=str, default="simdb_reports", help="Directory to save the exported report files.")
parser.add_argument("--force", action="store_true", help="Force overwrite export directory.")
args = parser.parse_args()

if not os.path.exists(args.db_file):
    print(f"Error: The specified database file '{args.db_file}' does not exist.")
    sys.exit(1)

if os.path.exists(args.export_dir):
    if args.force:
        shutil.rmtree(args.export_dir)
    else:
        print(f"Error: The export directory '{args.export_dir}' already exists. Use --force to overwrite.")
        sys.exit(1)

os.makedirs(args.export_dir)
conn = sqlite3.connect(args.db_file)
cursor = conn.cursor()

cmd = "SELECT LocPattern, DefFile, DestFile, Format FROM ReportDescriptors"
if args.dest_file:
    cmd += f" WHERE DestFile = '{args.dest_file}'"
cursor.execute(cmd)

# For now, just "touch" each report file. The comparison script will naturally
# fail which is okay for now.
for _, _, dest_file, _ in cursor.fetchall():
    dest_file = os.path.join(args.export_dir, dest_file)
    with open(dest_file, "w") as fout:
        print (f"Exporting {dest_file}...")
        fout.write("# This is a placeholder file. The SimDB exporter is not implemented yet.\n")
