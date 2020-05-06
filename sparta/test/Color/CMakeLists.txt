project(Color_test)

sparta_add_test_executable(Color_test Color_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Color_test Color_test_RUN)
