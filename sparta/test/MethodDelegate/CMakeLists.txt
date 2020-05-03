project(Methoddelegate_test)

sparta_add_executable(MethodDelegate_test main.cpp)

include(../TestingMacros.cmake)

sparta_test(MethodDelegate_test MethodDelegate_test_RUN 10)
