project(Buffer_test)

sparta_add_executable(Buffer_test Buffer_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Buffer_test Buffer_test_RUN)
