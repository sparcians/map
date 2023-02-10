project(Array_test)
include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(Array_test Array_test.cpp)
sparta_test(Array_test Array_test_RUN)
