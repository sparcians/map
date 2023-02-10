project(Counter_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(Counter_test Counter_test.cpp)

sparta_test(Counter_test Counter_test_RUN)
