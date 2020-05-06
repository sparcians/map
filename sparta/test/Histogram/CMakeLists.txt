project(Histogram_test)

sparta_add_test_executable(Histogram_test Histogram_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Histogram_test Histogram_test_RUN)

