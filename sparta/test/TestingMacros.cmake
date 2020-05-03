
#
# Testing macros as well as setting up Valgrind options
#

# MACROS for adding to the targets. Use these to add your tests.

# sparta_regress enforces that your binary gets built as part of the
# regression commands.
macro (sparta_regress target)
  add_dependencies (regress ${target} )
  add_dependencies (regress_valgrind ${target})
endmacro (sparta_regress)

# A function to add a sparta test with various options
function (sparta_fully_named_test name target run_valgrind)
  add_test (NAME ${name} COMMAND $<TARGET_FILE:${target}> ${ARGN})
  sparta_regress (${target})
  # Only add a valgrind test if desired.
  # The older 2.6 version of cmake has issues with this.
  # will ignore them for now.
  if (VALGRIND_REGRESS_ENABLED)
    if (run_valgrind)
          add_test (NAME valgrind_${name} COMMAND valgrind
            ${VALGRIND_OPTS} $<TARGET_FILE:${target}> ${ARGN})
          set_tests_properties (valgrind_${name} PROPERTIES LABELS ${VALGRIND_TEST_LABEL})
    endif ()
  endif ()
endfunction (sparta_fully_named_test)

# Tell sparta to run the following target with the following name.
macro (sparta_named_test name target)
  sparta_fully_named_test (${name} ${target} TRUE ${ARGN})
endmacro (sparta_named_test)

# Run the test without a valgrind test.
# This should only be used for special tests, and not an excuse to avoid
# fixing memory issues!
macro (sparta_named_test_no_valgrind name target)
  sparta_fully_named_test (${name} ${target} FALSE ${ARGN})
endmacro (sparta_named_test_no_valgrind)

# Just add the executable to the testing using defaults.
macro (sparta_test target)
  sparta_named_test (${target} ${target} ${ARGN})
endmacro (sparta_test)

# Define a macro for copying required files to be along side the build
# files.  This is useful for golden outputs in sparta's tests that need
# to be copied to the build directory.
macro (sparta_copy build_target cp_file)
    add_custom_command (TARGET ${build_target} PRE_BUILD
      COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/${cp_file} ${CMAKE_CURRENT_BINARY_DIR}/)
endmacro (sparta_copy)

# Define a macro for recursively copying required files to be along
# side the build files.  This is useful for golden outputs in sparta's
# tests that need to be copied to the build directory.
macro (sparta_recursive_copy build_target cp_file)
    add_custom_command (TARGET ${build_target} PRE_BUILD
      COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/${cp_file} ${CMAKE_CURRENT_BINARY_DIR}/)
endmacro (sparta_recursive_copy)

#
# Create a sparta test executable
#
macro (sparta_add_executable target srcs)
  add_executable (${target} ${srcs})
  target_link_libraries(${target} ${Sparta_LIBS})
endmacro (sparta_add_executable)
