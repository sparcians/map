# Argos: The PipeViewer Transaction Viewer

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

## Running Argos

If all steps in the prerequisites were followed and were successful,
then the modeler can invoke the Argos tool.

Argos requires `python3` to run on Linux and `pythonw` to run on MacOS.  To invoke:

```
# Be at the root of the MAP repo -- not required, but all commands follow from this POV.
cd map

# Linux
python3 helios/pipeViewer/pipe_view/argos.py

# MacOS
pythonw helios/pipeViewer/pipe_view/argos.py
```
If built correctly, the first window that pops up should be modal box
that is asking for a transaction database.

## Attempt to Load an Actual Database

Follow the directions on the [CoreModel
Example](https://sparcians.github.io/map/core_example.html) doxygen
page subsection "Generating Pipeouts" to generate a pipeout.

This example assumes that sparta was built in a directory called
"release" directly off of the map repo.

Load the generated pipeout:
```
# Be at the root of the MAP repo -- not required, but all commands follow from this POV.
cd map

# Linux
python3 helios/pipeViewer/pipe_view/argos.py \
    -l release/sparta/example/CoreModel/cpu_layout.alf \
    -d release/sparta/release/example/CoreModel/my_pipeout

# MacOS
pythonw helios/pipeViewer/pipe_view/argos.py \
    -l release/sparta/example/CoreModel/cpu_layout.alf \
    -d release/sparta/release/example/CoreModel/my_pipeout
```