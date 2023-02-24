project(ContextCounter_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(ContextCounter_test ContextCounter_test.cpp)

sparta_test(ContextCounter_test ContextCounter_test_RUN)
