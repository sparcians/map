#!/usr/bin/env bash

set -x

echo "Starting Build Entry"
echo "HOME:" $HOME
echo "GITHUB_WORKSPACE:" $GITHUB_WORKSPACE
echo "GITHUB_EVENT_PATH:" $GITHUB_EVENT_PATH
echo "PWD:" `pwd`

CXX_COMPILER=${COMPILER/clang/clang++}
CXX_COMPILER=${CXX_COMPILER/gcc/g++}

#
# Compile Sparta Infra (always build with release)
#   Have other build types point to release
#
echo "Building Sparta Infra"
cd ${GITHUB_WORKSPACE}/sparta
mkdir -p ${BUILD_TYPE}
cd ${BUILD_TYPE}
CC=$COMPILER CXX=$CXX_COMPILER  cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=/usr/local/
if [ $? -ne 0 ]; then
    echo "ERROR: CMake for Sparta framework failed"
    exit 1
fi

sudo make -j$(nproc --all) install
BUILD_SPARTA=$?
if [ ${BUILD_SPARTA} -ne 0 ]; then
    echo "ERROR: build/install for sparta FAILED!!!"
    echo "$(<install.log)"
    exit 1
fi
rm install.log

if [ "${BUILD_TYPE}" = "Release" ]; then
    make -j$(nproc --all) regress
    if [ $? -ne 0 ]; then
        echo "ERROR: regress of sparta FAILED!!!"
        exit 1
    fi

    make -j$(nproc --all) example_regress
    if [ $? -ne 0 ]; then
        echo "ERROR: example_regress of sparta FAILED!!!"
        exit 1
    fi

    make -j$(nproc --all) core_example_regress
    if [ $? -ne 0 ]; then
        echo "ERROR: example_regress of sparta FAILED!!!"
        exit 1
    fi
fi
