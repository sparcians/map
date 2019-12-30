project(Utils_test)

add_executable(Utils_test Utils_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Utils_test Utils_test_RUN)
