project(BitArray_test)
add_executable(BitArray_test BitArray_test.cpp)
include(../TestingMacros.cmake)
sparta_test(BitArray_test BitArray_test_RUN)
