project(SIMDB_Utils_test)

add_executable(SIMDB_Utils_test Utils_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SimdbTestingMacros.cmake)

simdb_test(SIMDB_Utils_test SIMDB_Utils_test_RUN)
