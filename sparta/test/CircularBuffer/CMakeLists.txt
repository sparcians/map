project(CircularBuffer_test)

sparta_add_test_executable(CircularBuffer_test CircularBuffer_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(CircularBuffer_test CircularBuffer_test_RUN)
