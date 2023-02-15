cmake_minimum_required(VERSION 3.15)

project(transactiondb)

set(TRANSACTIONDB_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
get_property(YAML_CPP_INCLUDE_DIR TARGET yaml-cpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
##################################################################
# Build the transactiondb.so
##################################################################
# FIXME use consistent cmake recipe for python extensions here and in ..
add_custom_command(OUTPUT transactiondb.sh
  COMMAND printf 'SYSINCDIRS="${YAML_CPP_INCLUDE_DIR}"
  PY_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src"
  SPARTA_BASE="${SPARTA_BASE}"
  TARGETDIR="lib"
  BUILD_MODULES="transactiondb"
  REQUIRED_SPARTA_LIBS="sparta simdb hdf5 sqlite3"
  HDF5_LIBS="${HDF5_LIBRARIES}"
  LIB_SPARTA_BUILT_PATH="${MAP_BINARY_DIR}/sparta ${MAP_BINARY_DIR}/sparta/simdb"
  CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  CC="${CMAKE_C_COMPILER}"
  CXX="${CMAKE_CXX_COMPILER}"
  LD="${CMAKE_LINKER}"
  ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py build_ext --inplace;' > transactiondb.sh && chmod +x transactiondb.sh
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/setup.py)

add_custom_target (transactiondb_sh_driver ALL DEPENDS transactiondb.sh)
add_dependencies(transactiondb_sh_driver sparta)

add_custom_command(OUTPUT lib/transactiondb.so
  COMMAND ./transactiondb.sh
  DEPENDS
  transactiondb.sh)

add_custom_target (transactiondb_driver ALL DEPENDS
  lib/transactiondb.so
  ${TRANSACTIONDB_SRC_DIR}/common.pxd
  ${TRANSACTIONDB_SRC_DIR}/transactiondb.pyx
  ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
  ${TRANSACTIONDB_SRC_DIR}/Reader.hpp
  ${TRANSACTIONDB_SRC_DIR}/TransactionDatabaseInterface.hpp)
add_dependencies(transactiondb_driver sparta)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/ TYPE LIB)
