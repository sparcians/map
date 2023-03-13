project(Counter_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(Counter_test Counter_test.cpp)

sparta_test(Counter_test Counter_test_RUN)
