project(CommandLineSimulatorArgs_test)

sparta_add_executable(CommandLineSimulatorArgs_test CommandLineSimulatorArgs_test.cpp)

include(../../TestingMacros.cmake)

sparta_copy(CommandLineSimulatorArgs_test *.yaml)

sparta_test(CommandLineSimulatorArgs_test CommandLineSimulatorArgs_test_RUN)
