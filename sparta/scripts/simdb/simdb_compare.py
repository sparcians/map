import os, sys
import argparse
import shutil
import sqlite3
import io
from compare_utils import *

parser = argparse.ArgumentParser(description="Compare SimDB reports with legacy reports.")
parser.add_argument("--legacy-reports-dir", default=".", help="Directory containing legacy reports.")
parser.add_argument("--results-dir", default="simdb-comparison-results", help="Directory to save comparison results.")
parser.add_argument("--force", action="store_true", help="Force overwrite results directory.")
parser.add_argument("--quiet", action="store_true", help="Suppress output.")
parser.add_argument("database", type=str, help="Path to the SimDB database file.")
args = parser.parse_args()

if not os.path.exists(args.database):
    print(f"Error: The specified database file '{args.database}' does not exist.")
    sys.exit(1)

legacy_reports_dir_out = os.path.join(args.results_dir, "legacy_reports")
legacy_reports_dir_in = args.legacy_reports_dir
if not os.path.exists(legacy_reports_dir_in):
    print(f"Error: The specified legacy reports directory '{legacy_reports_dir_in}' does not exist.")
    sys.exit(1)

if os.path.exists(args.results_dir):
    if args.force:
        shutil.rmtree(args.results_dir)
    else:
        print(f"Error: The results directory '{args.results_dir}' already exists. Use --force to overwrite.")
        sys.exit(1)

# Copy all the legacy reports to <results_dir>/legacy_reports
os.makedirs(legacy_reports_dir_out)

# Copy the database file to the results directory
db_file_name = os.path.basename(args.database)
db_file_out = os.path.join(args.results_dir, db_file_name)
shutil.copy2(args.database, db_file_out)

conn = sqlite3.connect(args.database)
cursor = conn.cursor()

cmd = "SELECT DestFile FROM ReportDescriptors"
cursor.execute(cmd)
baseline_reports = []
for row in cursor.fetchall():
    dest_file = row[0]
    baseline_reports.append(dest_file)
    src_path = os.path.join(legacy_reports_dir_in, dest_file)
    dst_path = os.path.join(legacy_reports_dir_out, dest_file)
    assert os.path.exists(src_path)
    shutil.copy2(src_path, dst_path)

# Export all SimDB reports to <results_dir>/simdb_reports
simdb_reports_dir_out = os.path.join(args.results_dir, "simdb_reports")
scripts_dir = os.path.dirname(os.path.abspath(__file__))
export_py = os.path.join(scripts_dir, "simdb_export.py")
export_cmd = f"python3 {export_py} --export-dir {simdb_reports_dir_out} --force {args.database}"
if args.quiet:
    export_cmd += " > /dev/null 2>&1"
os.system(export_cmd)

# Run the comparisons and generate the final comparison report.
logger = io.StringIO()
results = RunComparison(legacy_reports_dir_out, simdb_reports_dir_out, baseline_reports, logger)

passing_reports = results["passing"]
failing_reports = results["failing"]
unsupported_reports = results["unsupported"]

logger.write("\nSummary of Comparison Results...\n")

if passing_reports:
    logger.write("PASSING:\n")
    for report in passing_reports:
        logger.write(f"  {report}\n")

if failing_reports:
    logger.write("FAILING:\n")
    for report in failing_reports:
        logger.write(f"  {report}\n")

if unsupported_reports:
    logger.write("UNSUPPORTED:\n")
    for report in unsupported_reports:
        logger.write(f"  {report}\n")

logfile = os.path.join(args.results_dir, "comparison_results.txt")
with open(logfile, "w") as fout:
    fout.write(logger.getvalue())

if unsupported_reports:
    print(f"{len(passing_reports)} passed, {len(failing_reports)} failed, {len(unsupported_reports)} unsupported.")
else:
    print(f"{len(passing_reports)} passed, {len(failing_reports)} failed.")

print(f"Comparison results saved to {logfile}")
