#!/bin/sh

# Runs a test on the skeleton pipeline to make sure read-final-config matches exactly with a final config
# since we need a full simulation environment to use this feature it's bolted on here. 

set -e 
set -x

./sparta_skeleton -p top.producer0.params.test_param 1000 --write-final-config final.yaml
./sparta_skeleton --read-final-config final.yaml --write-final-config test.yaml

grep -o '^[^#]*' final.yaml > final_stripped.yaml
grep -o '^[^#]*' test.yaml > test_stripped.yaml

diff test_stripped.yaml final_stripped.yaml || exit 1
