project(Color_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(Color_test Color_test.cpp)

sparta_test(Color_test Color_test_RUN)
