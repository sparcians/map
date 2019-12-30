project(TreeNodePrivacy_test)

add_executable(TreeNodePrivacy_test TreeNodePrivacy_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNodePrivacy_test TreeNodePrivacy_test_RUN)
