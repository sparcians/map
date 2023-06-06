project(Color_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(Color_test Color_test.cpp)

sparta_test(Color_test Color_test_RUN)
