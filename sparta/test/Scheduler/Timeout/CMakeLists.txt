project(SchedulerTimeout_test)

add_executable(SchedulerTimeout_test Timeout_test.cpp)

include(../../TestingMacros.cmake)

sparta_named_test_no_valgrind(SchedulerTimeout_test SchedulerTimeout_test)
