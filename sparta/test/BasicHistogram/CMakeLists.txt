project(BasicHistogram_test)

sparta_add_test_executable(BasicHistogram_test BasicHistogram_test.cpp)

include(../TestingMacros.cmake)

sparta_test(BasicHistogram_test BasicHistogram_test_RUN)

