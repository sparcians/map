
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

1. **Sparta** -- A set of C++ classes (C++17) used to construct, bind,
   and run full simulation designs and produce performance analysis
   data in text form, database form (SimDB), or HDF5. It's a modeling
   framework.
1. **Helios** -- A set of python tools used to visualize, analyze, and
   deep dive data generated for a Sparta-built simulator.  It's a
   visualization toolset.

## Current Regression Status

[![CircleCI](https://circleci.com/gh/sparcians/map.svg?style=svg)](https://circleci.com/gh/sparcians/map)
[![MacOS Build Status](https://dev.azure.com/sparcians/map/_apis/build/status/sparcians.map?branchName=master&label=MacOS)](https://dev.azure.com/sparcians/map/_build/latest?definitionId=1&branchName=master)
[![Documentation](https://github.com/sparcians/map/workflows/Documentation/badge.svg)](https://sparcians.github.io/map/)

## MAP Development

> [!NOTE]
> map_v1 is no longer supported.
> `master`, while always compiling/regressing clean should never be used for development

Current development branches:

| Release | Description |
| map\_v2.0 | Development prior to SimDB integration |
| map\_v2.1 | SimDB integration/report generation support |
| map\_v2.2 | TreeNode Extensions API Update |
| map\_v3.0 | Brand new Argos pipeline collection mechanism coupled with a brand new Argos viewer |

Clone MAP with the `--recursive` option to also clone the git submodules, and `--branch map_v2.2` to clone
the latest stable v2.  `master` branch of Sparta should never be used (development).

The developers of the Sparta framework suggest using MAP v2.2 to start.

- `git clone git@github.com:sparcians/map.git --recursive --branch map_v2.2`

## Building MAP/Sparta

## Prerequisites

The following packages are needed to build Sparta (not the Helios tools):

- (cmake) cmake v3.22
- (libboost-all-dev) boost 1.74.0
- (yaml-cpp-dev) YAML CPP 0.7.0
- (rapidjson-dev) RapidJSON CPP 1.1.0
- (libsqlite3-dev) SQLite3 3.37.2
- (libhdf5-dev) HDF5 1.10.7
- (clang++) Clang, Version: 14.0.0 OR (g++) v13.0.0 or greater

These packages were tested with Ubuntu 22.02/24.04 (as well as WSL) and MacOS.
Use `apt`, `yum` or `brew` to install.

Clone sparta (suggest using map\_v2.2):
```
git clone --recursive git@github.com:sparcians/map --branch map_v2.2
```
Building Sparta:
```
cd map/sparta && mkdir release && cd release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Installing the libraries and headers locally on your system:
```
sudo cmake --install . --prefix /usr/local
```

## MAP v3

MAP v3 overhauls the Helios/Argos tool set.  This includes updates to
the Sparta modeling framework, although not extensive.  Specifically,
the pipeline collection backend has been enhanced/improved both in
performance and disk space.

Simulators build with map_v3 will work without modification.  However,
if the modeler uses pipeline collection (`-z` option), the modeler
might encounter errors like so:

```shell
terminate called after throwing an instance of 'sparta::SpartaException'
  what():  Uncollectable type encountered at top.cpu.core0.dcache.mshr_file.mshr_file0
```
This error indicates that the collectable type is not compatible with the new Argos tools and need to be ported.

See [doc/PairDefinitionPortingGuide.md](Pair Definition Porting Guide)
for porting information.
