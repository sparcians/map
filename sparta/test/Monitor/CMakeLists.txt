project(Monitor_test)

sparta_add_executable(monitor_test Monitor_main.cpp)

include(../TestingMacros.cmake)

sparta_test(monitor_test monitor_test_RUN)
