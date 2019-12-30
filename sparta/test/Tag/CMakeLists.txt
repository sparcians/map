project(Tag_test)

add_executable(Tag_test Tag_main.cpp)

include(../TestingMacros.cmake)

sparta_test(Tag_test Tag_test_RUN)
