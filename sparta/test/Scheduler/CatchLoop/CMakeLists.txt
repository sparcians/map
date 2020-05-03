project(SchedulerCatchLoop_test)

sparta_add_executable(SchedulerCatchLoop_test CatchLoop_test.cpp)

include(../../TestingMacros.cmake)

sparta_named_test_no_valgrind(SchedulerCatchLoop_test SchedulerCatchLoop_test)
