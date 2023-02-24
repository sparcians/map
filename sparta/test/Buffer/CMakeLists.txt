project(Buffer_test)

sparta_add_test_executable(Buffer_test Buffer_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(Buffer_test Buffer_test_RUN)
