project(Audience_test)

sparta_add_test_executable(Audience_test Audience_main.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(Audience_test Audience_test_RUN)
