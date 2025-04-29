import os, sys
import argparse
import subprocess
import shutil
import re
import stat
import csv, json

script_dir = os.path.dirname(os.path.abspath(__file__))
report_yaml_dir = os.path.join(script_dir, "report_yamls")
refs_dir = os.path.join(script_dir, "refs")

parser = argparse.ArgumentParser(description="Verify the SimDB report exporters for all report configurations.")
parser.add_argument("--sim-exe-path", type=str, default="release/example/CoreModel/sparta_core_example",
                    help="Path to the Sparta executable.")
parser.add_argument("--report-yaml-dir", type=str, default=report_yaml_dir,
                    help="Directory containing the report definition YAML files to verify.")
parser.add_argument("--results-dir", type=str, default="simdb_verif_results",
                    help="Directory to store the pass/fail results and all baseline/simdb reports.")
parser.add_argument("--force", action="store_true", help="Force overwrite of the results directory if it exists.")

args = parser.parse_args()
if not os.path.exists(args.sim_exe_path):
    print(f"Error: The specified Sparta executable path does not exist: {args.sim_exe_path}")
    sys.exit(1)

if os.path.exists(args.results_dir):
    if args.force:
        print(f"Warning: The results directory already exists and will be overwritten: {args.results_dir}")
        shutil.rmtree(args.results_dir)
    else:
        print(f"Error: The results directory already exists. Use --force to overwrite: {args.results_dir}")
        sys.exit(1)

os.makedirs(args.results_dir)

# Helper to set the executable bit on a file (sim.cmd, export.cmd)
def MakeExecutable(filename):
    # Add user/group/other executable bits to the current permissions
    current_permissions = os.stat(filename).st_mode
    os.chmod(filename, current_permissions | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

# This class does the heavy lifting of running the sparta executable, exporting the reports
# from SimDB to the results dir, and comparing the results to the baseline reports.
class ReportVerifier:
    def __init__(self, sparta_exe_path, report_yaml_dir, results_dir):
        self.sparta_exe_path = os.path.abspath(sparta_exe_path)
        self.report_yaml_dir = os.path.abspath(report_yaml_dir)
        self.results_dir = os.path.abspath(results_dir)

    def RunAll(self):
        # Get the list of report YAML files
        report_yaml_files = [f for f in os.listdir(self.report_yaml_dir) if f.endswith(".yaml")]
        if not report_yaml_files:
            raise ValueError(f"No report YAML files found in the directory: {self.report_yaml_dir}")

        # Say the report YAML directory contained:
        #
        #     report_yamls/
        #         all_timeseries_formats.yaml
        #             - basic.csv
        #             - cumulative.csv
        #         all_json_formats.yaml
        #             - basic.json
        #             - basic_triggered.json
        #             - reduced.json
        #
        # Then the results directory might look like:
        #
        #     simdb_verif_results/
        #         PASSING/
        #             all_timeseries_formats/
        #                 sparta.db
        #                 sim.cmd
        #                 basic/
        #                     basic.csv
        #             all_json_formats/
        #                 sparta.db
        #                 sim.cmd
        #                 basic/
        #                     basic.json
        #                 basic_triggered/
        #                     basic_triggered.json
        #         FAILING/
        #             all_timeseries_formats/
        #                 sparta.db
        #                 sim.cmd
        #                 cumulative/
        #                     cumulative.csv
        #                     cumulative.simdb.csv
        #                     export.cmd
        #             all_json_formats/
        #                 sparta.db
        #                 sim.cmd
        #                 reduced/
        #                     reduced.json
        #                     reduced.simdb.json
        #                     export.cmd
        num_failing = 0
        num_passing = 0

        # Maintain a pass/fail list of tests that we ran for the final
        # birds-eye view of the report verification.
        birds_eye = {}
        for yamlfile in report_yaml_files:
            yamlfile_path = os.path.join(self.report_yaml_dir, yamlfile)
            fail_count, pass_count = self.__RunTest(yamlfile_path, birds_eye)
            num_failing += fail_count
            num_passing += pass_count

        passing_dir = os.path.join(self.results_dir, "PASSING")
        failing_dir = os.path.join(self.results_dir, "FAILING")

        if num_failing == 0:
            if os.path.exists(failing_dir):
                shutil.rmtree(failing_dir)

        if num_passing == 0:
            if os.path.exists(passing_dir):
                shutil.rmtree(passing_dir)

        # Print out the final report results:
        #
        #   PASSING:
        #       all_timeseries_formats:
        #           update_cycles.csv
        #           update_time.csv
        #       all_json_formats:
        #           basic.json
        #   FAILING:
        #       all_timeseries_formats:
        #           cumulative.csv
        #           update_time.csv
        print ('')
        for key in ['PASSING', 'FAILING']:
            if key in birds_eye:
                print (key+":")
                yamlfiles = birds_eye[key]
                yamlfiles = sorted(yamlfiles.keys())

                for yamlfile in yamlfiles:
                    print (f"    {yamlfile}:")
                    dest_files = birds_eye[key][yamlfile]
                    dest_files = sorted(dest_files)

                    for dest_file in dest_files:
                        print (f"        {dest_file}")

    def __RunTest(self, yamlfile_path, birds_eye):
        test_dir = os.path.join(self.results_dir, 'RUNNING')
        if os.path.exists(test_dir):
            shutil.rmtree(test_dir)
        os.makedirs(test_dir)

        db_file = os.path.join(test_dir, "sparta.db")
        calling_dir = os.getcwd()
        os.chdir(test_dir)

        # Copy <refs_dir/*.yaml> into the test directory
        for ref_file in os.listdir(refs_dir):
            if ref_file.endswith(".yaml"):
                shutil.copy(os.path.join(refs_dir, ref_file), test_dir)

        simargs = [self.sparta_exe_path, '-i', '20k', '--report', yamlfile_path, '--enable-simdb-reports', '--simdb-file', db_file]
        simcmd = ' '.join(simargs)
        print ('Running subprocess: ' + simcmd)
        subprocess.run(simargs)

        # Switch the sim.cmd to point the DB file to "rerun.db"
        simargs[-1] = "rerun.db"
        simcmd = ' '.join(simargs)

        with open(yamlfile_path, 'r') as fin:
            yaml_str = fin.read()
            dest_files = re.findall(r'dest_file:\s*(\S+)', yaml_str)

        # Get "all_json_formats" from "/foo/bar/all_json_formats.yaml"
        yaml_basename = os.path.basename(yamlfile_path)
        yaml_basename = os.path.splitext(yaml_basename)[0]

        num_failing, num_passing = self.__RunComparisons(db_file, simcmd, yaml_basename, dest_files, birds_eye)

        os.chdir(calling_dir)
        shutil.rmtree(test_dir)

        return num_failing, num_passing

    def __RunComparisons(self, db_file, simcmd, yaml_basename, dest_files, birds_eye):
        # Create the directories for the PASSING and FAILING results
        passing_dir = os.path.join(self.results_dir, "PASSING", yaml_basename)
        failing_dir = os.path.join(self.results_dir, "FAILING", yaml_basename)

        if not os.path.exists(passing_dir):
            os.makedirs(passing_dir)
        if not os.path.exists(failing_dir):
            os.makedirs(failing_dir)

        # Copy the database file and sim command to both directories
        shutil.copy(db_file, passing_dir)
        shutil.copy(db_file, failing_dir)
        with open(os.path.join(passing_dir, "sim.cmd"), 'w') as f:
            f.write(simcmd)
        with open(os.path.join(failing_dir, "sim.cmd"), 'w') as f:
            f.write(simcmd)

        MakeExecutable(os.path.join(passing_dir, "sim.cmd"))
        MakeExecutable(os.path.join(failing_dir, "sim.cmd"))

        # Copy <refs_dir/*.yaml> into the passing/failing directories
        for ref_file in os.listdir(refs_dir):
            if ref_file.endswith(".yaml"):
                shutil.copy(os.path.join(refs_dir, ref_file), passing_dir)
                shutil.copy(os.path.join(refs_dir, ref_file), failing_dir)

        # Remove the PASSING directory if all tests failed, and vice versa.
        num_failing = 0
        num_passing = 0

        # Export all reports from SimDB
        export_py = os.path.join(script_dir, "simdb_export.py")
        export_args = ['python3', export_py, '--db-file', db_file, '--force']
        export_cmd = ' '.join(export_args)
        print ('Running subprocess: ' + export_cmd)
        subprocess.run(export_args)

        # Compare the exported reports with the baseline reports one at a time
        for dest_file in dest_files:
            baseline_report = dest_file
            simdb_report = os.path.join('simdb_reports', dest_file)
            comparator = GetComparator(dest_file)

            if not comparator.Compare(baseline_report, simdb_report):
                if 'FAILING' not in birds_eye:
                    birds_eye['FAILING'] = {}
                if yaml_basename not in birds_eye['FAILING']:
                    birds_eye['FAILING'][yaml_basename] = []
                birds_eye['FAILING'][yaml_basename].append(dest_file)
                num_failing += 1
                failing_report_dir = os.path.join(failing_dir, os.path.splitext(dest_file)[0])
                if not os.path.exists(failing_report_dir):
                    os.makedirs(failing_report_dir)

                simdb_dest_file_in = os.path.join('simdb_reports', dest_file)
                dest_file_parts = dest_file.split('.')
                simdb_dest_file = dest_file_parts[0] + '.simdb.' + dest_file_parts[1]
                simdb_dest_file_out = os.path.join(failing_report_dir, simdb_dest_file)
                shutil.copy(simdb_dest_file_in, simdb_dest_file_out)

                # Write the export.cmd file, noting that it is intended to be run
                # from the failing report directory (hence the use of relative paths)
                with open(os.path.join(failing_report_dir, "export.cmd"), 'w') as fout:
                    export_cmd = f"python3 {export_py} --db-file ../sparta.db --force --export-dir export_rerun --dest-file {dest_file}"
                    fout.write(export_cmd)

                MakeExecutable(os.path.join(failing_report_dir, "export.cmd"))

                # Copy the baseline report to the failing directory
                shutil.copy(dest_file, failing_report_dir)
            else:
                if 'PASSING' not in birds_eye:
                    birds_eye['PASSING'] = {}
                if yaml_basename not in birds_eye['PASSING']:
                    birds_eye['PASSING'][yaml_basename] = []
                birds_eye['PASSING'][yaml_basename].append(dest_file)
                num_passing += 1

        # If there are zero failing tests, remove the FAILING directory
        if num_failing == 0:
            shutil.rmtree(failing_dir)
        if num_passing == 0:
            shutil.rmtree(passing_dir)

        return num_failing, num_passing

def GetComparator(dest_file):
    extension = os.path.splitext(dest_file)[1]
    if extension in ('.txt', '.text'):
        return TextReportComparator()
    if extension in ('.json'):
        return JSONReportComparator()
    if extension in ('.csv'):
        return CSVReportComparator()

    return UnsupportedComparator()

class Comparator:
    def __init__(self):
        pass

    def NormalizeText(self, text):
        # Remove leading/trailing whitespace and replace all internal whitespace with a single space
        return re.sub(r'\s+', ' ', text.strip())

class TextReportComparator(Comparator):
    def __init__(self):
        Comparator.__init__(self)

    def Compare(self, baseline_report, simdb_report):
        with open(baseline_report, "r") as bf, open(simdb_report, "r") as ef:
            baseline_text = self.NormalizeText(bf.read())
            export_text = self.NormalizeText(ef.read())

        return baseline_text == export_text

class JSONReportComparator(Comparator):
    def __init__(self):
        Comparator.__init__(self)

    def Compare(self, baseline_report, simdb_report):
        with open(baseline_report, "r", encoding="utf-8") as bf, open(simdb_report, "r", encoding="utf-8") as ef:
            try:
                baseline_data = json.load(bf)
            except json.JSONDecodeError as e:
                return False

            try:
                export_data = json.load(ef)
            except json.JSONDecodeError as e:
                return False

        return baseline_data == export_data

class CSVReportComparator(Comparator):
    def __init__(self):
        Comparator.__init__(self)

    def Compare(self, baseline_report, simdb_report):
        with open(baseline_report, "r", encoding="utf-8") as bf, open(simdb_report, "r", encoding="utf-8") as ef:
            baseline_reader = list(csv.reader(bf))
            export_reader = list(csv.reader(ef))

        if len(baseline_reader) != len(export_reader):
            return False

        for row, (baseline_text, export_text) in enumerate(zip(baseline_reader, export_reader)):
            if len(baseline_text) != len(export_text):
                return False

            if row <= 1:
                # Metadata and stat locations headers: soft compare
                norm1 = [self.NormalizeText(cell) for cell in baseline_text]
                norm2 = [self.NormalizeText(cell) for cell in export_text]
                if norm1 != norm2:
                    return False
            else:
                # Data rows: strict float compare
                try:
                    floats1 = [float(cell) for cell in baseline_text]
                    floats2 = [float(cell) for cell in export_text]
                except ValueError as e:
                    return False

                if floats1 != floats2:
                    return False

        return True

class UnsupportedComparator(Comparator):
    def __init__(self):
        Comparator.__init__(self)

    def Compare(self, baseline_report, simdb_report):
        return False

# Run report verification
verifier = ReportVerifier(args.sim_exe_path, args.report_yaml_dir, args.results_dir)
verifier.RunAll()
print(f"\nReport verification completed. Results saved in '{args.results_dir}'.")
