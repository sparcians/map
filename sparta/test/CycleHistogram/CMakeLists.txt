project(CycleHistogram_test)

add_executable(CycleHistogram_test CycleHistogram_test.cpp)

include(../TestingMacros.cmake)

sparta_test(CycleHistogram_test CycleHistogram_test_RUN)
