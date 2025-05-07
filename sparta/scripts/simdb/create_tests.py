import os
import yaml 
import argparse
import re

script_dir = os.path.dirname(os.path.abspath(__file__))
sparta_dir = os.path.dirname(os.path.dirname(script_dir))
core_model_dir = os.path.join(sparta_dir, "example", "CoreModel")
default_cmake_list_file = os.path.join(core_model_dir, "CMakeLists.txt")

parser = argparse.ArgumentParser(description='Create test definition YAML file from CMakeLists.txt for verif_simdb_reports')
parser.add_argument('--cmake-list-file', type=str, default=default_cmake_list_file,
                    help='Path to the CMakeLists.txt file to parse for test definitions')
parser.add_argument('--outfile', type=str, default='tests.yaml', help='Output YAML file name')
args = parser.parse_args()

class SpartaTest:
    @classmethod
    def CreateFromCMake(cls, cmake_text, cmake_dir):
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

        return cls(test_name, sim_cmd, cmake_dir, top_level_yamls)

    def __init__(self, test_name, sim_cmd, cmake_dir, top_level_yamls):
        self.test_name = test_name
        self.sim_cmd = sim_cmd
        self.cmake_dir = cmake_dir
        self.top_level_yamls = top_level_yamls

    def ExtractVerifTests(self):
        test_yamls = []
        for top_level_yaml in self.top_level_yamls:
            yaml_file = os.path.join(self.cmake_dir, top_level_yaml)
            # Ideally we could use yaml.load() here, but the format
            # we use in CMakeLists.txt is not exactly valid YAML.
            # The form is not really a list like it should be:
            #
            #   content:
            #     report:
            #       pattern:   <pattern>
            #       def_file:  <def_file>
            #       dest_file: <dest_file>
            #       format:    <format>
            #     report:
            #       pattern:   <pattern>
            #       def_file:  <def_file>
            #       dest_file: <dest_file>
            #       format:    <format>
            #     ...
            #
            # So we will parse the file ourselves.
            descriptors = []
            with open(yaml_file, 'r') as fin:
                for line in fin.readlines():
                    line = line.strip()
                    if line in ('report:', '- report:'):
                        descriptors.append(ReportDescriptor())
                    elif line.startswith('pattern:'):
                        pattern = line.split(':')[1].strip()
                        descriptors[-1].pattern = pattern
                    elif line.startswith('def_file:'):
                        def_file = line.split(':')[1].strip()
                        descriptors[-1].def_file = def_file
                    elif line.startswith('dest_file:'):
                        dest_file = line.split(':')[1].strip()
                        descriptors[-1].dest_file = dest_file
                    elif line.startswith('format:'):
                        format = line.split(':')[1].strip()
                        descriptors[-1].format = format

            # Make sure the format field is set, else infer it from the dest_file extension.
            for descriptor in descriptors:
                if not isinstance(descriptor.format, str) or not descriptor.format:
                    extension = os.path.splitext(descriptor.dest_file)[1]
                    descriptor.format = extension.lstrip('.')

        # Group the descriptors by format.
        descriptors_by_format = {}
        for descriptor in descriptors:
            # Skip over any descriptor that doesn't have a format.
            # The only way this happens is if the dest_file is '1'
            # which means "print to stdout".
            if not descriptor.format:
                continue

            if descriptor.format not in descriptors_by_format:
                descriptors_by_format[descriptor.format] = []
            descriptors_by_format[descriptor.format].append(descriptor)

        # The yaml format for one SimDB report verif test is:
        #
        #     - name:  <test_name>_<format>
        #       group: <format>/<test_name>
        #       args:  <sim_cmd>         // not including executable
        #       verif:
        #       - report1.<format>
        #       - report2.<format>
        #       ...
        for format, descriptors in descriptors_by_format.items():
            sim_args = self.sim_cmd.split()
            sim_args = ' '.join(sim_args[1:])  # Remove the executable name.

            test_yaml = {
                'name': f"{self.test_name}_{format}",
                'group': format + "/" + self.test_name,
                'args': sim_args,
                'verif': [desc.dest_file for desc in descriptors]
            }
            test_yamls.append(test_yaml)

        return test_yamls

class ReportDescriptor:
    def __init__(self):
        self.pattern = None
        self.def_file = None
        self.dest_file = None
        self.format = None

# Go through the CMakeLists.txt file and find all the Sparta tests.
sparta_tests = []
with open(args.cmake_list_file, 'r') as f:
    cmake_dir = os.path.dirname(args.cmake_list_file)
    cmake_text = f.read()
    for line in cmake_text.splitlines():
        test = SpartaTest.CreateFromCMake(line, cmake_dir)
        if test:
            sparta_tests.append(test)

# Each SpartaTest can "expand" into multiple verif_simdb_reports tests.
verif_tests = []
for test in sparta_tests:
    verif_tests.extend(test.ExtractVerifTests())

# Write the tests to the output YAML file.
with open(args.outfile, 'w') as fout:
    yaml.dump(verif_tests, fout, default_flow_style=False)
