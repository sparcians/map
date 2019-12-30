project(Treenodedebug_test)

add_executable(TreeNodeDebug_test TreeNodeDebug_test.cpp)

include(../TestingMacros.cmake)

sparta_test(TreeNodeDebug_test TreeNodeDebug_test_RUN)
