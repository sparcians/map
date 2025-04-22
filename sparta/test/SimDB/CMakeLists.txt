project(SimDB_test)
include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(SimDB_test SimDB_test.cpp)
sparta_test(SimDB_test SimDB_test_RUN)

