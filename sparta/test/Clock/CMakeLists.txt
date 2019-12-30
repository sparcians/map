project(Clock_test)

add_executable(Clock_test Clock_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Clock_test Clock_test_RUN)
