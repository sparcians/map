project(Collection_test)

sparta_add_test_executable(Collection_test Collection_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Collection_test Collection_test_RUN)
