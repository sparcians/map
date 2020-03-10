#!/bin/bash

set -x

declare -a CMAKE_PLATFORM_FLAGS
if [[ $(uname) == Darwin ]]; then
    # note, conda-forge-ci-setup's run_conda_forge_build_setup_osx
    # exports MACOS_DEPLOYMENT_TARGET based on value seen in conda-build-config.yaml
    # exports CONDA_BUILD_SYSROOT based on MACOSX_DEPLOYMENT_TARGET
    # will repeat check for CONDA_BUILD_SYSROOT here since we're using it in our
    # cmake flags
    if [ -d "${CONDA_BUILD_SYSROOT}" ]; then
	echo "Found CONDA_BUILD_SYSROOT: ${CONDA_BUILD_SYSROOT}"
    else
	echo "Missing CONDA_BUILD_SYSROOT: ${CONDA_BUILD_SYSROOT}"
	exit 1
    fi

    CMAKE_PLATFORM_FLAGS+=("-DCMAKE_OSX_SYSROOT=${CONDA_BUILD_SYSROOT}")
else

    # Override CC and CXX to use clang on Linux.  Since it depends
    # on gcc being available on Linux, cmake will default to picking up gcc
    export CC=clang
    export CXX=clang++
fi


mkdir -p sparta/release
pushd sparta/release
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX:PATH="$PREFIX" \
      "${CMAKE_PLATFORM_FLAGS[@]}" \
      ..
cmake --build . -j "$CPU_COUNT" || cmake --build . -v
cmake --build . -j "$CPU_COUNT" --target regress
cmake --build . -j "$CPU_COUNT" --target simdb_regress

# The example tests are built as a part of the toplevel 'regress'
# target but they aren't run.  We have to explicitly run
# by cd'ing into the example subdir and running ctest
# because not all of the subdirs of example create their
# own <subdir>_regress target like the core example.
(cd example && ctest -j "$CPU_COUNT" --test-action test)

cmake --build . --target install

popd
