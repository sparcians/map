project(Scheduler_test)

add_subdirectory(NoLoopTest)
add_subdirectory(CatchLoop)
add_subdirectory(Timeout)

sparta_add_test_executable(Scheduler_test Scheduler_test.cpp)

sparta_test(Scheduler_test Scheduler_test_RUN)

# This project depends upon some files, we need to copy them to the build. 
# there is a copy command for this in the newer cmake.. but we want to support older cmake i guess.
sparta_copy(Scheduler_test scheduler.log.basic.EXPECTED)
