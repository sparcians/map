project(Register_test)

sparta_add_executable(Register_test Register_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Register_test Register_test_RUN)
