project(Staticinit_test)

sparta_add_executable(StaticInit_test StaticInit_test.cpp)

include(../TestingMacros.cmake)

sparta_test(StaticInit_test StaticInit_test_RUN)
