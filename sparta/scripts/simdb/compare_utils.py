import os, json, re

def GetComparator(dest_file):
    extension = os.path.splitext(dest_file)[1]
    if extension in ('.txt', '.text'):
        return TextReportComparator()
    if extension in ('.json'):
        return JSONReportComparator()
    if extension in ('.csv'):
        return CSVReportComparator()
    if extension in ('.html', '.htm'):
        return HTMLReportComparator()

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

class HTMLReportComparator(TextReportComparator):
    pass

def RunComparison(legacy_reports_dir, simdb_reports_dir, baseline_reports, logger):
    def WriteToLogger(logger, msg):
        msg = msg.rstrip() + "\n"
        logger.write(msg)

    # Compare the baseline reports to the SimDB reports.
    passing_reports = []
    failing_reports = []
    unsupported_reports = []

    WriteToLogger(logger, "Comparing baseline reports to SimDB reports.")
    for report in baseline_reports:
        baseline_report = os.path.join(legacy_reports_dir, report)
        simdb_report = os.path.join(simdb_reports_dir, report)

        if not os.path.exists(simdb_report):
            WriteToLogger(logger, f"SimDB report '{report}' not found.")
            failing_reports.append(report)
            continue
        
        # Compare the two reports.
        comparator = GetComparator(report)
        if comparator is None:
            WriteToLogger(logger, f"Report {report} is not supported for comparison.")
            unsupported_reports.append(report)
            continue

        if not comparator.Compare(baseline_report, simdb_report):
            WriteToLogger(logger, f"Baseline report {report} does not match SimDB report. Test will fail.")
            failing_reports.append(report)
        else:
            WriteToLogger(logger, f"Baseline report {report} matches SimDB report.")
            passing_reports.append(report)
        
    num_passed = len(passing_reports)
    total_comparisons = num_passed + len(failing_reports) + len(unsupported_reports)
    WriteToLogger(logger, f"Report comparison complete: {num_passed} of {total_comparisons} reports passed.")

    return {
        "passing": passing_reports,
        "failing": failing_reports,
        "unsupported": unsupported_reports
    }
