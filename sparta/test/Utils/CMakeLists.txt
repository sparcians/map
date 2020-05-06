project(Utils_test)

sparta_add_test_executable(Utils_test Utils_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Utils_test Utils_test_RUN)
