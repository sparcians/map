# Sparta

Sparta is a toolkit for modeling/simulation engineers to build CPU,
GPU, and platform simulations.  This is a _framework_ much like `STL`
or Boost in that it doesn't do anything on its own, but rather
provides structure/organization as well as numerous helper classes and
interfaces allowing models built with Sparta to work together for
functional simulation, performance simulation, analysis, and design.

This framework is built on the principles of a OO framework design.
It's written in C++ (mostly C++11 and some 17) as well as python.  The
concepts in modeling communication/scheduling are similar to SystemC
and Gem5, but takes a much more abstract approach to simulation
design.

## Quick Start for the Impatient, Yet Confident

Quick guide that illustrates how to build _just the framework_ and not
the visualization tools.  Highly suggested to use a conda environment as
described in map/README.md.

```

# Install the following
#
#    cmake boost hdf5 yaml-cpp rapidJSON xz sqlite doxygen
#
# Versions tested and known to work:
#    cmake     3.15
#    boost     1.71, 1.76
#    yaml-cpp  0.6,  0.7
#    RapidJSON 1.1
#    SQLite3   3.19, 3.36
#    HDF5      1.10
#    Doxygen   1.8

# Clone Sparta via the MAP GitHub repo and 'cd' into it
git clone http://github.com/sparcians/map
cd map/sparta

# Build a release version
mkdir release; cd release
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Build a debug version
mkdir debug; cd debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Regress
cd release; make regress
cd debug; make regress

# Documentation (requires Doxygen 1.8)
cd doc; make
open html/index.html

```

## Documentation

All great projects have great documentation.  The Sparta team is striving to have the same, but there's always room for improvement.  There are presentations and online documentation being built, with a few ready for use.

Specifically, start by generating your own Doxygen build or flip through the [online generated documentation](https://sparcians.github.io/map/).

We are starting to build a series of presentations made to RISC-V International via the Performance Modeling SIG:
- [Sparta Overview](https://docs.google.com/presentation/d/e/2PACX-1vS1BWtVv0x3qXKQWAeECe2gsF9cMG3Zp2HnXJw52grCAcl21lv3a9pLW6J0lZ32e5DWdZkFyUMcE_AI/pub?start=false&loop=false&delayms=3000)

To build your own copy, after cloning the repo, ensure Doxygen and dot (part of the Graphviz tool suite) are installed.  Then `cd doc; make`.  On the Mac, type `open html/index.html` and peruse the documentation about the `SkeletalPipeline` and the `Core Example`.

## Building Sparta with packages used in Continuous Integration (MacOS & CentOS7 or newer Linux)

<!-- Centos7 was the sysroot used by default for most conda-forge packages at the time of writing and as such, conda-forge
     packages should work on any linux distribution newer than Centos7.  The conda-forge sysroot pinnings were be found at
     https://github.com/conda-forge/conda-forge-pinning-feedstock/blob/119668995b2ac2c797f673ce56d51cae05f65ce4/recipe/conda_build_config.yaml#L131-L154
-->

The tested dependencies are maintained in the `conda.recipe/` directory at the toplevel of the repository.  To install packages using that same tested recipe:
1. If you already have `conda` or `mamba` installed and in your `PATH`, skip to step 3.
1. Download and install the latest [miniforge installer](https://github.com/conda-forge/miniforge#miniforge3)
  * Linux running on x86_64 `wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh && bash ./Miniforge3-Linux-x86_64.sh`
  * Macos running on x86_64 `wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-x86_64.sh && bash ./Miniforge3-MacOSX-x86_64.sh`
  * Macos running on arm64 `wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-arm64.sh && bash ./Miniforge3-MacOSX-arm64.sh`
  * Make sure to `activate` or start a new shell as directed by the installer
1. `conda install yq` it is not a dependency of Sparta unless you are using the script to create an environment.  The script will tell you to install it if you don't have it in your path.
1. From withing the `map` top level directory, run `./scripts/create_conda_env.sh <environment_name> dev` using whatever name you would like in place of `<environment_name>`to create a named [conda environment](https://docs.conda.io/projects/conda/en/latest/user-guide/concepts/environments.html) containing all of the dependencies needed to **dev**elop Sparta.   Be patient, this takes a few minutes.
1. `conda activate <environment_name>` using the `<environment_name>` you created above.
1. Follow the normal cmake-based build steps in the [Quick Start](#quick-start-for-the-impatient-yet-confident).  After running cmake for a build, you should notice that `USING_CONDA` has been set because the version string reported by the conda-forge compiler contains the string "conda".

Using conda is not a requirement for building sparta but it is *one* way to install the required dependencies.  See below for alternatives on MacOS and Ubuntu.

We leverage [the conda-forge CI management system](https://conda-forge.org/docs/user/ci-skeleton.html) to define the matrix of target machines and dependency versions that we run through CI.

Please also note that `conda` will solve the package requirements and may install newer or different packages than were installed duing CI.

## Getting Sparta to build on MacOS X

Highly suggested to use a conda environment as described in
map/README.md, but brew can work just as well.

### Time to Brew

Go to the [Brew main website](https://brew.sh) to learn more.

Open a MacOS Terminal

1. Install brew: `/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)”`
1. `brew install cmake`
1. `brew install boost` -- this assumes that boost v1.71 will be installed
1. `brew install hdf5`
1. `brew install yaml-cpp`
1. `brew install rapidJSON`
1. `brew install xz`
1. `brew install zstd`
1. `brew install cppcheck`
1. `brew install binutils`  # This is for addr2line support for error debug log generation on crash
1. `sudo easy_install Pygments==2.5.2` # Do not use HomeBrew! Needed for cppcheck-htmlreport
1. Install XCode and check for clang: `clang --version`

Additionally, there are Python packages to install
1. `pip install cython`

Clone sparta through ssh: `git clone http://github.com/sparcians/map.git`

Attempt a build:

1. `mkdir release` (directory name not important)
1. `cd release`
1. `CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Release`
2. NOTE: Some MacOS environments will need `cmake .. -DCMAKE_CXX_FLAGS=-isystem\ /usr/local/include -DCMAKE_BUILD_TYPE=Release`
3. `make`

If you're having issues on MacOS X with Boost-python not found, try this:

1. `brew install boost-python3`
1. `cd /usr/local/Cellar/boost-python3/1.71.0_1`  (assuming boost 1.71)
1. `rsync -r --progress lib/ ../../boost/1.71.0/lib/`
1. `cd ../../`
1. `mv boost-python3 boost-python3.bad`

This will copy the libraries and cmake files to boost 1.71.0 directory
(lib) proper.  There's a bug in Boost Python's cmake files where it
does not take the canonical path to the cmake root.

* Note: If running `cmake ..` from a subdirectory such as `build` (as
  in the `Attempt a build` step above) does not produce any build
  files in the `build` subdirectory/pwd, you may have already run
  `cmake .` from the project root directory. Delete the
  `CMakeCache.txt` file in the project root directory and try the
  `cmake ..` command again from the `build` subdirectory.

* Note: On MacOS X, during the `cmake` stage, you'll get a lot of
  boost warnings.  This is a bug in Boost's cmake and is fixed in the
  next revision.*

* Note: `std::uncaught_exception` is deprecated and gcc9 doesn't like
  Sparta's use of it.  Clang is ok with it, so do this to build on
  Linux box: `CC=clang CXX=clang++ cmake ..`

## Getting Sparta to build on Ubuntu and derivatives

### Install these packages

This can be done generally via "sudo apt install <package name>"

- build-essential
- clang
- python3
- cmake
- libboost-all-dev
- libyaml-cpp-dev
- rapidjson-dev
- libsqlite3-dev
- zlib1g-dev
- libhdf5-cpp-103
- libhdf5-dev
- cppcheck
- pygments (for cppcheck-htmlreport)
- doxygen graphviz (if you want generate the documentation and dot graphs)

## Notes on Building

* Note: On MacOS X, during the `cmake` stage, you'll get a lot of
 boost warnings (ignore them).  This is a bug in Boost's cmake and is
 fixed in the next revision.*

* Note: If running `cmake ..` from a subdirectory such as `build` (as
 in the `Attempt a build` step above) does not produce any build files
 in the `build` subdirectory/pwd, you may have already run `cmake .`
 from the project root directory. Delete the `CMakeCache.txt` file in
 the project root directory and try the `cmake ..` command again from
 the `build` subdirectory.

## Developing on Sparta

Bug fixes, enhancements are welcomed and encouraged.  But there are a
few rules...

* Rule1: Any bug fix/enhancement _must be accompanied_ with a test
  that illustrates the bug fix/enhancement.  No test == no acceptance.
  Documentation fixes obviously don't require this...

* Rule2: Adhere to Sparta's Coding Style. Look at the existing code
  and mimic it.  Don't mix another preferred style with Sparta's.
  Stick with Sparta's.

* There are simple style rules:
     1. Class names are `CamelCase` with the Camel's head up: `class SpartaHandler`
     1. Public class method names are `camelCase` with the camel's head down: `void myMethod()`
     1. Private/protected class method names are `camelCase_` with the camel's head down and a trailing `_`: `void myMethod_()`
     1. Member variable names that are `private` or `protected` must
        be all `lower_case_` with a trailing `_` so developers know a
        memory variable is private or protected.  Placing an `_`
        between words: preferred.
     1. Header file guards are `#pragma once`
     1. Any function/class from `std` namespace must always be
        explicit: `std::vector` NOT `vector`.  Never use `using
        namespace <blah>;` in *any* header file
     1. Consider using source files for non-critical path code
     1. Try to keep methods short and concise (yes, we break this rule a bit)
     1. Do not go nuts with `auto`.  This `auto foo(const auto & in)` is ... irritating
     1. All public APIs *must be doxygenated*.  No exceptions.

## CppCheck Support
Note that it is recomended to keep cppcheck up-to-date

```
cmake --build . --target cppcheck-analysis
# Or use make if generator was make, that is
make cppcheck-analysis
```

There is support to build XML files for CI tools.  In root level,
CMakeLists.txt add the following:
```
set(CPPCHECK_XML_OUTPUT "${PROJECT_BINARY_DIR}/analysis/cppcheck/cppcheck_analysis.xml")
```

For more configuration options see:
`sparta/cmake/modules/FindCppcheck.cmake`

(Some tweaks from https://hopstorawpointers.blogspot.com/2018/11/integrating-cppcheck-and-cmake.html)

### Generate HTML from XML
In sparta/release|debug dir, run the following command after running cppcheck with XML:

```
cppcheck-htmlreport --source-dir .. --title "CppCheck" --file analysis/cppcheck/cppcheck_analysis.xml --report-dir analysis/cppcheck/html
```

There will be an analysis/cppcheck/html/index.html file to open in a browser to review

## Cool Stuff
Here are a couple of cool things you can do with `cmake` and `make`:

1. `mkdir debug; cd debug; cmake .. -DCMAKE_BUILD_TYPE=Debug` will build a debug version
2. `cmake -DSparta_VERBOSE=YES` will spit out all of the predefines cmake is using
3. `make VERBOSE=1` will show how your files are being built/linked

### Python shell

To build Sparta with the python shell:

1. `mkdir build-python` (directory name not important)
2. `cd build-python`
3. `cmake -DCMAKE_BUILD_TYPE=[Debug|Release] -DCOMPILE_WITH_PYTHON=ON ..`
4. `make`
