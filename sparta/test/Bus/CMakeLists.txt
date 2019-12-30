project(Bus_test)

add_executable(Bus_test Bus_main.cpp)

include(../TestingMacros.cmake)

sparta_test(Bus_test Bus_test_RUN)
