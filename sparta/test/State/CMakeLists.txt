project(State_test)

sparta_add_executable(State_test State_main.cpp)

include(../TestingMacros.cmake)

sparta_test(State_test State_test_RUN)
