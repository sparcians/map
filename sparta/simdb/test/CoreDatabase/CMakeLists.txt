project(SIMDB_CoreDatabase_test)

add_executable(SIMDB_CoreDatabase_test CoreDatabase_test.cpp)

include(../TestingMacros.cmake)

simdb_recursive_copy(SIMDB_CoreDatabase_test test_dbs)

simdb_test(SIMDB_CoreDatabase_test SIMDB_CoreDatabase_test_RUN)
