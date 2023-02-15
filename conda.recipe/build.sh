#!/bin/bash

set -x

# before building, cleanup conda prefix to cut down on disk usage
conda clean -y -a

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

    # Don't use Frameworks, Mono on Azure Pipelines caused mis-location of sqlite
    # https://github.com/conda-forge/conda-smithy/issues/1251
    CMAKE_PLATFORM_FLAGS+=("-DCMAKE_FIND_FRAMEWORK=NEVER")

    BUILD_TEST_DEST="$SYSTEM_DEFAULTWORKINGDIRECTORY"
else

    # Override CC and CXX to use clang on Linux.  Since it depends
    # on gcc being available on Linux, cmake will default to picking up gcc
    export CC=clang
    export CXX=clang++

    BUILD_TEST_DEST="$HOME/feedstock_root/build_artifacts"
fi

#  echo env for visibility in CI
env | sort

################################################################################
#
#  BUILD & TEST MAP
#
################################################################################

df -h /

mkdir -p release
pushd release
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX:PATH="$PREFIX" \
      "${CMAKE_PLATFORM_FLAGS[@]}" \
      ..

df -h /
cmake --build . -j "$CPU_COUNT" || cmake --build . -v
df -h /
cmake --build . -j "$CPU_COUNT" --target regress
df -h /
cmake --build . -j "$CPU_COUNT" --target simdb_regress
df -h /

# The example tests are built as a part of the toplevel 'regress'
# target but they aren't run.  We have to explicitly run
# by cd'ing into the example subdir and running ctest
# because not all of the subdirs of example create their
# own <subdir>_regress target like the core example.
(cd sparta/example && CTEST_OUTPUT_ON_FAILURE=1 ctest -j "$CPU_COUNT" --test-action test)
df -h /

# if we want to create individual packages this should move into a separate install script for only SPARTA
# and we might want to create separate install targets for the headers and the libs and the doc
cmake --build . --target install
df -h /

popd


################################################################################
#
# Preserve build-phase test results so that we can track them individually
#
################################################################################
#
# conda-build will remove the directory where it cloned and built
# everything after a successful build.  It does this so that
# when it goes to the conda-build test phase and installs the conda
# package, any tests that are run will only use the installed
# conda package and not accidentally use stuff from the source
# build that isn't included in the package.
rsync -a \
    --include '**/Test.xml' \
    --include '*/' \
    --exclude '**' \
    --prune-empty-dirs  \
    --verbose \
    . "$BUILD_TEST_DEST/build_test_artifacts/" || \
    true # don't let failure of saving artifacts fail the build
