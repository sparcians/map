project(Bus_test)

sparta_add_test_executable(Bus_test Bus_main.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(Bus_test Bus_test_RUN)
