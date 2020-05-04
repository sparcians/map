project(Tag_test)

sparta_add_test_executable(Tag_test Tag_main.cpp)

include(../TestingMacros.cmake)

sparta_test(Tag_test Tag_test_RUN)
