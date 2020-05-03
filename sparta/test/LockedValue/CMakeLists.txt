project(LockedValue_test)

sparta_add_executable(LockedValue_test LockedValue_test.cpp)

include(../TestingMacros.cmake)

sparta_test(LockedValue_test LockedValue_test_RUN)
