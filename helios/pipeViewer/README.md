# PipeViewer Transaction Viewer

## Prerequisites

- this project requires Cython and wxPython
- create a conda environment from the root of MAP
  - ./scripts/create_conda_env.sh <name> dev
  - sudo pip3 install cython wxPython
- to build this tool, you must use cmake from the root of MAP
  - cd $(git rev-parse --show-toplevel); mkdir release; cd release
  - cmake -DCMAKE_BUILD_TYPE=Release ..
  - make
- set environment var TRANSACTIONDB_MODULE_DIR=<transactiondb module dir>
