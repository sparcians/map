project(Statistic_test)

sparta_add_test_executable(Statistic_test Statistic_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Statistic_test Statistic_test_RUN)
