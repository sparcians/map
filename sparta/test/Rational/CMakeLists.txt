project(Rational_test)

sparta_add_test_executable(Rational_test main.cpp)

include(../TestingMacros.cmake)

sparta_test(Rational_test Rational_test_RUN)
