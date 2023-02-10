project(Buffer_test)

sparta_add_test_executable(Buffer_test Buffer_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_test(Buffer_test Buffer_test_RUN)
