project(TreeNodeScope_test)

sparta_add_executable(TreeNodeScope_test TreeNodeScope_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNodeScope_test TreeNodeScope_test_RUN)
