project(TreeNodeScope_test)

add_executable(TreeNodeScope_test TreeNodeScope_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNodeScope_test TreeNodeScope_test_RUN)
