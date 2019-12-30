project(Pipeline_test)

add_executable(Pipeline_test Pipeline_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Pipeline_test Pipeline_test_RUN)
