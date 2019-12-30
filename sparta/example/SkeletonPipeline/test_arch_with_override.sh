#!/bin/sh

# Runs a test on the skeleton pipeline to make sure
# when a parameter is manually set to a value in the ParameterSet
# constructor, such is the case with Producer::ParameterSet::arch_override_test_param.
# The test asserts that the value is what we expected after forcing a new
# value to get loaded via the --arch command.
set -e
set -x

./sparta_skeleton --write-final-config test_arch_override.yaml
sed -i.bak 's/reset_in_constructor/provided_by_arch_file/' test_arch_override.yaml
rm *.bak
./sparta_skeleton --arch test_arch_override.yaml --arch-search-dir ./ --write-final-config test_arch_override2.yaml
grep "provided_by_arch_file" test_arch_override2.yaml
