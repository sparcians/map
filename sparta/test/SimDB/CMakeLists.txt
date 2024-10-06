project(SimDB_test)

sparta_add_test_executable(SimDB_test SimDB_test.cpp)

sparta_copy(SimDB_test *.db)

sparta_test(SimDB_test SimDB_test_RUN)
