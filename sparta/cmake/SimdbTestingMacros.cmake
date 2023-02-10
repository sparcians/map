#
# Testing macros as well as setting up Valgrind options
#

# MACROS for adding to the targets. Use these to add your tests.

# simdb_regress enforces that your binary gets built as part of the
# regression commands.
macro (simdb_regress target)
  add_dependencies(simdb_regress ${target} )
  add_dependencies(simdb_regress_valgrind ${target})
endmacro(simdb_regress)

# A function to add a simdb test with various options
function(simdb_fully_named_test name target run_valgrind)
  add_test (NAME ${name} COMMAND $<TARGET_FILE:${target}> ${ARGN})
  simdb_regress(${target})
  # Only add a valgrind test if desired.
  # The older 2.6 version of cmake has issues with this.
  # will ignore them for now.
  if (VALGRIND_REGRESS_ENABLED)
    if (run_valgrind)
          add_test (NAME valgrind_${name} COMMAND valgrind
            ${VALGRIND_OPTS} $<TARGET_FILE:${target}> ${ARGN})
          set_tests_properties(valgrind_${name} PROPERTIES LABELS ${VALGRIND_TEST_LABEL})
    endif()
  endif()
  target_link_libraries      (${target} ${SimDB_LIBS})
  target_include_directories (${target} PUBLIC "${SPARTA_BASE}")
endfunction (simdb_fully_named_test)

# Tell simdb to run the following target with the following name.
macro(simdb_named_test name target)
  simdb_fully_named_test(${name} ${target} TRUE ${ARGN})
endmacro(simdb_named_test)

# Run the test without a valgrind test.
# This should only be used for special tests, and not an excuse to avoid
# fixing memory issues!
macro (simdb_named_test_no_valgrind name target)
  simdb_fully_named_test(${name} ${target} FALSE ${ARGN})
endmacro (simdb_named_test_no_valgrind)

# Just add the executable to the testing using defaults.
macro (simdb_test target)
  simdb_named_test(${target} ${target} ${ARGN})
endmacro (simdb_test)

# Define a macro for copying required files to be along side the build
# files.  This is useful for golden outputs in simdb's tests that need
# to be copied to the build directory.
macro (simdb_copy build_target cp_file)
    add_custom_command(TARGET ${build_target} PRE_BUILD
      COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/${cp_file} ${CMAKE_CURRENT_BINARY_DIR}/)
endmacro(simdb_copy)

# Define a macro for recursively copying required files to be along
# side the build files.  This is useful for golden outputs in simdb's
# tests that need to be copied to the build directory.
macro (simdb_recursive_copy build_target cp_file)
    add_custom_command(TARGET ${build_target} PRE_BUILD
      COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/${cp_file} ${CMAKE_CURRENT_BINARY_DIR}/)
endmacro(simdb_recursive_copy)
