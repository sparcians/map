# PipeViewer Transaction Viewer

## Prerequisites

- this project requires Cython and wxPython
- create a conda environment from the root of MAP
  - Get miniconda and install: https://docs.conda.io/en/latest/miniconda.html
  - Install JSON and Yaml parsers:
    o conda install -c conda-forge jq
    o conda install -c conda-forge yq
  - Create a sparta conda development environment:
    o ./scripts/create_conda_env.sh sparta dev
  - Activate the environment
    o conda activate sparta
  - Install Cython and wxPython
    o sudo pip3 install cython wxPython
- to build this tool, you must use cmake from the root of MAP
  - cd $(git rev-parse --show-toplevel); mkdir release; cd release
  - cmake -DCMAKE_BUILD_TYPE=Release ..
  - make
