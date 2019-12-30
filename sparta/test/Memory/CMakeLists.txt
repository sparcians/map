project(Memory_test)

add_executable(Memory_test Memory_test.cpp)

include(../TestingMacros.cmake)

sparta_named_test_no_valgrind(Memory_test Memory_test)

# This test requires a larger stack size for valgrind
# so i am adding the valgrind test specially. 
# In general, use the macros. 
if (VALGRIND_REGRESS_ENABLED)
    add_test (NAME valgrind_Memory_test COMMAND valgrind 
                                        --max-stackframe=2103512 ${VALGRIND_OPTS} $<TARGET_FILE:Memory_test>)

    set_tests_properties(valgrind_Memory_test PROPERTIES LABELS ${VALGRIND_TEST_LABEL})
endif()
