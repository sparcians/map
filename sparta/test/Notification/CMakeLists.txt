project(Notification_test.cpp)

add_executable(Notification_test Notification_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Notification_test Notification_test_RUN)
