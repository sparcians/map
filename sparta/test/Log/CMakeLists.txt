project(Log_test)

add_executable(Log_test Log_test.cpp)

include(../TestingMacros.cmake)

sparta_test(Log_test Log_test_RUN)
sparta_copy(Log_test *.EXPECTED)
#sparta_copy(Log_test all.log.basic.EXPECTED)
#sparta_copy(Log_test a_out.log.basic.EXPECTED)
#sparta_copy(Log_test a_removed.log.basic.EXPECTED)
#sparta_copy(Log_test b_out.log.basic.EXPECTED)
#sparta_copy(Log_test c_out.log.basic.EXPECTED)
#sparta_copy(Log_test empty.log.EXPECTED)
#sparta_copy(Log_test e_out.log.basic.EXPECTED)
#sparta_copy(Log_test global_warn.log.basic.EXPECTED)
#sparta_copy(Log_test top_warn.log.basic.EXPECTED)
#sparta_copy(Log_test warn.log.basic.EXPECTED)

