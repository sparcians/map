
# MAP - Modeling Architectural Platform

This is a framework designed and built by expert modeling/simulation
engineers in the industry.  It's purpose is to provide a set of
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
