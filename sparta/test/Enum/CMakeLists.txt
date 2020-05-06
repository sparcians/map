project(Enum_test)

sparta_add_test_executable(Enum_test Enum_main.cpp)

include(../TestingMacros.cmake)

sparta_test(Enum_test Enum_test_RUN)
