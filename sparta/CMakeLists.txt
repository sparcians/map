cmake_minimum_required (VERSION 3.19)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (sparta CXX)

set (GEN_DEBUG_INFO ON CACHE BOOL "Genearate debug info in compile & link -g")

# Note that all of SimDB is optional, though included by default.
# If this option is OFF, then it is assumed that MAP was cloned
# without the --recursive option, therefore the SimDB submodule
# was not cloned. If OFF, we will not put SimDB on the include
# path, nor build the SimDB test, nor install SimDB alongside
# the Sparta headers.
option (USING_SIMDB "Enable Sparta to be used with SimDB" ON)

# The following option is typically enabled only for CI purposes.
# Commits that fail the github checks will produce a log file with
# specific failures in e.g. EXPECT_EQUAL, and those logs can be
# retrieved directly from the CI job artifacts.
option (SPARTA_TESTER_ERROR_LOG "Redirect the SpartaTester std::cerr to a log file" OFF)

set (SPARTA_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include (${SPARTA_BASE}/cmake/sparta-config.cmake)

if (USING_SIMDB)
  if (EXISTS "${SPARTA_BASE}/simdb/CMakeLists.txt")
    add_subdirectory (${SPARTA_BASE}/simdb)
  else ()
    message(FATAL_ERROR "SimDB not found. Run 'git submodule update --init --recursive'")
  endif ()
endif ()

# Add the source for libsparta.a
list (APPEND SourceCppFiles
            src/ArgosOutputter.cpp
            src/Backtrace.cpp
            src/BaseFormatter.cpp
            src/Clock.cpp
            src/ClockManager.cpp
            src/CommandLineSimulator.cpp
            src/ConfigParserYAML.cpp
            src/ContextCounter.cpp
            src/ContextCounterTrigger.cpp
            src/CounterBase.cpp
            src/CsvFormatter.cpp
            src/DAG.cpp
            src/Destination.cpp
            src/EdgeFactory.cpp
            src/EventNode.cpp
            src/ExportedPort.cpp
            src/Expression.cpp
            src/ExpressionGrammar.cpp
            src/ExpressionTrigger.cpp
            src/File.cpp
            src/JavascriptObject.cpp
            src/JsonFormatter.cpp
            src/MessageInfo.cpp
            src/MessageSource.cpp
            src/Parameter.cpp
            src/Port.cpp
            src/RegisterSet.cpp
            src/Report.cpp
            src/ReportDescriptor.cpp
            src/ReportRepository.cpp
            src/Resource.cpp
            src/SpartaException.cpp
            src/RootTreeNode.cpp
            src/Scheduler.cpp
            src/Scheduleable.cpp
            src/Scoreboard.cpp
            src/Simulation.cpp
            src/SimulationConfiguration.cpp
            src/SimulationInfo.cpp
            src/StatisticDef.cpp
            src/StatisticInstance.cpp
            src/StatisticsArchives.cpp
            src/StatisticsStreams.cpp
            src/StatsMapping.cpp
            src/TemporaryRunController.cpp
            src/TreeFilterExpression.cpp
            src/TreeFilterExpressionGrammar.cpp
            src/TreeNode.cpp
            src/TreeNodeExtensions.cpp
            src/Trigger.cpp
            src/TriggerManager.cpp
            src/Unit.cpp
            src/Vertex.cpp
            src/VertexFactory.cpp
            src/YAMLTreeEventHandler.cpp
            src/sparta.cpp
            src/State.tpp
            src/GenericUnit.cpp
)

if (USING_SIMDB)
  list (APPEND SourceCppFiles
    src/CherryPickFastCheckpointer.cpp
    src/ReportStatsCollector.cpp
  )
endif ()

# Add python support
if (COMPILE_WITH_PYTHON)
  list (APPEND SourceCppFiles
    python/sparta_support/module_sparta.cpp
    python/sparta_support/PythonInterpreter.cpp
    python/sparta_support/Completer.cpp)
endif ()

execute_process (COMMAND bash "-c" "git describe --tags --always"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REPO_VERSION RESULT_VARIABLE rc
)

if (NOT rc EQUAL "0")
  message (FATAL_ERROR "could not run git command 'git describe --tags --always', rc=${rc}")
endif ()

#
# Debug and Release are cmake internal build types.  Release will add
# the appropriate optimization flags and turn on NDEBUG automatically.
# Debug will build unoptimized and allow asserts.
#
# SPARTA cmake will _always_ have debug symbols regardless of the
# build type.
#
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message (STATUS "Building Sparta in Debug mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  message (STATUS "Building Sparta in Release mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  message (STATUS "Building Sparta in Release With Debug Info mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  message (STATUS "Building Sparta in Release Minimal Size mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "Profile")
  message (STATUS "Building Sparta with Profile support")
else ()
  message (FATAL_ERROR "Unknown CMAKE_BUILD_TYPE.  See README.md OR type 'make' at the SPARTA root")
endif ()

string (STRIP ${GIT_REPO_VERSION} GIT_REPO_VERSION)
message (STATUS "Sparta Version: ${GIT_REPO_VERSION}")

# Use ccache if we've got it.
find_program (CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  message (STATUS "Using ccache")
endif ()

# -Wpedantic
add_definitions(-DSPARTA_VERSION=\"${GIT_REPO_VERSION}\" -DRAPIDJSON_NOMEMBERITERATORCLASS)
add_compile_options(-Werror -fPIC -Wdeprecated -pedantic -Wextra
  -Wall -Wextra -Winline -Winit-self -Wno-unused-function
  -Wuninitialized -Wno-sequence-point -Wno-inline -Wno-unknown-pragmas
  -Woverloaded-virtual -Wno-unused-parameter -Wno-missing-field-initializers)

if (GEN_DEBUG_INFO)
  message (STATUS "Building with debug info -g")
  if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
      add_compile_options ( -g )
      add_link_options( -g )
  endif ()
else ()
  message (STATUS "Building without debug info -g")
endif ()

if (ENABLE_SANITIZERS)
    message (STATUS "Building with address and undefined behavior sanitizers")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined)
    set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
endif ()
if(DEFINED SPARTA_CXX_FLAGS_DEBUG AND SPARTA_CXX_FLAGS_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${SPARTA_CXX_FLAGS_DEBUG}")
  message(STATUS "Using Sparta custom debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
  message(STATUS "Using Sparta default debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
endif()
#
# If we're using CONDA, we might be using the one suggested for
# Sparta.  Need to use the llvm-ar found in the conda package to
# prevent irritating linker issues
#
if (USING_CONDA)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    unset(LLVM_AR)
    unset(LLVM_AR CACHE)
    find_program(LLVM_AR "llvm-ar")
    if (NOT LLVM_AR)
      unset(LLVM_AR)
      unset(LLVM_AR CACHE)
      find_program(LLVM_AR "llvm-ar-9")
      if (NOT LLVM_AR)
        message(FATAL_ERROR "llvm-ar is needed to link trace_tools on this system")
      else()
        SET(CMAKE_AR "llvm-ar-9")
      endif()
    else()
      SET(CMAKE_AR "llvm-ar")
    endif()
  endif()
endif()

message(STATUS "Using Compiler ${CMAKE_CXX_COMPILER_ID}, Version: ${CMAKE_CXX_COMPILER_VERSION}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  # -stdlib=libc++
  add_compile_options(-Wpedantic -Wno-gnu-zero-variadic-macro-arguments)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using GCC
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # using Intel C++
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++
endif ()

set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -U_FORTIFY_SOURCE")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g")
set(CMAKE_CXX_FLAGS_PROFILE        "-O3 -pg -g")

# CppCheck Support
# Add a custom CMake Modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
set (CPPCHECK_XML_OUTPUT "${PROJECT_BINARY_DIR}/analysis/cppcheck/cppcheck_analysis.xml")
set (CPPCHECK_EXCLUDES
  ${MAP_BINARY_DIR}
  ${SPARTA_BASE}/test
)
find_package (Cppcheck)

add_library (sparta ${SourceCppFiles})

if (USING_SIMDB)
  message (STATUS "SimDB is enabled")
  target_compile_definitions (sparta PRIVATE -DSIMDB_ENABLED=1)
  target_include_directories (sparta PUBLIC "./simdb/include/")
  install(DIRECTORY simdb/include/ DESTINATION include)
else ()
  message (STATUS "SimDB is disabled")
  target_compile_definitions (sparta PRIVATE -DSIMDB_ENABLED=0)
endif ()

# Add local includes
target_include_directories (sparta PUBLIC "./")

set (SPARTA_STATIC_LIBS ${PROJECT_BINARY_DIR}/libsparta.a)
set (SPARTA_CMAKE_MACRO_PATH ${SPARTA_BASE}/cmake)

#
# Testing, examples, and tools
#
add_subdirectory (test EXCLUDE_FROM_ALL)
add_subdirectory (example EXCLUDE_FROM_ALL)

#
# Installation
#
install(DIRECTORY sparta/ DESTINATION include/sparta)
install(DIRECTORY cache/ DESTINATION include/cache)
install(FILES ${SPARTA_STATIC_LIBS} DESTINATION lib)
install(DIRECTORY cmake/ DESTINATION lib/cmake/sparta)
install(DIRECTORY scripts/simdb/exporters/ DESTINATION bin/exporters)
install(PROGRAMS scripts/simdb/simdb_compare DESTINATION bin)
install(PROGRAMS scripts/simdb/simdb_export DESTINATION bin)
install(FILES scripts/simdb/simdb_export.py
              scripts/simdb/simdb_compare.py
              scripts/simdb/compare_utils.py DESTINATION bin)
