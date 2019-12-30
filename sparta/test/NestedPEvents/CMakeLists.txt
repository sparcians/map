project(NestedPeventhelper)

add_executable(NestedPEventHelper NestedPEventHelper_test.cpp)

include(../TestingMacros.cmake)

sparta_test(NestedPEventHelper NestedPEventHelper_RUN)

