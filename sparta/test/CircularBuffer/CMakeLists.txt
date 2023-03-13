project(CircularBuffer_test)

sparta_add_test_executable(CircularBuffer_test CircularBuffer_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(CircularBuffer_test CircularBuffer_test_RUN)
