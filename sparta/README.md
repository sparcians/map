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
git clone ssh://github.com/sparcians/map
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

To build your own copy, after cloning the repo, ensure Doxygen and dot (part of the Graphviz tool suite) are installed.  Then `cd doc; make`.  On the Mac, type `open html/index.html` and peruse the documentation about the `SkeletalPipeline` and the `Core Example`.

## Getting Sparta to build on MacOS X

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

Clone sparta through ssh: `git clone ssh://github.com/sparcians/map.git`

Attempt a build:

1. `mkdir release` (directory name not important)
1. `cd release`
1. `CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Release`
1. `make`

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
