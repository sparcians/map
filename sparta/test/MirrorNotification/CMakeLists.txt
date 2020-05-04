project(MirrorNotifications_test)

sparta_add_test_executable(MirrorNotifications_test MirrorNotifications_test.cpp)

include(../TestingMacros.cmake)

sparta_test(MirrorNotifications_test MirrorNotifications_test_RUN)
