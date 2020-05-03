project(Parameter_test)

sparta_add_executable(Parameter_test Device.cpp Param_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Parameter_test Parameter_test_RUN)
