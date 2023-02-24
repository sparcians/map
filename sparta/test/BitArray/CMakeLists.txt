project(BitArray_test)
sparta_add_test_executable(BitArray_test BitArray_test.cpp)
include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)
sparta_test(BitArray_test BitArray_test_RUN)
