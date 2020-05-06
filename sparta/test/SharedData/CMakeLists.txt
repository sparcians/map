project(SharedData)

sparta_add_test_executable(SharedData_test SharedData_test.cpp)

include(../TestingMacros.cmake)

sparta_test(SharedData_test SharedData_test_RUN)
