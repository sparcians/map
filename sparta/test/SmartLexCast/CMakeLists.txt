project(Smartlexcast_test)

add_executable(SmartLexCast_test SmartLexCast_test.cpp)

include(../TestingMacros.cmake)

sparta_test(SmartLexCast_test SmartLexCast_test_RUN)
