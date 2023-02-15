cmake_minimum_required (VERSION 3.15)

project(MAP)

set (MAP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

set (SPARTA_BASE ${CMAKE_CURRENT_SOURCE_DIR}/sparta)

OPTION(ENABLE_SPARTA "Enable sparta component" ON)
OPTION(ENABLE_HELIOS "Enable helios" ON)

if (ENABLE_SPARTA)
add_subdirectory (sparta)
endif ()

if (ENABLE_HELIOS)
add_subdirectory (helios)
endif ()
