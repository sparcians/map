project(Collection_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(Collection_test Collection_test.cpp)

sparta_test(Collection_test Collection_test_RUN)
