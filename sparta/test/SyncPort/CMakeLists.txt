project(Syncport_test)

sparta_add_executable(SyncPort_test SyncPort_test.cpp)

include(../TestingMacros.cmake)

sparta_test(SyncPort_test SyncPort_test_RUN)
