project(PersistentFastCheckpoint_test)

sparta_add_executable(PersistentFastCheckpoint_test PersistentFastCheckpoint_test.cpp)

include(../../TestingMacros.cmake)

sparta_test(PersistentFastCheckpoint_test PersistentFastCheckpoint_test_RUN)
