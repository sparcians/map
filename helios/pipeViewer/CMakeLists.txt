cmake_minimum_required(VERSION 3.15)
project (PipeViewer)

# If we are in virtualenv or conda, that takes prio over any system python there might be
set(Python3_FIND_VIRTUALENV FIRST)

find_package (Python3 3.7 REQUIRED COMPONENTS Interpreter Development)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ${Python3_FOUND})
  message(WARNING "Cmake could not find PythonInterp, trying to fall back on system default python3")
  find_program(Python3_EXECUTABLE 'python3')
endif()

set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/setup.py")

set(PY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build/core.so.built")

add_custom_command(OUTPUT "${PY_OUTPUT}"
         COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER} TARGETDIR=${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core -- ${Python3_EXECUTABLE} ${SETUP_PY} build_ext --inplace
                   COMMAND ${CMAKE_COMMAND} -E touch ${PY_OUTPUT}
                   DEPENDS ${SETUP_PY} "${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/src/core.pyx"
                   )
add_custom_target(pipe_view_py ALL DEPENDS "${PY_OUTPUT}")

include_directories(${SPARTA_BASE} ${SPARTA_BASE}/simdb/include ./)

include(${SPARTA_BASE}/cmake/sparta-config.cmake)

add_subdirectory(transactiondb)
add_subdirectory(transactionsearch)
add_subdirectory(argos_dumper)
