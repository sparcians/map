project(Systemc_test)

sparta_add_test_executable(SystemC_test SystemC_test.cpp)
target_link_libraries     (SystemC_test SystemC::systemc)

sparta_test(SystemC_test SystemC_test_RUN)
sparta_copy(SystemC_test scheduler.debug.EXPECTED)
