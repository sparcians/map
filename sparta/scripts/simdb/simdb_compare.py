import os, sys
import argparse
import re, csv, json

parser = argparse.ArgumentParser(description="Compare exported SimDB records to formatted report files.")
parser.add_argument("--baseline-dir", type=str, required=True, help="Path to the baseline directory containing the formatted report files.")
parser.add_argument("--export-dir", type=str, required=True, help="Path to the exported SimDB record files.")
parser.add_argument("--results-dir", type=str, default=os.getcwd(), help="Directory to save the comparison results.")
parser.add_argument("--force", action="store_true", help="Force overwrite results directory (if provided).")
args = parser.parse_args()

if not os.path.exists(args.baseline_dir):
    print(f"Error: The specified baseline directory '{args.baseline_dir}' does not exist.")
    sys.exit(1)

if not os.path.exists(args.export_dir):
    print(f"Error: The specified export directory '{args.export_dir}' does not exist.")
    sys.exit(1)

pass_file = os.path.join(args.results_dir, "PASSED")
if os.path.exists(pass_file):
    if args.force:
        os.remove(pass_file)
    else:
        print(f"Error: The results directory '{args.results_dir}' already contains a PASSED file. Use --force to overwrite.")
        sys.exit(1)

fail_file = os.path.join(args.results_dir, "FAILED")
if os.path.exists(fail_file):
    if args.force:
        os.remove(fail_file)
    else:
        print(f"Error: The results directory '{args.results_dir}' already contains a FAILED file. Use --force to overwrite.")
        sys.exit(1)

class ReportComparator:
    def __init__(self, baseline_dir, export_dir):
        self.baseline_dir = baseline_dir
        self.export_dir = export_dir
        self.passing_reports = []
        self.failing_reports = []
        self.fail_reasons = {}

    def Compare(self, report_filename):
        baseline_file = os.path.join(self.baseline_dir, report_filename)
        export_file = os.path.join(self.export_dir, report_filename)

        if not os.path.exists(baseline_file):
            reason = f"Baseline file '{baseline_file}' does not exist."
            self.failing_reports.append(report_filename)
            self.fail_reasons[report_filename] = reason
            return False
        
        if not os.path.exists(export_file):
            reason = f"Export file '{export_file}' does not exist."
            self.failing_reports.append(report_filename)
            self.fail_reasons[report_filename] = reason
            return False

        extension = os.path.splitext(report_filename)[1]
        if extension == ".txt":
            return self.__CompareTextFiles(baseline_file, export_file)
        elif extension == ".csv":
            return self.__CompareCSVFiles(baseline_file, export_file)
        elif extension == ".json":
            return self.__CompareJSONFiles(baseline_file, export_file)
        else:
            reason = f"Unsupported file type '{extension}' for report '{report_filename}'."
            self.failing_reports.append(report_filename)
            self.fail_reasons[report_filename] = reason
            return False

    def GeneratePASSFAIL(self, results_dir):
        assert os.path.isdir(results_dir), "Results directory does not exist."

        # Create <results_dir>/PASSED file if all report comparisons passed
        if not self.failing_reports:
            with open(os.path.join(results_dir, "PASSED"), "w") as fout:
                # Just list all the passing report filenames
                for report in self.passing_reports:
                    fout.write(f"{report}\n")

            print (f"Report comparison succeeded. Results saved in '{results_dir}/PASSED'.")
            return

        # Create <results_dir>/FAILED file if any report comparison failed
        with open(os.path.join(results_dir, "FAILED"), "w") as fout:
            failed_reports_maxlen = max(len(report) for report in self.failing_reports)
            for report in self.failing_reports:
                reason = self.fail_reasons[report]
                fout.write(f"{report.ljust(failed_reports_maxlen)}: {reason}\n")

                baseline_abspath = os.path.abspath(os.path.join(self.baseline_dir, report))
                export_abspath = os.path.abspath(os.path.join(self.export_dir, report))
                vimdiff_cmd = f"vimdiff {baseline_abspath} {export_abspath}"
                fout.write(f"\t{vimdiff_cmd}\n\n")

            print (f"Report comparison failed. Results saved in '{results_dir}/FAILED'.")
            return

    def __CompareTextFiles(self, baseline_file, export_file):
        with open(baseline_file, "r") as bf, open(export_file, "r") as ef:
            baseline_text = self.__NormalizeText(bf.read())
            export_text = self.__NormalizeText(ef.read())

        if baseline_text == export_text:
            self.passing_reports.append(os.path.basename(baseline_file))
            return True
        else:
            reason = f"Text content mismatch between '{baseline_file}' and '{export_file}'."
            self.failing_reports.append(os.path.basename(baseline_file))
            self.fail_reasons[os.path.basename(baseline_file)] = reason
            return False

    def __CompareCSVFiles(self, baseline_file, export_file):
        with open(baseline_file, "r", encoding="utf-8") as bf, open(export_file, "r", encoding="utf-8") as ef:
            baseline_reader = list(csv.reader(bf))
            export_reader = list(csv.reader(ef))

        if len(baseline_reader) != len(export_reader):
            reason = f"Row count mismatch between '{baseline_file}' and '{export_file}'."
            self.failing_reports.append(os.path.basename(baseline_file))
            self.fail_reasons[os.path.basename(baseline_file)] = reason
            return False

        for row, (baseline_text, export_text) in enumerate(zip(baseline_reader, export_reader)):
            if len(baseline_text) != len(export_text):
                reason = f"Column count mismatch in row {row+1} between '{baseline_file}' and '{export_file}'."
                self.failing_reports.append(os.path.basename(baseline_file))
                self.fail_reasons[os.path.basename(baseline_file)] = reason
                return False

            if row == 0:
                # Header row: soft compare for column headers
                norm1 = [self.__NormalizeText(cell) for cell in baseline_text]
                norm2 = [self.__NormalizeText(cell) for cell in export_text]
                if norm1 != norm2:
                    reason = f"Header mismatch in row {row+1} between '{baseline_file}' and '{export_file}'."
                    self.failing_reports.append(os.path.basename(baseline_file))
                    self.fail_reasons[os.path.basename(baseline_file)] = reason
                    return False
            else:
                # Data rows: strict float compare
                try:
                    floats1 = [float(cell) for cell in baseline_text]
                    floats2 = [float(cell) for cell in export_text]
                except ValueError as e:
                    reason = f"Data conversion error in row {row+1} between '{baseline_file}' and '{export_file}': {e}"
                    self.failing_reports.append(os.path.basename(baseline_file))
                    self.fail_reasons[os.path.basename(baseline_file)] = reason
                    return False

                if floats1 != floats2:
                    reason = f"Data mismatch in row {row+1} between '{baseline_file}' and '{export_file}'."
                    self.failing_reports.append(os.path.basename(baseline_file))
                    self.fail_reasons[os.path.basename(baseline_file)] = reason
                    return False

        self.passing_reports.append(os.path.basename(baseline_file))
        return True

    def __CompareJSONFiles(self, baseline_file, export_file):
        with open(baseline_file, "r", encoding="utf-8") as bf, open(export_file, "r", encoding="utf-8") as ef:
            try:
                baseline_data = json.load(bf)
            except json.JSONDecodeError as e:
                reason = f"JSON decode error in '{baseline_file}': {e}"
                self.failing_reports.append(os.path.basename(baseline_file))
                self.fail_reasons[os.path.basename(baseline_file)] = reason
                return False

            try:
                export_data = json.load(ef)
            except json.JSONDecodeError as e:
                reason = f"JSON decode error in '{export_file}': {e}"
                self.failing_reports.append(os.path.basename(baseline_file))
                self.fail_reasons[os.path.basename(baseline_file)] = reason
                return False

        if baseline_data == export_data:
            self.passing_reports.append(os.path.basename(baseline_file))
            return True
        else:
            reason = f"JSON content mismatch between '{baseline_file}' and '{export_file}'."
            self.failing_reports.append(os.path.basename(baseline_file))
            self.fail_reasons[os.path.basename(baseline_file)] = reason
            return False

    def __NormalizeText(self, text):
        # Remove leading/trailing whitespace and replace all internal whitespace with a single space
        return re.sub(r'\s+', ' ', text.strip())

# Run comparison for all files in the baseline directory
comparator = ReportComparator(args.baseline_dir, args.export_dir)
for report_filename in os.listdir(args.baseline_dir):
    if not report_filename.startswith(".") and os.path.isfile(os.path.join(args.baseline_dir, report_filename)):
        comparator.Compare(report_filename)

# Generate PASS/FAIL report
comparator.GeneratePASSFAIL(args.results_dir)
