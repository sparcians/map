cmake_minimum_required (VERSION 3.15)

project(MAP)

set (MAP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

set (SPARTA_BASE ${CMAKE_CURRENT_SOURCE_DIR}/sparta)

add_subdirectory (sparta)
add_subdirectory (helios)
