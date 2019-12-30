project(Dataview_test)

add_executable(DataView_test DataView_test.cpp)

include(../TestingMacros.cmake)

sparta_test(DataView_test DataView_test_RUN)
