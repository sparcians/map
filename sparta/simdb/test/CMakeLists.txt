project(SIMDB_TESTS)

enable_testing()

# Setup options for valgrind testing.
set (VALGRIND_OPTS "--error-exitcode=5 \
       --leak-check=full \
       --show-reachable=yes \
       --undef-value-errors=yes \
       --soname-synonyms=somalloc=NONE")
set (VALGRIND_TEST_LABEL valgrind_test)

# Add the custom regress/regress_valgrind targets.
add_custom_target (simdb_regress)
add_custom_target (simdb_regress_valgrind)
add_custom_command(TARGET simdb_regress          POST_BUILD COMMAND ctest -LE ${VALGRIND_TEST_LABEL} -j10)
add_custom_command(TARGET simdb_regress_valgrind POST_BUILD COMMAND ctest -L  ${VALGRIND_TEST_LABEL} -j10)

add_subdirectory(CoreDatabase)
add_subdirectory(HDF5Database)
add_subdirectory(SharedDB)
add_subdirectory(SQLiteDatabase)
add_subdirectory(Thread)
add_subdirectory(Utils)
