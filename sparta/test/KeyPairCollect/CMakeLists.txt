project(Keypaircollect)

sparta_add_test_executable(KeyPairCollect KeyPairCollect_test.cpp)

include(../TestingMacros.cmake)

sparta_test(KeyPairCollect KeyPairCollect_RUN)
