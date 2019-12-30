project(Trigger_test)

add_executable(Trigger_test Trigger_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Trigger_test Trigger_test_RUN)
