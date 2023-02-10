project(Parameter_test)

sparta_add_test_executable(Parameter_test Device.cpp Param_test.cpp)

sparta_test(Parameter_test Parameter_test_RUN)
