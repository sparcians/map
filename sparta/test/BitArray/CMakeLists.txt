project(BitArray_test)
sparta_add_test_executable(BitArray_test BitArray_test.cpp)
include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)
sparta_test(BitArray_test BitArray_test_RUN)
