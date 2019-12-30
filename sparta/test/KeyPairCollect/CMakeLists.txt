project(Keypaircollect)

add_executable(KeyPairCollect KeyPairCollect_test.cpp)

include(../TestingMacros.cmake)

sparta_test(KeyPairCollect KeyPairCollect_RUN)
