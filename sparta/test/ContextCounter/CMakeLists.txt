project(ContextCounter_test)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SpartaTestingMacros.cmake)

sparta_add_test_executable(ContextCounter_test ContextCounter_test.cpp)

sparta_test(ContextCounter_test ContextCounter_test_RUN)
