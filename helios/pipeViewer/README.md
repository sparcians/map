# PipeViewer Transaction Viewer

## Prerequisites

1. This project requires Cython and wxPython, which are available in the Conda environment
1. If conda is not installed, install it
  * Get miniconda and install: https://docs.conda.io/en/latest/miniconda.html
  * You can install miniconda anywhere
1. Go to the root of MAP
  * `cd map`
1. Install JSON and Yaml parsers
  * `conda install -c conda-forge jq`
  * `conda install -c conda-forge yq`
1. Create a sparta conda development environment
  * `./scripts/create_conda_env.sh sparta dev`
1. Activate the environment
  * `conda activate sparta`
1. To build this tool and its dependent libraries, use cmake from the root of MAP using the created conda environment
  * `conda activate sparta`
  * `cd $(git rev-parse --show-toplevel); mkdir release; cd release`
  * `cmake -DCMAKE_BUILD_TYPE=Release ..`
  * `make`
