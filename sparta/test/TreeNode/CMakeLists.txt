project(Treenode_test)

add_executable(TreeNode_test TreeNode_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNode_test TreeNode_test_RUN)
sparta_copy(TreeNode_test *.json)
