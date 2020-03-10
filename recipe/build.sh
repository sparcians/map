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
cd example && ctest -j "$CPU_COUNT"

# --test-action test (creating Test.xml results to load into a dashboard)
# only works after the tests are built, ctest needs more configuration
# to know how to only build the tests. Punting for now.
#
# # running with --test-action test creates Testing/<datetime>/Test.xml
# # that can be loaded into the CI test result tracker
# # We could and possibly should combine all of the tests into
# # one ctest run or a special target that gets run with --test-action
# (cd test && ctest --test-action test -j "$CPU_COUNT")
# (cd simdb && ctest --test-action test -j "$CPU_COUNT")
# (cd example && ctest --test-action test -j "$CPU_COUNT")


cmake --build . --target install

popd
