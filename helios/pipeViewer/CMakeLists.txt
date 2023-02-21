cmake_minimum_required(VERSION 3.15)
project (PipeViewer)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/sparta-config.cmake)

# If we are in virtualenv or conda, that takes prio over any system python there might be
set(Python3_FIND_VIRTUALENV FIRST)
find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter)

# Look for wxWidgets
find_package(wxWidgets REQUIRED core base)
include(${wxWidgets_USE_FILE})

# Populate list of include dirs:
# Little helper script to find wxPython include path
execute_process(
  COMMAND ${Python3_EXECUTABLE} wxPythonInclude.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE WX_PYTHON_INC
  ERROR_VARIABLE WXCHECK_LOG
  RESULT_VARIABLE WXCHECK_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT WXCHECK_RESULT EQUAL "0")
  message(FATAL_ERROR "Could not find wxPython: ${WXCHECK_LOG}")
endif()
list(APPEND _INC_DIRS "$<TARGET_PROPERTY:sparta,INCLUDE_DIRECTORIES>" ${WX_PYTHON_INC} ${wxWidgets_INCLUDE_DIRS})

# Populate list of library dirs:
# Set up the include and link dirs for python's setuptools using setup.cfg file 
# HDF5: need to extract dirs from paths to libs
foreach(lib IN LISTS HDF5_LIBRARIES)
  get_filename_component(d ${lib} DIRECTORY)
  list(APPEND _LIB_DIRS ${d})
endforeach()
list(REMOVE_DUPLICATES _LIB_DIRS)
# And we add sparta and simdb location
list(APPEND _LIB_DIRS "$<TARGET_FILE_DIR:sparta>" "$<TARGET_FILE_DIR:simdb>" ${wxWidgets_LIBRARY_DIRS})

# Those get pushed to setup.cfg file
file(GENERATE
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg
  CONTENT
"
[build_ext]
include_dirs=$<JOIN:${_INC_DIRS},:>
library_dirs=$<JOIN:${_LIB_DIRS},:>
")

# Wx specific arguments get passed via LDFLAGS and CXXFLAGS in the command
foreach(def IN LISTS wxWidgets_DEFINITIONS)
  string(APPEND PYTHON_CFLAGS "-D${def} ")
endforeach()
message(WARNING " " ${wxWidgets_LIBRARIES})
string(JOIN " " PYTHON_LDFLAGS ${wxWidgets_LIBRARIES})
message(WARNING " " ${PYTHON_LDFLAGS})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp
  COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}  CFLAGS=${PYTHON_CFLAGS} LDFLAGS=${PYTHON_LDFLAGS} ${Python3_EXECUTABLE} setup.py build_ext --inplace VERBATIM
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
      ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/src/common.pxd
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/src/core.pyx
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/src/helpers.h
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/logsearch/src/log_search.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/logsearch/src/logsearch.pyx
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/PipelineDataCallback.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/Reader.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/SimpleOutputInterface.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/TransactionDatabaseInterface.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/TransactionInterval.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/helpers.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/common.pxd
      ${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/transactiondb/src/transactiondb.pyx
      sparta simdb
    ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg
  COMMENT "Building pipeViwer and its depenencies"
)
add_custom_target(pipeView ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cython.stamp)

add_subdirectory(transactionsearch)

install(CODE
  "execute_process(
    COMMAND ${Python3_EXECUTABLE} setup.py install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})"
)

# TODO: do we need to build argos_dumper?
#add_subdirectory(argos_dumper)
