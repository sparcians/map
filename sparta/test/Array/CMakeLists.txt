project(Array_test)
include(../TestingMacros.cmake)

sparta_add_test_executable(Array_test Array_test.cpp)
sparta_test(Array_test Array_test_RUN)
