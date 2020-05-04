project(Queue_test)

sparta_add_test_executable(Queue_test Queue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Queue_test Queue_test_RUN)
