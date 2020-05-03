project(Fastcheckpoint_test)

sparta_add_executable(FastCheckpoint_test FastCheckpoint_test.cpp)

include(../TestingMacros.cmake)

sparta_test(FastCheckpoint_test FastCheckpoint_test_RUN)

add_subdirectory(FILEStream)
add_subdirectory(PersistentFastCheckpoint)

