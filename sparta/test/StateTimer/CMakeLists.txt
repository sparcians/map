project(StateTimer_test)

add_executable(StateTimer_test StateTimer_test.cpp)

include(../TestingMacros.cmake)

sparta_test(StateTimer_test StateTimer_test_RUN)
