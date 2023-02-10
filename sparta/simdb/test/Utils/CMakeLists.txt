project(SIMDB_Utils_test)

add_executable(SIMDB_Utils_test Utils_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SimdbTestingMacros.cmake)

simdb_test(SIMDB_Utils_test SIMDB_Utils_test_RUN)
