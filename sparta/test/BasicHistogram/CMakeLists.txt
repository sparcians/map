project(BasicHistogram_test)

sparta_add_test_executable(BasicHistogram_test BasicHistogram_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(BasicHistogram_test BasicHistogram_test_RUN)

