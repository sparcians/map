project(Buffer_test)

add_executable(Buffer_test Buffer_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Buffer_test Buffer_test_RUN)
