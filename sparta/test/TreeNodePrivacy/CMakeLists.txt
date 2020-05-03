project(TreeNodePrivacy_test)

sparta_add_executable(TreeNodePrivacy_test TreeNodePrivacy_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNodePrivacy_test TreeNodePrivacy_test_RUN)
