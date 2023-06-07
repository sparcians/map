
# MAP - Modeling Architectural Platform

This is a framework designed and built by expert modeling/simulation
engineers in the industry.  Its purpose is to provide a set of
classes, tools, and flows to aid in modeling/simulation of complex
hardware for the purpose of performance analysis and better hardware
designs.

These classes and tools are also designed to work with existing
platforms like Gem5 and SystemC while providing more abstract and
flexible methodologies for quick analysis and study.

MAP is broken into two parts:
1. **Sparta** -- A set of C++ classes (C++17) used to construct, bind, and run full simulation designs and produce performance analysis data in text form, database form, or HDF5. It's a modeling framework.
1. **Helios** -- A set of python tools used to visualize, analyze, and deep dive data generated for a Sparta-built simulator.  It's a visualization toolset.

## Current Regression Status

[![CircleCI](https://circleci.com/gh/sparcians/map.svg?style=svg)](https://circleci.com/gh/sparcians/map)
[![MacOS Build Status](https://dev.azure.com/sparcians/map/_apis/build/status/sparcians.map?branchName=master&label=MacOS)](https://dev.azure.com/sparcians/map/_build/latest?definitionId=1&branchName=master)
[![Documentation](https://github.com/sparcians/map/workflows/Documentation/badge.svg)](https://sparcians.github.io/map/)

## Building MAP

Building MAP is done in two parts

1. Sparta, the modeling framework: build sparta only in the sparta folder
2. Argos, the transaction viewer in Helios in the helios folder. Note that to build and use helios, you will need sparta built and installed somwehere on your system.

The MAP repository has numerous dependencies, which are listed in a
[conda recipe](https://github.com/sparcians/map/blob/master/conda.recipe/meta.yaml),
and the versions of these libraries continuously change.

However, with the use of the conda recipe, users can set up a conda
environment that will build and run the tools found in this repository.

This guide assumes the user is not familiar with conda nor has it
installed and would like to build everything (not just sparta).

1. If conda is not installed, install it
   * Get miniconda and install: https://docs.conda.io/en/latest/miniconda.html
   * You can install miniconda anywhere
1. Activate conda `conda activate`
1. Go to the root of MAP
   * `cd map`
1. Install JSON and YAML parsers
   * `conda install -c conda-forge jq`
   * `conda install -c conda-forge yq`
1. Create a sparta conda development environment
   * `./scripts/create_conda_env.sh sparta dev`
1. Activate the environment
   * `conda activate sparta`
1. To build Sparta framework components:
   * `conda activate sparta`
   * `cd map/sparta; mkdir release; cd release`
   * `cmake -DCMAKE_BUILD_TYPE=Release ..`
   * `make`
   * `cmake --install . --prefix $CONDA_PREFIX`
1. To build Helios/Argos transaction viewer:
   * `conda activate sparta`
   * `cd map/helios; mkdir release; cd release`
   * `cmake -DCMAKE_BUILD_TYPE=Release ..`
   * `make`
   * `cmake --install . --prefix $CONDA_PREFIX`

A few interesting cmake options to help resolve dependencies are:

For both Sparta and Helios:

* `-DBOOST_ROOT=<BOOST_LOCATION>`: Custom Boost location
* `-DCMAKE_INSTALL_PREFIX=`: Install prefix, defaults to a system wide location normally so you can use this for a local install in a home folder for example.

Helios only:

* `-DSPARTA_SEARCH_DIR=<SPARTA_INSTALLED_LOCATION>`: Use this to ensure helios finds Sparta, when you installed it in a non-default location  Not providing this will try and find sparta in the map source tree, but this might fail, if you did not build in a folder named `release`
* `-DPython3_ROOT_DIR=<PYTHON_LOCATION>`: Not often needed but useful to point to the right python if you are not in a conda env)


## Updating Regression/Build Environments for CI

CI files are generated when the command `conda smithy rerender` is run
inside a MAP clone.  That command uses the following files to control
the generation of the CI-specific control files:

- `conda-forge.yml` - defines which platforms you want to support and some other higher-level things
- `conda.recipe/conda_build_config.yaml` - defines lists of values for variables that are used in meta.yaml
- `conda.recipe/meta.yaml` - uses variables (stuff inside {{ varname }} double curlies)

To update versions of OSes, edit the following file:
https://github.com/sparcians/map/blob/master/conda.recipe/conda_build_config.yaml
and then run `conda smithy rerender`.

Install `conda smithy` instructions:
```
conda install -n root -c conda-forge conda-smithy
conda install -n root -c conda-forge conda-package-handling
```