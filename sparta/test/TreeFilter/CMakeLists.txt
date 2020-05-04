project(Treefilter_test)

sparta_add_test_executable(TreeFilter_test TreeFilter_main.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeFilter_test TreeFilter_test_RUN)
