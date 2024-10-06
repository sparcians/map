# Locate cppcheck and add a cppcheck-analysis target
#
# This module defines
#  CPPCHECK_BIN, where to find cppcheck
#
# To help find the binary you can set CPPCHECK_ROOT_DIR to search a custom path
# Exported argumets include
#   CPPCHECK_FOUND, if false, do not try to link to cppcheck --- if (CPPCHECK_FOUND)
#
#   CPPCHECK_THREADS_ARG - Number of threads to use (default -j2)
#   CPPCHECK_PROJECT_ARG - The project to use (compile_comands.json)
#   CPPCHECK_BUILD_DIR_ARG - The build output directory (default - ${PROJECT_BINARY_DIR}/analysis/cppcheck)
#   CPPCHECK_ERROR_EXITCODE_ARG - The exit code if an error is found (default --error-exitcode=1)
#   CPPCHECK_SUPPRESSIONS - A suppressiosn file to use (defaults to .cppcheck_suppressions)
#   CPPCHECK_EXITCODE_SUPPRESSIONS - An exitcode suppressions file to use (defaults to .cppcheck_exitcode_suppressions)
#   CPPCHECK_CHECKS_ARGS - The checks to run (defaults to --enable=warning)
#   CPPCHECK_OTHER_ARGS - Any other arguments (defaults to --inline-suppr)
#   CPPCHECK_COMMAND - The full command to run the default cppcheck configuration
#   CPPCHECK_EXCLUDES - A list of files or folders to exclude from the scan. Must be the full path
#
# if CPPCHECK_XML_OUTPUT is set to an output file name before calling this. CppCheck will create an xml file with that name
# find the cppcheck binary

# if custom path check there first
if (CPPCHECK_ROOT_DIR)
  find_program (CPPCHECK_BIN
    NAMES
    cppcheck
    PATHS
    "${CPPCHECK_ROOT_DIR}"
    NO_DEFAULT_PATH)
  find_program (CPPCHECK_HTMLREPORT_BIN
    NAMES
    cppcheck-htmlreport
    PATHS
    "${CPPCHECK_ROOT_DIR}"
    NO_DEFAULT_PATH)
endif ()

find_program (CPPCHECK_BIN NAMES cppcheck)
find_program (CPPCHECK_HTMLREPORT_BIN NAMES cppcheck-htmlreport)

if (CPPCHECK_BIN)
  message ("-- Adding support for cppcheck")
  execute_process (COMMAND ${CPPCHECK_BIN} --version
    OUTPUT_VARIABLE CPPCHECK_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set (CPPCHECK_THREADS_ARG "-j1" CACHE STRING "The number of threads to use")
  set (CPPCHECK_PROJECT_ARG "--project=${PROJECT_BINARY_DIR}/compile_commands.json")
  set (CPPCHECK_BUILD_DIR_ARG "--cppcheck-build-dir=${PROJECT_BINARY_DIR}/analysis/cppcheck" CACHE STRING "The build directory to use")
  # Don't show thise errors
  if (EXISTS "${CMAKE_SOURCE_DIR}/.cppcheck_suppressions")
    set (CPPCHECK_SUPPRESSIONS "--suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck_suppressions" CACHE STRING "The suppressions file to use")
  else ()
    set (CPPCHECK_SUPPRESSIONS "" CACHE STRING "The suppressions file to use")
  endif ()

  # Show these errors but don't fail the build
  # These are mainly going to be from the "warning" category that is enabled by default later
  if (EXISTS "${CMAKE_SOURCE_DIR}/.cppcheck_exitcode_suppressions")
    set (CPPCHECK_EXITCODE_SUPPRESSIONS "--exitcode-suppressions=${CMAKE_SOURCE_DIR}/.cppcheck_exitcode_suppressions" CACHE STRING "The exitcode suppressions file to use")
  else ()
    set (CPPCHECK_EXITCODE_SUPPRESSIONS "" CACHE STRING "The exitcode suppressions file to use")
  endif ()

  set (CPPCHECK_ERROR_EXITCODE_ARG "--error-exitcode=0" CACHE STRING "The exitcode to use if an error is found")
  set (CPPCHECK_CHECKS_ARGS "--enable=all" CACHE STRING "Arguments for the checks to run")
  set (CPPCHECK_OTHER_ARGS "--inline-suppr" CACHE STRING "Other arguments")
  set (_CPPCHECK_EXCLUDES)

  ## set exclude files and folders
  foreach (ex ${CPPCHECK_EXCLUDES})
    list (APPEND _CPPCHECK_EXCLUDES "-i${ex}")
  endforeach (ex)

  set (CPPCHECK_ALL_ARGS
    ${CPPCHECK_THREADS_ARG}
    ${CPPCHECK_PROJECT_ARG}
    ${CPPCHECK_BUILD_DIR_ARG}
    ${CPPCHECK_ERROR_EXITCODE_ARG}
    ${CPPCHECK_SUPPRESSIONS}
    ${CPPCHECK_EXITCODE_SUPPRESSIONS}
    ${CPPCHECK_CHECKS_ARGS}
    ${CPPCHECK_OTHER_ARGS}
    ${_CPPCHECK_EXCLUDES}
    )

  # run cppcheck command with optional xml output for CI system
  if (NOT CPPCHECK_XML_OUTPUT)
    set (CPPCHECK_COMMAND
      ${CPPCHECK_BIN}
      ${CPPCHECK_ALL_ARGS}
      )
  else ()
    set (CPPCHECK_COMMAND
      ${CPPCHECK_BIN}
      ${CPPCHECK_ALL_ARGS}
      --xml
      --xml-version=2
      2> ${CPPCHECK_XML_OUTPUT}
      )
    set (CPPCHECK_HTMLREPORT_COMMAND
      ${CPPCHECK_HTMLREPORT_BIN}
      --source-dir ${CMAKE_SOURCE_DIR}
      --title "CppCheck"
      --file ${CPPCHECK_XML_OUTPUT}
      --report-dir ${CPPCHECK_XML_OUTPUT}_html
      )
  endif ()

else ()
    add_custom_target (cppcheck-analysis COMMAND echo cppcheck is not installed.  Install and re-run cmake)
endif ()

# handle the QUIETLY and REQUIRED arguments and set YAMLCPP_FOUND to TRUE if all listed variables are TRUE
include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (
  Cppcheck
  DEFAULT_MSG
  CPPCHECK_BIN)

mark_as_advanced (
  CPPCHECK_BIN
  CPPCHECK_THREADS_ARG
  CPPCHECK_PROJECT_ARG
  CPPCHECK_BUILD_DIR_ARG
  CPPCHECK_ERROR_EXITCODE_ARG
  CPPCHECK_SUPPRESSIONS
  CPPCHECK_EXITCODE_SUPPRESSIONS
  CPPCHECK_CHECKS_ARGS
  CPPCHECK_EXCLUDES
  CPPCHECK_OTHER_ARGS)

# If found add a cppcheck-analysis target
if (CPPCHECK_FOUND)
  message ("-- Using ${CPPCHECK_VERSION}. Use cppcheck-analysis targets to run it")
  file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/analysis/cppcheck)
  add_custom_target (cppcheck-analysis
    COMMAND ${CPPCHECK_COMMAND})
  if (CPPCHECK_XML_OUTPUT)
    add_custom_command (TARGET cppcheck-analysis POST_BUILD VERBATIM
      COMMAND ${CPPCHECK_HTMLREPORT_COMMAND}
      COMMENT "Generating HTML REPORT")
    #  message ("--   HTML Report CMD: ${CPPCHECK_HTMLREPORT_COMMAND}")
  endif ()
else ()
  message ("-- cppcheck not found")
endif ()
