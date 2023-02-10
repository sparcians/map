project(CycleHistogram_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(CycleHistogram_test CycleHistogram_test.cpp)
sparta_test(CycleHistogram_test CycleHistogram_test_RUN)
