project(Array_test)

add_executable(Array_test Array_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Array_test Array_test_RUN)
