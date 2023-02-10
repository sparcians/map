project(Collection_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(Collection_test Collection_test.cpp)

sparta_test(Collection_test Collection_test_RUN)
