project(Histogram_test)

add_executable(Histogram_test Histogram_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Histogram_test Histogram_test_RUN)

