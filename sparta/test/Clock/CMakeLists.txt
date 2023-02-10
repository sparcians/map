project(Clock_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(Clock_test Clock_test.cpp)

sparta_test(Clock_test Clock_test_RUN)
