project(FILEStream_test)

add_executable(FILEStream_test FILEStream_test.cpp)

include(../../TestingMacros.cmake)

sparta_test(FILEStream_test FILEStream_test_RUN)
