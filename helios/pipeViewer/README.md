# PipeViewer Transaction Viewer

## Prerequisites

- This project requires Cython and wxPython, which are available in the Conda environment
- Download mini-conda (https://docs.conda.io/en/latest/miniconda.html)
- Create a conda environment from the root of MAP.  Follow any directions the script may request.
  - `./scripts/create_conda_env.sh sparta dev`
- To build this tool and its dependent libraries, use cmake from the root of MAP using the created conda environment
  - `conda activate sparta`
  - `cd $(git rev-parse --show-toplevel); mkdir release; cd release`
  - `cmake -DCMAKE_BUILD_TYPE=Release ..`
  - `make`
