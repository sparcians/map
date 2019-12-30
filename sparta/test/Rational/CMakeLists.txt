project(Rational_test)

add_executable(Rational_test main.cpp)

include(../TestingMacros.cmake)

sparta_test(Rational_test Rational_test_RUN)
