# PipeViewer Transaction Viewer

## Prerequisites

- this project requires Cython and wxPython
- create a conda environment from the root of MAP
  - ./scripts/create_conda_env.sh sparta dev
- to build this tool, you must use cmake from the root of MAP and use the created conda environment
  - conda activate sparta
  - cd $(git rev-parse --show-toplevel); mkdir release; cd release
  - cmake -DCMAKE_BUILD_TYPE=Release ..
  - make
