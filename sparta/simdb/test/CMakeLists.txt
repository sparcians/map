project(SIMDB_TESTS)

# Enable testing and configures the test/DartConfiguration.tcl
include (CTest)

# Setup options for valgrind testing.
set(VALGRIND_REGRESS_ENABLED TRUE)
set (VALGRIND_OPTS
    "--error-exitcode=5"
    "--leak-check=full"
    "--show-reachable=yes"
    "--undef-value-errors=yes"
    "--suppressions=${SPARTA_BASE}/test/valgrind_leakcheck.supp"
    "--soname-synonyms=somalloc=NONE"
    )

set (VALGRIND_TEST_LABEL valgrind_test)

# Add the custom regress/regress_valgrind targets.
add_custom_target (simdb_regress)
add_custom_target (simdb_regress_valgrind)

# Since make does not pass the parallel jobs flag to ctest from the user,
# a fixed count will be set based on core count w/ a max 8
include(ProcessorCount)
ProcessorCount(NUM_CORES)
if (NOT NUM_CORES EQUAL 0)
  set(CTEST_BUILD_FLAGS -j${NUM_CORES})
  set(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${NUM_CORES})
endif()
message(STATUS "Found " ${NUM_CORES} " cores in machine (for ctest)")

# NOTE:
# running ctest with --test-action test creates Testing/<datetime>/Test.xml
# that can be loaded into the CI test result tracker
add_custom_command(TARGET simdb_regress          POST_BUILD COMMAND ctest -LE ${VALGRIND_TEST_LABEL} -j${NUM_CORES} --test-action test)
add_custom_command(TARGET simdb_regress_valgrind POST_BUILD COMMAND ctest -L  ${VALGRIND_TEST_LABEL} -j${NUM_CORES} --test-action test)

add_subdirectory(CoreDatabase)
add_subdirectory(HDF5Database)
add_subdirectory(SharedDB)
add_subdirectory(SQLiteDatabase)
add_subdirectory(Thread)
add_subdirectory(Utils)
