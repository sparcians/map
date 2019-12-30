project(SIMDB_Utils_test)

add_executable(SIMDB_Utils_test Utils_test.cpp)

include(../TestingMacros.cmake)

simdb_test(SIMDB_Utils_test SIMDB_Utils_test_RUN)
