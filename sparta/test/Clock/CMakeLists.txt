project(Clock_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(Clock_test Clock_test.cpp)

sparta_test(Clock_test Clock_test_RUN)
