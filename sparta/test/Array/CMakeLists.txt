project(Array_test)
include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(Array_test Array_test.cpp)
sparta_test(Array_test Array_test_RUN)
