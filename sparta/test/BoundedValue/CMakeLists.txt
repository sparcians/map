project(BoundedValue_test)

sparta_add_executable(BoundedValue_test BoundedValue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(BoundedValue_test BoundedValue_test_RUN)

