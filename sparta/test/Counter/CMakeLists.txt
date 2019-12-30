project(Counter_test)

add_executable(Counter_test Counter_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Counter_test Counter_test_RUN)
