cmake_minimum_required(VERSION 3.15)
project (PipeViewer)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/sparta-config.cmake)

# If we are in virtualenv or conda, that takes prio over any system python there might be
set(Python3_FIND_VIRTUALENV FIRST)
find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter)

# Set up the include and link dirs for python's setuptools using setup.cfg file 
# HDF5: need to extract dirs from paths to libs
foreach(lib IN LISTS HDF5_LIBRARIES)
  get_filename_component(d ${lib} DIRECTORY)
  list(APPEND _LIB_DIRS ${d})
endforeach()
list(REMOVE_DUPLICATES _LIB_DIRS)
# And we add sparta and simdb location
list(APPEND _LIB_DIRS "$<TARGET_FILE_DIR:sparta>" "$<TARGET_FILE_DIR:simdb>")
# Write actual setup.cfg
file(GENERATE
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg
  CONTENT
"
[build_ext]
include_dirs=$<JOIN:$<TARGET_PROPERTY:sparta,INCLUDE_DIRECTORIES>,:>
library_dirs=$<JOIN:${_LIB_DIRS},:>
")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp
  COMMAND ${Python3_EXECUTABLE} setup.py build_ext
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/setup.py
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/setup.cfg
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/src/foo/__init__.py
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/src/foo/c_foo.pxd
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/src/foo/foo_ext.pxd
  #    ${CMAKE_CURRENT_SOURCE_DIR}/extras/python-bindings/src/foo/foo_ext.pyx
    sparta simdb
    ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg
  COMMENT "Building pipeViwer and its depenencies"
)
add_custom_target(pipeView ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp)

install(CODE
  "execute_process(
    COMMAND ${Python3_EXECUTABLE} setup.py install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}"
)

#find_package (Python3 3.7 REQUIRED COMPONENTS Interpreter Development)
#
#set (CMAKE_CXX_STANDARD 17)
#set (CMAKE_CXX_STANDARD_REQUIRED ON)
#
#if(NOT ${Python3_FOUND})
#  message(WARNING "Cmake could not find python3, trying to fall back on system default python3")
#  find_program(Python3_EXECUTABLE 'python3')
#endif()
#
# Point find_package to the right parts of the source and build tree
#set(CMAKE_MODULE_PATH  ${CMAKE_SOURCE_DIR}/sparta/cmake ${CMAKE_MODULE_PATH})
#include_directories(${SPARTA_BASE} ${SPARTA_BASE}/simdb/include ./)
#include(${CMAKE_SOURCE_DIR}/sparta/cmake/sparta-config.cmake)
#
#set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/setup.py")
#
#set(PY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build/core.so.built")
#
#add_custom_command(OUTPUT "${PY_OUTPUT}"
#         COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER} TARGETDIR=${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core -- ${Python3_EXECUTABLE} ${SETUP_PY} build_ext --inplace
#                   COMMAND ${CMAKE_COMMAND} -E touch ${PY_OUTPUT}
#                   DEPENDS ${SETUP_PY} "${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/src/core.pyx"
#                   )
#add_custom_target(pipe_view_py ALL DEPENDS "${PY_OUTPUT}")
##
#add_subdirectory(transactiondb)
#add_subdirectory(transactionsearch)
#add_subdirectory(argos_dumper)
#
#install(DIRECTORY pipe_view/ DESTINATION pipe_view PATTERN "__pycache__" EXCLUDE)
