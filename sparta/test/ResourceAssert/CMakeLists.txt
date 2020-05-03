project(Resourceassert_test)

sparta_add_executable(ResourceAssert_test ResourceAssert_test.cpp)

include(../TestingMacros.cmake)

sparta_test(ResourceAssert_test ResourceAssert_test_RUN)
