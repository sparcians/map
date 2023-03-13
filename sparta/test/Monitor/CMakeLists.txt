project(Monitor_test)

sparta_add_test_executable(monitor_test Monitor_main.cpp)

sparta_test(monitor_test monitor_test_RUN)
