project(Preloading_test)

sparta_add_test_executable(Preloading_test Preloading_main.cpp)

sparta_copy(Preloading_test *.yaml)

sparta_test(Preloading_test Preloading_test_RUN)
