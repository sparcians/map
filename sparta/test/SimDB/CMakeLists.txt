project(SimDB_test)

add_executable(SimDB_test SimDB_test.cpp)

include(../TestingMacros.cmake)

sparta_copy(SimDB_test *.db)

sparta_test(SimDB_test SimDB_test_RUN)
