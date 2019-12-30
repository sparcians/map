project(Preloading_test)

add_executable(Preloading_test Preloading_main.cpp)

include(../TestingMacros.cmake)

sparta_copy(Preloading_test *.yaml)

sparta_test(Preloading_test Preloading_test_RUN)
