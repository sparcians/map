project(Notification_test.cpp)

sparta_add_test_executable(Notification_test Notification_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Notification_test Notification_test_RUN)
