import os
import re
import copy
import argparse
import subprocess
import shutil
import multiprocessing
import tempfile
import glob
import json
import sys
import io

script_dir = os.path.dirname(os.path.abspath(__file__))
sparta_dir = os.path.dirname(os.path.dirname(script_dir))
core_model_dir = os.path.join(sparta_dir, "example", "CoreModel")
cmake_list_file = os.path.join(core_model_dir, "CMakeLists.txt")

parser = argparse.ArgumentParser(description="Verify the SimDB report exporters for all report configurations.")
parser.add_argument("--sim-exe-path", type=str, default="release/example/CoreModel/sparta_core_example",
                    help="Path to the Sparta executable.")
parser.add_argument("--results-dir", type=str, default="simdb_verif_results",
                    help="Directory to store the pass/fail results and all baseline/simdb reports.")
parser.add_argument("--force", action="store_true", help="Force overwrite of the results directory if it exists.")
parser.add_argument("--serial", action="store_true", help="Run tests serially instead of in parallel.")
parser.add_argument("--skip", nargs='+', default=[], help="Skip the specified tests.")
parser.add_argument("--test-only", nargs='+', default=[], help="Run only the specified test(s).")
args = parser.parse_args()

# Overwrite the results directory since each test runs in its own tempdir.
args.results_dir = os.path.abspath(args.results_dir)

if os.path.exists(args.results_dir):
    if args.force:
        shutil.rmtree(args.results_dir)
    else:
        print(f"ERROR: The results directory {args.results_dir} already exists. Use --force to overwrite.")
        exit(1)

def SymlinkTree(src, dst):
    os.makedirs(dst, exist_ok=True)
    for root, dirs, files in os.walk(src):
        rel_path = os.path.relpath(root, src)
        dst_root = os.path.join(dst, rel_path)

        for d in dirs:
            os.makedirs(os.path.join(dst_root, d), exist_ok=True)
        for f in files:
            src = os.path.join(root, f)
            dst = os.path.join(dst_root, f)
            os.symlink(src, dst)

class SpartaTest:
    @classmethod
    def CreateFromCMake(cls, cmake_text, cmake_dir, copy_files, copy_dirs, unsupported_reports_queue):
        # Only create tests that include "--report" in the cmake_text,
        # and that start with "sparta_named_test".
        if not cmake_text.startswith("sparta_named_test("):
            return None

        # We only support the use of --report like this:
        #    --report descriptor.yaml
        # Not this:
        #    --report top defn.yaml out.json
        
        # Use a regex to extract the test name and the sim command.
        #   e.g. "sparta_named_test(json_reports sparta_core_example -i 10k --report all_json_formats.yaml)"
        #   becomes:
        #   test_name = "json_reports"
        #   sim_cmd = "sparta_core_example -i 10k --report all_json_formats.yaml"
        match = re.match(r'sparta_named_test\(([^ ]+) ([^ ]+.*)\)', cmake_text)
        if match:
            test_name = match.group(1)
            sim_cmd = match.group(2)
        else:
            return None

        if test_name in args.skip:
            print(f"Skipping test {test_name}")
            return None

        if args.test_only and test_name not in args.test_only:
            print(f"Skipping test {test_name} (not in --test-only list)")
            return None

        top_level_yamls = []
        for i, arg in enumerate(sim_cmd.split()):
            if arg == "--report" and i + 1 < len(sim_cmd.split()):
                # The next argument is the report descriptor file.
                report_descriptor = sim_cmd.split()[i + 1]
                if not report_descriptor.endswith(".yaml"):
                    return None
                top_level_yamls.append(report_descriptor)
            elif arg == "--report":
                return None

        if not top_level_yamls:
            return None

        return cls(test_name, sim_cmd, cmake_dir, copy_files, copy_dirs, unsupported_reports_queue)

    def __init__(self, test_name, sim_cmd, cmake_dir, copy_files, copy_dirs, unsupported_reports_queue):
        self.test_name = test_name
        self.sim_cmd = sim_cmd
        self.cmake_dir = cmake_dir
        self.copy_files = copy.deepcopy(copy_files)
        self.copy_dirs = copy.deepcopy(copy_dirs)
        self.unsupported_reports_queue = unsupported_reports_queue
        self.logout = None

        # Each test runs in its own temporary directory.
        self.test_dir = tempfile.mkdtemp()

    def RunTest(self):
        sim_exe = args.sim_exe_path
        self.copy_files.append(os.path.abspath(sim_exe))
        self.calling_dir = os.getcwd()
        self.logout = io.StringIO()

        running_test_dir = os.path.join(args.results_dir, "RUNNING", self.test_name)
        os.makedirs(running_test_dir, exist_ok=True)

        # RAII to go into/out of the test directory.
        class ScopedTestDir:
            def __init__(self, test_dir):
                self.calling_dir = os.getcwd()
                os.chdir(test_dir)

            def __del__(self):
                os.chdir(self.calling_dir)

        scoped_test_dir = ScopedTestDir(self.test_dir)

        # Copy the files and directories to the test directory.
        for file in self.copy_files:
            src = os.path.join(self.cmake_dir, file)
            if os.path.exists(src):
                srcs = [src]
            else:
                srcs = glob.glob(os.path.join(self.cmake_dir, file))

            for src in srcs:
                dst = os.path.join(self.test_dir, os.path.basename(src))
                os.symlink(src, dst)

        for dir in self.copy_dirs:
            src = os.path.join(self.cmake_dir, dir)
            if os.path.exists(src):
                srcs = [src]
            else:
                srcs = glob.glob(os.path.join(self.cmake_dir, dir))

            for src in srcs:
                dst = os.path.join(self.test_dir, os.path.basename(src))
                SymlinkTree(src, dst)

        # Ensure '--simdb-file sparta.db' is present, or add it, or replace it.
        sim_args = self.sim_cmd.split()
        if "--simdb-file" in sim_args:
            simdb_index = sim_args.index("--simdb-file")
            assert simdb_index + 1 < len(sim_args)
            sim_args[simdb_index + 1] = "sparta.db"
        else:
            sim_args.append("--simdb-file")
            sim_args.append("sparta.db")

        # Ensure '--enable-simdb-reports' is present.
        if "--enable-simdb-reports" not in sim_args:
            sim_args.append("--enable-simdb-reports")

        # Run the test sim command.
        sim_cmd = ' '.join(sim_args)
        sim_cmd = './' + sim_cmd.lstrip('./')

        self.__WriteToTestLog(f"Running sim command: {sim_cmd}")
        subprocess.run(sim_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # Note that the RUNNING directory will be deleted if the test
        # continues all the way to report comparison.
        if not os.path.exists("sparta.db"):
            self.__WriteToTestLog("sparta.db not found. Test cannot continue.")
            return

        # Get the expected Sparta-generated reports from "recursively" parsing the
        # YAML files, starting with the sim command, looking for all reports of the
        # form "dest_file: <file>".
        yaml_queue = []
        for i, arg in enumerate(sim_args):
            if arg == "--report" and i + 1 < len(sim_args):
                # The next argument is the report descriptor file.
                report_descriptor = sim_args[i + 1]
                if not report_descriptor.endswith(".yaml"):
                    continue
                yaml_queue.append(report_descriptor)
            elif arg == "--report":
                continue

        # Helper to find a <file>.yaml file in the current directory
        # or any subdirectories.
        def FindYamlFile(filename):
            # Check if the file exists in the current directory.
            if os.path.exists(filename):
                return filename

            # Check if the file exists in any subdirectory.
            for root, dirs, files in os.walk("."):
                if filename in files:
                    return os.path.join(root, filename)

            return None

        visited_yamls = set()
        while yaml_queue:
            yaml_file = yaml_queue.pop(0)
            if yaml_file in visited_yamls:
                continue

            visited_yamls.add(yaml_file)
            with open(FindYamlFile(yaml_file), 'r') as fin:
                # Use regex to look for any *.yaml reference, and add it to the queue.
                for line in fin.readlines():
                    matches = re.findall(r'\s([\w.-]+\.yaml)\s', f' {line} ')
                    for match in matches:
                        if match not in yaml_queue:
                            yaml_queue.append(match)

        # Go through all the YAML files we encountered and find all of
        # the "dest_file: <file.format>" strings. Extract all of the
        # <file.format> filenames and add them to the baseline reports.
        baseline_reports = []
        for yaml_file in visited_yamls:
            with open(FindYamlFile(yaml_file), 'r') as fin:
                for line in fin.readlines():
                    match = re.search(r'dest_file:\s*([\w.-]+)', line)
                    if match:
                        dest_file = match.group(1)

                        # Note that we cannot test a dest_file of "1" because that
                        # means "dump to stdout" and we cannot compare that to a file
                        # so easily.
                        if dest_file not in baseline_reports and dest_file != "1":
                            baseline_reports.append(dest_file)

        # We cannot continue the test if we cannot find the baseline reports.
        if not baseline_reports:
            self.__WriteToTestLog("No baseline reports found.")
            return

        failed = False
        for report in baseline_reports:
            if not os.path.exists(report):
                self.__WriteToTestLog(f"Baseline report {report} not found.")
                failed = True

        if failed:
            self.__WriteToTestLog(f"Baseline report(s) not found.")
            return

        # Create the 'baseline_reports' directory and move the baseline reports there.
        os.makedirs("baseline_reports")
        for report in baseline_reports:
            src = os.path.join(self.test_dir, report)
            dst = os.path.join("baseline_reports", os.path.basename(report))
            shutil.move(src, dst)

        # Export all reports from SimDB into the 'simdb_reports' directory.
        export_cmd = "python3 " + os.path.join(script_dir, "simdb_export.py")
        export_cmd += " --db-file sparta.db"
        export_cmd += " --export-dir simdb_reports"
        export_cmd += " --force"

        self.__WriteToTestLog(f"Exporting SimDB reports with command: {export_cmd}")
        subprocess.run(export_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # Compare the baseline reports to the SimDB reports.
        passing_reports = []
        failing_reports = []
        unsupported_reports = []

        self.__WriteToTestLog("Comparing baseline reports to SimDB reports.")
        for report in baseline_reports:
            baseline_report = os.path.join("baseline_reports", report)
            simdb_report = os.path.join("simdb_reports", report)

            if not os.path.exists(simdb_report):
                self.__WriteToTestLog(f"SimDB report {report} not found.")
                failing_reports.append(report)
                continue

            # Compare the two reports.
            comparator = GetComparator(report)
            if comparator is None:
                self.__WriteToTestLog(f"Report {report} is not supported for comparison.")
                unsupported_reports.append(report)
                continue

            if not comparator.Compare(baseline_report, simdb_report):
                self.__WriteToTestLog(f"Baseline report {report} does not match SimDB report. Test will fail.")
                failing_reports.append(report)
            else:
                self.__WriteToTestLog(f"Baseline report {report} matches SimDB report.")
                passing_reports.append(report)

        num_passed = len(passing_reports)
        total_comparisons = num_passed + len(failing_reports)
        msg = f"Report comparison complete: {num_passed} of {total_comparisons} reports passed."
        self.__WriteToTestLog(msg)

        if unsupported_reports:
            msg = "Unsupported reports:\n"
            for report in unsupported_reports:
                msg += f"  {report}\n"

            self.__WriteToTestLog(msg)
            for report in unsupported_reports:
                self.unsupported_reports_queue.put(report)

        results = {
            "PASS": passing_reports,
            "FAIL": failing_reports
        }
        for passfail, reports in results.items():
            for report in reports:
                baseline_report = os.path.join("baseline_reports", report)
                simdb_report = os.path.join("simdb_reports", report)
                extension = os.path.splitext(report)[1].lstrip('.')

                # Refine "json" to either: "json", "json_reduced", or "json_detail"
                if extension == "json":
                    with open(baseline_report, "r", encoding="utf-8") as bf:
                        try:
                            baseline_data = json.load(bf)
                            if "report_metadata" in baseline_data:
                                report_metadata = baseline_data["report_metadata"]
                                if "report_format" in report_metadata:
                                    extension = report_metadata["report_format"]
                        except json.JSONDecodeError as e:
                            self.__WriteToTestLog(f"Failed to load JSON report {report}: {e}")
                            continue

                # Copy the passing reports to the results directory, along with the test.log file
                report_dir = os.path.join(args.results_dir, passfail, extension, self.test_name, report)
                os.makedirs(report_dir, exist_ok=True)
                shutil.copy(baseline_report, os.path.join(report_dir, "baseline." + extension))
                shutil.copy(simdb_report, os.path.join(report_dir, "simdb." + extension))
                self.__CopyTestLog(report_dir)

                # Move sparta.db to the report directory (if failed)
                if passfail == "FAIL":
                    shutil.copy("sparta.db", os.path.join(report_dir, "sparta.db"))

        # Delete this test's RUNNING directory.
        running_test_dir = os.path.join(args.results_dir, "RUNNING", self.test_name)
        if os.path.exists(running_test_dir):
            shutil.rmtree(running_test_dir)

    def __WriteToTestLog(self, text):
        self.logout.write(text.rstrip() + "\n")

    def __CopyTestLog(self, report_dir):
        with open(os.path.join(report_dir, "test.log"), "w") as fout:
            fout.write(self.logout.getvalue())

def GetComparator(dest_file):
    extension = os.path.splitext(dest_file)[1]
    if extension in ('.txt', '.text'):
        return TextReportComparator()
    if extension in ('.json'):
        return JSONReportComparator()
    if extension in ('.csv'):
        return CSVReportComparator()

    return None

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
        with open(baseline_report, "r") as bf, open(simdb_report, "r") as ef:
            bf_lines = bf.readlines()
            ef_lines = ef.readlines()

            if len(bf_lines) != len(ef_lines):
                return False

            for i in range(len(bf_lines)):
                bf_line = self.NormalizeText(bf_lines[i])
                ef_line = self.NormalizeText(ef_lines[i])

                if bf_line != ef_line:
                    return False

        return True

# Parse all sparta_copy() files and all sparta_recursive_copy() dirs for each test.
copy_files = []
copy_dirs = []
with open(cmake_list_file, 'r') as f:
    cmake_dir = os.path.dirname(cmake_list_file)
    cmake_text = f.read()
    for line in cmake_text.splitlines():
        if line.startswith("sparta_copy("):
            # This is of the form:
            # sparta_copy(sparta_core_example <file>)
            match = re.match(r'sparta_copy\(sparta_core_example ([^ ]+.*)\)', line)
            if match:
                copy_files.append((match.group(1)))
        elif line.startswith("sparta_recursive_copy("):
            # This is of the form:
            # sparta_recursive_copy(sparta_core_example <dir>)
            match = re.match(r'sparta_recursive_copy\(sparta_core_example ([^ ]+.*)\)', line)
            if match:
                copy_dirs.append((match.group(1)))

# Go through the CMakeLists.txt file and find all the Sparta tests.
sparta_tests = []
unsupported_reports_queue = multiprocessing.Queue()
with open(cmake_list_file, 'r') as f:
    cmake_dir = os.path.dirname(cmake_list_file)
    cmake_text = f.read()
    for line in cmake_text.splitlines():
        test = SpartaTest.CreateFromCMake(line, cmake_dir, copy_files, copy_dirs, unsupported_reports_queue)
        if test:
            sparta_tests.append(test)

# Run one test.
def RunTest(test):
    test.RunTest()

if not args.serial:
    # Run all the tests in parallel.
    processes = []
    test_procs = {}
    for test in sparta_tests:
        process = multiprocessing.Process(target=RunTest, args=(test,))
        processes.append(process)
        test_procs[test.test_name] = process

    print (f"Running {len(processes)} tests in parallel...")
    for process in processes:
        process.start()

    while True:
        num_finished = 0
        for process in processes:
            if not process.is_alive():
                num_finished += 1

        if num_finished == len(processes):
            break

        print(f"--- Progress: {num_finished} of {len(processes)} tests complete.", end="\r")
        sys.stdout.flush()

    # Wait for all processes to finish.
    for process in processes:
        process.join()

    abort_monitoring = True
else:
    # Run all the tests serially.
    print (f"Running {len(sparta_tests)} tests serially...")
    for i, test in enumerate(sparta_tests):
        print(f"--- Test {i+1} of {len(sparta_tests)}:\t{test.test_name}", end="\r")
        try:
            test.RunTest()
        except Exception as e:
            pass
        sys.stdout.flush()

# Delete the <results_dir>/RUNNING directory if there are no subdirs.
running_dir = os.path.join(args.results_dir, "RUNNING")
if os.path.exists(running_dir):
    if not os.listdir(running_dir):
        shutil.rmtree(running_dir)

# Print final results.
os.chdir(args.results_dir)

incomplete_tests = []
if os.path.exists("RUNNING"):
    for d in os.listdir("RUNNING"):
        if os.path.isdir(os.path.join("RUNNING", d)):
            incomplete_tests.append(d)

def FindNumTestsByFormat(passfail):
    num_tests_by_format = {}
    if os.path.exists(passfail):
        os.chdir(passfail)
        # Each subdir here is a format
        for format in os.listdir("."):
            if os.path.isdir(format):
                # The total number of tests in this format is equal to the number of
                # subdirs with the "test.log" file in them.
                num_tests = 0
                for root, dirs, files in os.walk(format):
                    # Check if the test.log file exists in this directory.
                    for f in files:
                        if os.path.basename(f) == "test.log":
                            num_tests += 1

                num_tests_by_format[format] = num_tests

        os.chdir("..")
    return num_tests_by_format

print("")
print("Final results:")
print("")

num_passing_by_format = FindNumTestsByFormat("PASS")
num_failing_by_format = FindNumTestsByFormat("FAIL")

num_unsupported_by_format = {}
while not unsupported_reports_queue.empty():
    report = unsupported_reports_queue.get()
    extension = os.path.splitext(report)[1].lstrip('.')
    if extension not in num_unsupported_by_format:
        num_unsupported_by_format[extension] = 0
    num_unsupported_by_format[extension] += 1

formats = set(num_passing_by_format.keys()).union(set(num_failing_by_format.keys()))
formats = formats.union(set(num_unsupported_by_format.keys()))
formats = list(formats)
formats.sort()

# Format   Passed   Failed   NoCompare
# -----------------------------------------
# csv      10       2        0
# json     5        0        0
# html     0        0        6
# ...
max_format_len = max([len(format) for format in formats])
max_format_len = max(max_format_len, len("Format"))
print(f"{'Format':<{max_format_len}} {'Passed':<8} {'Failed':<8} {'NoCompare':<8}")
print("-----------------------------------------")

for format in formats:
    num_passing = num_passing_by_format.get(format, 0)
    num_failing = num_failing_by_format.get(format, 0)
    num_unsupported = num_unsupported_by_format.get(format, 0)
    print(f"{format:<{max_format_len}} {num_passing:<8} {num_failing:<8} {num_unsupported:<8}")

print("")
if incomplete_tests:
    print("The following tests are incomplete:")
    for test in incomplete_tests:
        print(f"  {test}")
