project(Audience_test)

sparta_add_test_executable(Audience_test Audience_main.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(Audience_test Audience_test_RUN)
