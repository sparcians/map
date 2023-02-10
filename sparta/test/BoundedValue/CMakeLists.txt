project(BoundedValue_test)

sparta_add_test_executable(BoundedValue_test BoundedValue_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(BoundedValue_test BoundedValue_test_RUN)

