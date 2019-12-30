project(Test1)

add_executable(test1 main.cpp)

include(../../TestingMacros.cmake)

sparta_test(test1 test1_RUN)
