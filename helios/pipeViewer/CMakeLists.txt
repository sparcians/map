cmake_minimum_required(VERSION 3.15)
project (PipeViewer)

find_package(PythonInterp 3.7 REQUIRED)
find_package(PythonLibs 3.7 REQUIRED)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ${PYTHON})
    find_program(PYTHON 'python3')
endif()

set(SETUP_PY "${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core/setup.py")

set(PY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build")

add_custom_command(OUTPUT "${PY_OUTPUT}"
		   COMMAND TARGETDIR=${CMAKE_CURRENT_SOURCE_DIR}/pipe_view/core ${PYTHON} ${SETUP_PY} build_ext --inplace
                   COMMAND ${CMAKE_COMMAND} -E touch ${PY_OUTPUT}
                   )
add_custom_target(pipe_view_py ALL DEPENDS "${PY_OUTPUT}")

include_directories(${SPARTA_BASE} ${SPARTA_BASE}/simdb/include ./)

include(${SPARTA_BASE}/cmake/sparta-config.cmake)

add_subdirectory(transactiondb)
add_subdirectory(transactionsearch)
add_subdirectory(argos_dumper)
