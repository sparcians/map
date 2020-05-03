project(BitArray_test)
sparta_add_executable(BitArray_test BitArray_test.cpp)
include(../TestingMacros.cmake)
sparta_test(BitArray_test BitArray_test_RUN)
