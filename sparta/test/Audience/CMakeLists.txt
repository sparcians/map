project(Audience_test)

add_executable(Audience_test Audience_main.cpp)

include(../TestingMacros.cmake)

sparta_test(Audience_test Audience_test_RUN)
