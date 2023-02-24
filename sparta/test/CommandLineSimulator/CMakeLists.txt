project(CommandLineSimulator_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(CommandLineSimulator_test CommandLineSimulator_test.cpp)

add_subdirectory(Args)

sparta_recursive_copy(CommandLineSimulator_test archs)
sparta_recursive_copy(CommandLineSimulator_test other_archs)
sparta_recursive_copy(CommandLineSimulator_test deep_archs)
sparta_recursive_copy(CommandLineSimulator_test configs)
sparta_recursive_copy(CommandLineSimulator_test my_test.yaml)
sparta_recursive_copy(CommandLineSimulator_test your_test.yaml)
sparta_recursive_copy(CommandLineSimulator_test sample_feat_opts.yaml)

sparta_named_test(CMDLS_arch_test1a CommandLineSimulator_test --arch test1)
sparta_named_test(CMDLS_arch_test2a CommandLineSimulator_test --arch test2)
sparta_named_test(CMDLS_arch_test3a CommandLineSimulator_test --arch test3)
sparta_named_test(CMDLS_arch_test4a CommandLineSimulator_test --arch test4)
sparta_named_test(CMDLS_arch_test1b CommandLineSimulator_test --arch test1.yaml)
sparta_named_test(CMDLS_arch_test2b CommandLineSimulator_test --arch test2.yml)
sparta_named_test(CMDLS_arch_test5_other CommandLineSimulator_test --arch test5)

sparta_named_test(CMDLS_arch_test5a CommandLineSimulator_test --arch-search-dir other_archs --arch test5)
sparta_named_test(CMDLS_arch_test6a CommandLineSimulator_test --arch-search-dir other_archs --arch test6)
sparta_named_test(CMDLS_arch_test7a CommandLineSimulator_test --arch-search-dir deep_archs --arch include_tests)

sparta_named_test(CMDLS_config_test CommandLineSimulator_test -c my_test.yaml)

# The configs cannot be both an architecture and a customer
# configuration.  This is no longer allowed.
# sparta_named_test(CMDLS_arch_test_ignore3 CommandLineSimulator_test -c configs/test_ignore2.yaml)
# sparta_named_test(CMDLS_arch_test_ignore1 CommandLineSimulator_test -c configs/test_ignore.yaml)
sparta_named_test(CMDLS_arch_test_ignore2 CommandLineSimulator_test --arch-search-dir configs --arch test_ignore)
sparta_named_test(CMDLS_arch_test_ignore4 CommandLineSimulator_test --arch-search-dir configs --arch test_ignore2)
