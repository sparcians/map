# PipeViewer Transaction Viewer

## Prerequisites

- This project requires Cython and wxPython, which are available in the Conda environment
- Create a conda environment from the root of MAP.  Follow any directions the script may request.
  - Get miniconda and install: https://docs.conda.io/en/latest/miniconda.html
  - Install JSON and Yaml parsers:
    o `conda install -c conda-forge jq`
    o `conda install -c conda-forge yq`
  - Create a sparta conda development environment:
    o `./scripts/create_conda_env.sh sparta dev`
  - Activate the environment
    o `conda activate sparta`
- To build this tool and its dependent libraries, use cmake from the root of MAP using the created conda environment
  - `conda activate sparta`
  - `cd $(git rev-parse --show-toplevel); mkdir release; cd release`
  - `cmake -DCMAKE_BUILD_TYPE=Release ..`
  - `make`
