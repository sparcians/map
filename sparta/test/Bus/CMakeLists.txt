project(Bus_test)

sparta_add_test_executable(Bus_test Bus_main.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(Bus_test Bus_test_RUN)
