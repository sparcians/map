project(ValidValue_test)

sparta_add_test_executable(ValidValue_test ValidValue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(ValidValue_test ValidValue_test_RUN)
