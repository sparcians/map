project(Systemc_test)

if (NOT DEFINED ENV{SYSTEMC_HOME})
  message (FATAL_ERROR "SYSTEMC_HOME is not defined -- cannot include this test")
endif ()

sparta_add_test_executable(SystemC_test SystemC_test.cpp fir.cpp tb.cpp)
target_link_libraries     (SystemC_test SystemC::systemc)

include(../TestingMacros.cmake)

sparta_test(SystemC_test SystemC_test_RUN)
sparta_copy(SystemC_test scheduler.debug.EXPECTED)
