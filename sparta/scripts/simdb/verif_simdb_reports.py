import os, sys
import argparse
import subprocess
import shutil
import re
import stat
import csv, json
import yaml

script_dir = os.path.dirname(os.path.abspath(__file__))
sparta_dir = os.path.dirname(os.path.dirname(script_dir))
report_yaml_dir = os.path.join(sparta_dir, "example", "CoreModel")

parser = argparse.ArgumentParser(description="Verify the SimDB report exporters for all report configurations.")
parser.add_argument("--sim-exe-path", type=str, default="release/example/CoreModel/sparta_core_example",
                    help="Path to the Sparta executable.")
parser.add_argument("--report-yaml-dir", type=str, default=report_yaml_dir,
                    help="Directory containing the report description/definition YAML files needed to run all tests.")
parser.add_argument("--group-regex", type=str, help="Regex to filter the test groups to run. Default is all groups.")
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

# Helper class to log to a file and stdout
class TestLog:
    def __init__(self, filename):
        self.log = open(filename, "w")

    def write(self, message):
        self.log.write(message)
        self.log.flush()
        print (message.rstrip())
        sys.stdout.flush()

    def close(self):
        if self.log:
            self.log.close()
            self.log = None

    def __del__(self):
        self.close()

# This class does the heavy lifting of running the sparta executable, exporting the reports
# from SimDB to the results dir, and comparing the results to the baseline reports.
class TestCase:
    def __init__(self, sim_cmd, test_name, test_group, test_reports):
        self.sim_cmd = sim_cmd
        self.test_name = test_name
        self.test_group = test_group
        self.test_reports = test_reports
        self.test_artifacts = []
        self.log = None

    def RunTest(self):
        self.log = TestLog("test.log")
        self.test_artifacts.append("test.log")

        # Run the simulation command
        self.log.write(f"Running test '{self.test_name}' in group '{self.test_group}'...\n")
        self.log.write(f"---Command: {self.sim_cmd}\n\n")
        result = subprocess.run(self.sim_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        stdout = result.stdout
        if stdout:
            self.log.write("Creating sim.stdout...\n")
            with open("sim.stdout", "w") as fout:
                fout.write(stdout)
                self.test_artifacts.append("sim.stdout")

        stderr = result.stderr
        if stderr:
            self.log.write("Creating sim.stderr...\n")
            with open("sim.stderr", "w") as fout:
                fout.write(stderr)
                self.test_artifacts.append("sim.stderr")

        # Create sim.cmd (executable)
        self.log.write("Creating sim.cmd...\n")
        with open("sim.cmd", "w") as fout:
            fout.write(self.sim_cmd)
        MakeExecutable("sim.cmd")
        self.test_artifacts.append("sim.cmd")

        # Create sim.rerun.cmd (executable)
        self.log.write("Creating sim.rerun.cmd...\n")
        with open("sim.rerun.cmd", "w") as fout:
            fout.write(self.sim_cmd + " --simdb-file rerun.db")
        MakeExecutable("sim.rerun.cmd")
        self.test_artifacts.append("sim.rerun.cmd")

        # Verify that the baseline reports all exist, else this test is
        # considered a failure.
        failed = False
        for report in self.test_reports:
            if not os.path.exists(report):
                self.log.write(f"Error: Baseline report '{report}' does not exist.\n")
                failed = True

        if os.path.exists("sparta.db"):
            self.test_artifacts.append("sparta.db")
        else:
            # Cannot continue the test!
            self.log.write("Error: sparta.db does not exist.\n")
            failed = True

        if failed:
            self.log.write("Test cannot continue. See above errors.\n")
            self.log.close()
            return False

        # Export all reports from SimDB into the 'simdb_reports' directory.
        export_cmd = "python3 " + os.path.join(script_dir, "simdb_export.py")
        export_cmd += " --db-file sparta.db"
        export_cmd += " --export-dir simdb_reports"
        export_cmd += " --force"
        self.log.write(f"Running export command: {export_cmd}\n")
        result = subprocess.run(export_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        stdout = result.stdout
        if stdout:
            self.log.write("Creating export.stdout...\n")
            with open("export.stdout", "w") as fout:
                fout.write(stdout)
                self.test_artifacts.append("export.stdout")

        stderr = result.stderr
        if stderr:
            self.log.write("Creating export.stderr...\n")
            with open("export.stderr", "w") as fout:
                fout.write(stderr)
                self.test_artifacts.append("export.stderr")

        # Create export.cmd (executable)
        self.log.write("Creating export.cmd...\n")
        with open("export.cmd", "w") as fout:
            fout.write(export_cmd)
        MakeExecutable("export.cmd")
        self.test_artifacts.append("export.cmd")

        # For each baseline report e.g. "basic.csv", check if there is an equivalent
        # report in the "simdb_reports" directory. If not, this test is considered a
        # failure. If so, run the comparison and move the baseline report into the
        # 'baseline_reports' directory.
        os.makedirs("baseline_reports")

        failed_comparison_reports = []
        passed_comparison_reports = []
        for report in self.test_reports:
            shutil.copy(report, "baseline_reports")
            self.test_artifacts.append(os.path.join("baseline_reports", os.path.basename(report)))
            simdb_report = os.path.join("simdb_reports", os.path.basename(report))
            if not os.path.exists(simdb_report):
                self.log.write(f"Error: SimDB report '{simdb_report}' does not exist.\n")
                failed = True
            else:
                self.log.write(f"Comparing report: {report}...\n")
                self.test_artifacts.append(simdb_report)
                comparator = GetComparator(report)
                if not comparator.Compare(report, simdb_report):
                    failed_comparison_reports.append(report)
                    self.log.write(f"Error: Report comparison failed for '{report}'.\n")
                    failed = True
                else:
                    passed_comparison_reports.append(report)
                    self.log.write(f"Report comparison passed for '{report}'.\n")

        # Create "passfail.txt" with the list of failed and passed reports.
        with open("passfail.txt", "w") as fout:
            if failed_comparison_reports:
                fout.write("Failed reports:\n")
                for report in failed_comparison_reports:
                    fout.write('    ' + report + "\n")

            if passed_comparison_reports:
                fout.write("Passed reports:\n")
                for report in passed_comparison_reports:
                    fout.write('    ' + report + "\n")

        self.test_artifacts.append("passfail.txt")

        if failed:
            self.log.write("Test FAILED. See above errors.\n")
        else:
            self.log.write("Test PASSED.\n")

        self.log.close()
        return not failed

# This class runs all the tests defined in test.yaml and prints high-level results to stdout.
class TestSuite:
    def __init__(self, sparta_exe_path, report_yaml_dir, results_dir):
        self.sparta_exe_path = os.path.abspath(sparta_exe_path)
        self.report_yaml_dir = os.path.abspath(report_yaml_dir)
        self.results_dir = os.path.abspath(results_dir)

    def RunAll(self):
        test_cases = []
        with open(os.path.join(script_dir, "tests.yaml"), 'r') as fin:
            test_configs = yaml.safe_load(fin)
            for config in test_configs:
                test_name = config['name']
                test_group = config['group']
                test_args = config['args']
                test_reports = config['verif']

                if args.group_regex and not re.search(args.group_regex, test_group):
                    print(f"Skipping test '{test_name}' in group '{test_group}' due to group regex filter.")
                    continue

                sim_cmd = self.sparta_exe_path + ' -i 20k ' + test_args
                sim_cmd += ' --enable-simdb-reports'
                sim_cmd += ' --report-search-dir ' + self.report_yaml_dir

                test_case = TestCase(sim_cmd, test_name, test_group, test_reports)
                test_cases.append(test_case)

        num_passing_by_group = {}
        num_tests_by_group = {}

        for test_case in test_cases:
            group = test_case.test_group.split('/')[0]
            if group not in num_tests_by_group:
                num_tests_by_group[group] = 0
            num_tests_by_group[group] += 1
            num_passing_by_group[group] = 0

        test_results_summary = {}
        for test_case in test_cases:
            running_dir = os.path.join(self.results_dir, 'RUNNING')
            if os.path.exists(running_dir):
                shutil.rmtree(running_dir)
            os.makedirs(running_dir)

            # Copy all .yaml files to the running directory (except tests.yaml)
            for yaml_file in os.listdir(script_dir):
                if yaml_file.endswith('.yaml') and yaml_file != 'tests.yaml':
                    src = os.path.join(script_dir, yaml_file)
                    dst = os.path.join(running_dir, yaml_file)
                    shutil.copy(src, dst)

            calling_dir = os.getcwd()
            os.chdir(running_dir)
            success = test_case.RunTest()
            subdir = 'PASSING' if success else 'FAILING'
            if success:
                group = test_case.test_group.split('/')[0]
                num_passing_by_group[group] = num_passing_by_group[group] + 1

            # Copy all artifacts to the subdir/group/test_name directory.
            test_results_dir = os.path.join(self.results_dir, subdir, test_case.test_group, test_case.test_name)
            if not os.path.exists(test_results_dir):
                os.makedirs(test_results_dir)

            if subdir not in test_results_summary:
                test_results_summary[subdir] = {}
            if test_case.test_group not in test_results_summary[subdir]:
                test_results_summary[subdir][test_case.test_group] = []
            test_results_summary[subdir][test_case.test_group].append(test_case.test_name)

            for artifact in test_case.test_artifacts:
                # The artifacts can be something like "basic.csv" or "simdb_reports/basic.csv" so
                # we need to create the full path to the artifact if needed.
                artifact_dir = os.path.join(test_results_dir, os.path.dirname(artifact))
                os.makedirs(artifact_dir, exist_ok=True)
                shutil.copy(artifact, artifact_dir)

            os.chdir(calling_dir)
            shutil.rmtree(running_dir)

        # Print the summary of test results.
        print ("")
        print ("Test results summary:")
        for key in ['PASSING', 'FAILING']:
            if key in test_results_summary:
                print (key + ":")
                for group, tests in test_results_summary[key].items():
                    print ("    " + group)
                    for test in tests:
                        print ("        " + test)

        print ("")
        print ("Pass rate by group:")
        max_group_name_len = max(len(group) for group in num_tests_by_group.keys())
        for group, num_tests in num_tests_by_group.items():
            group_num_passing = num_passing_by_group[group]
            group = group.ljust(max_group_name_len)
            print (f"  {group} -- {group_num_passing}/{num_tests} tests passed.")

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
verifier = TestSuite(args.sim_exe_path, args.report_yaml_dir, args.results_dir)
verifier.RunAll()
print(f"\nReport verification completed. Results saved in '{args.results_dir}'.")
