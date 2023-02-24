project(BasicHistogram_test)

sparta_add_test_executable(BasicHistogram_test BasicHistogram_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(BasicHistogram_test BasicHistogram_test_RUN)

