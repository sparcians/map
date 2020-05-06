project(EnumCycleHistogram_test)

sparta_add_test_executable(EnumCycleHistogram_test EnumCycleHistogram_test.cpp)

include(../TestingMacros.cmake)

sparta_test(EnumCycleHistogram_test EnumCycleHistogram_test_RUN)
