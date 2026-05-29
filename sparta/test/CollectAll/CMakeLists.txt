project(CollectAll_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(CollectAll_test CollectAll_test.cpp)
sparta_test(CollectAll_test CollectAll_test_RUN)

