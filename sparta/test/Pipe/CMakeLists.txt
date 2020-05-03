project(Pipe_test)

sparta_add_executable(Pipe_test Pipe_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Pipe_test Pipe_test_RUN)
