project(PersistentFastCheckpoint_test)

sparta_add_test_executable(PersistentFastCheckpoint_test PersistentFastCheckpoint_test.cpp)

include(../../TestingMacros.cmake)

sparta_test(PersistentFastCheckpoint_test PersistentFastCheckpoint_test_RUN)
