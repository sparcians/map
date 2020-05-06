project(Clock_test)

sparta_add_test_executable(Clock_test Clock_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Clock_test Clock_test_RUN)
