
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

## Current build status

[![CircleCI](https://circleci.com/gh/sparcians/map.svg?style=svg)](https://circleci.com/gh/sparcians/map)
[![MacOS Build Status](https://dev.azure.com/sparcians/map/_apis/build/status/sparcians.map?branchName=master&label=MacOS)](https://dev.azure.com/sparcians/map/_build/latest?definitionId=1&branchName=master)
[![Documentation](https://github.com/sparcians/map/workflows/Documentation/badge.svg)](https://sparcians.github.io/map/)

## Updating Regression/Build Environments

CI files are generated when the command `conda smithy rerender` is run
inside a MAP clone.  That command uses the following files to control
the generation of the CI-specific control files:

- `conda-forge.yml` - defines which platforms you want to support and some other higher-level things
- `conda.recipe/conda_build_config.yaml` - defines lists of values for variables that are used in meta.yaml
- `conda.recipe/meta.yaml` - uses variables (stuff inside {{ varname }} double curlies)

To update versions of OSes, edit the following file:
https://github.com/sparcians/map/blob/master/conda.recipe/conda_build_config.yaml
and then run `conda smithy rerender`.  (Ensure `conda install
conda-smithy` into your conda installation for that command to exist).