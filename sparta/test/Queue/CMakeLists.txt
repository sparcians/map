project(Queue_test)

add_executable(Queue_test Queue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Queue_test Queue_test_RUN)
