import os, sys
import argparse
import shutil
import sqlite3

from exporters.export_csv_report import CSVReportExporter
from exporters.export_json_report import JSONReportExporter
from exporters.export_json_report import JSONReducedReportExporter
from exporters.export_json_report import JSONDetailReportExporter
from exporters.export_text_report import TextReportExporter
from exporters.export_html_report import HTMLReportExporter
from exporters.export_js_report import JSReportExporter
from exporters.export_py_report import PyReportExporter
from exporters.export_gnuplot_report import GnuplotReportExporter
from exporters.export_stats_mapping_report import StatsMappingReportExporter

parser = argparse.ArgumentParser(description="Export SimDB records to formatted report files.")
parser.add_argument("--dest-file", type=str, help="Export only this one destination file (out.csv etc. from the defn yaml).")
parser.add_argument("--export-dir", type=str, default=".", help="Directory to save the exported report files.")
parser.add_argument("--force", action="store_true", help="Force overwrite export directory/reports.")
parser.add_argument("database", type=str, help="Path to the SimDB database file.")
args = parser.parse_args()

if not os.path.exists(args.database):
    print(f"Error: The specified database file '{args.database}' does not exist.")
    sys.exit(1)

export_dir_is_cwd = os.path.abspath(args.export_dir) == os.getcwd()
if os.path.exists(args.export_dir) and not export_dir_is_cwd:
    if args.force:
        shutil.rmtree(args.export_dir)
    else:
        print(f"Error: The export directory '{args.export_dir}' already exists. Use --force to overwrite.")
        sys.exit(1)

if not export_dir_is_cwd:
    os.makedirs(args.export_dir)

conn = sqlite3.connect(args.database)
cursor = conn.cursor()

# Get the appropriate exporter for the given report format.
def GetExporter(format):
    format = format.lower()
    if format in ('txt', 'text'):
        return TextReportExporter()
    if format in ('html', 'htm'):
        return HTMLReportExporter()
    if format in ('csv', 'csv_cumulative'):
        return CSVReportExporter()
    if format in ('js_json', 'jsjson'):
        return JSReportExporter()
    if format in ('python', 'py'):
        return PyReportExporter()
    if format in ('json'):
        return JSONReportExporter()
    if format in ('json_reduced'):
        return JSONReducedReportExporter()
    if format in ('json_detail'):
        return JSONDetailReportExporter()
    if format in ('gnuplot', 'gplt'):
        return GnuplotReportExporter()
    if format in ('stats_mapping'):
        return StatsMappingReportExporter()

    print (f"Unknown report format '{format}'")
    sys.exit(1)

cmd = "SELECT Id, DestFile, Format FROM ReportDescriptors"
if args.dest_file:
    cmd += f" WHERE DestFile = '{args.dest_file}'"
cursor.execute(cmd)

rows = cursor.fetchall()
existing_files = []
for _, dest_file, _ in rows:
    dest_file = os.path.join(args.export_dir, dest_file)
    if os.path.exists(dest_file):
        if not args.force:
            existing_files.append(dest_file)
        else:
            os.remove(dest_file)

if existing_files:
    print(f"Error: The following destination files already exist. Use --force to overwrite:")
    for file in existing_files:
        print(f"  {file}")
    sys.exit(1)

for id, dest_file, format in rows:
    dest_file = os.path.join(args.export_dir, dest_file)
    exporter = GetExporter(format)
    exporter.Export(dest_file, id, conn)
    print (f"Exported: {dest_file}")
