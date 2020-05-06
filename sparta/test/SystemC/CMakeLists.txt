project(Systemc_test)

set (SYSTEMC_BASE /sarc/spa/tools/systemc-2.3.0)

include_directories(${SYSTEMC_BASE}/include)
link_directories(${SYSTEMC_BASE}/lib-linux64)


sparta_add_test_executable(SystemC_test SystemC_test.cpp)

include(../TestingMacros.cmake)

sparta_test(SystemC_test SystemC_test_RUN)
sparta_copy(SystemC_test scheduler.log.basic.EXPECTED)
