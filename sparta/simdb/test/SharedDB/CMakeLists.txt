project(SIMDB_SharedDB_test)

add_executable(SIMDB_SharedDB_test SharedDB_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SimdbTestingMacros.cmake)

simdb_recursive_copy(SIMDB_SharedDB_test test_dbs)

simdb_test(SIMDB_SharedDB_test SIMDB_SharedDB_test_RUN)
