project(Trigger_test)

sparta_add_executable(Trigger_test Trigger_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Trigger_test Trigger_test_RUN)
