project(FastList_test)

sparta_add_test_executable(FastList_test FastList_test.cpp)

include(../TestingMacros.cmake)

sparta_test(FastList_test FastList_test_RUN)
