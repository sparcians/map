project(ContextCounter_test)

sparta_add_executable(ContextCounter_test ContextCounter_test.cpp)

include(../TestingMacros.cmake)

sparta_test(ContextCounter_test ContextCounter_test_RUN)
