project(Register_test)

sparta_add_test_executable(Register_test Register_test.cpp)

sparta_test(Register_test Register_test_RUN)

sparta_add_test_executable(RegisterBits_test reg_bit_test.cpp)

sparta_test(RegisterBits_test RegisterBits_test_RUN)
