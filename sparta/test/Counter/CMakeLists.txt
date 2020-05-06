project(Counter_test)

sparta_add_test_executable(Counter_test Counter_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Counter_test Counter_test_RUN)
