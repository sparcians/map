project(BoundedValue_test)

sparta_add_test_executable(BoundedValue_test BoundedValue_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_test(BoundedValue_test BoundedValue_test_RUN)

