cmake_minimum_required (VERSION 3.15)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (sparta CXX)

set (GEN_DEBUG_INFO ON CACHE BOOL "Genearate debug info in compile & link -g")

set (SPARTA_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include (${SPARTA_BASE}/cmake/sparta-config.cmake)

# Add RapidJSON
include_directories (SYSTEM ${RAPIDJSON_INCLUDE_DIRS})

# Add YAML CPP
include_directories (SYSTEM ${YAML_CPP_INCLUDE_DIR})

# Add local includes
include_directories ("./")
include_directories ("simdb/include")

# Add Boost (not typical system install)
include_directories (SYSTEM ${Boost_INCLUDE_DIRS})

# Add HDF5 (not typical system install)
include_directories (SYSTEM ${HDF5_C_INCLUDE_DIRS})
include_directories (SYSTEM ${HDF5_INCLUDE_DIRS})

# Add the source for libsparta.a
list (APPEND SourceCppFiles
            src/ArgosOutputter.cpp
            src/AsyncTimeseriesReport.cpp
            src/AsyncNonTimeseriesReport.cpp
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
            src/DatabaseContextCounter.cpp
            src/DatabaseSchema.cpp
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
            src/ReportHeader.cpp
            src/ReportRepository.cpp
            src/ReportTimeseries.cpp
            src/ReportVerifier.cpp
            src/Resource.cpp
            src/SpartaException.cpp
            src/RootTreeNode.cpp
            src/Scheduler.cpp
            src/Scheduleable.cpp
            src/Simulation.cpp
            src/SimulationConfiguration.cpp
            src/SimulationInfo.cpp
            src/SingleUpdateReport.cpp
            src/SINodeHierarchy.cpp
            src/StatisticDef.cpp
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
add_compile_options(-Werror -fPIC
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

# Add a custom CMake Modules directory
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

# CppCheck Support
set (CPPCHECK_XML_OUTPUT "${PROJECT_BINARY_DIR}/analysis/cppcheck/cppcheck_analysis.xml")
set (CPPCHECK_EXCLUDES
  ${MAP_BINARY_DIR}
  ${SPARTA_BASE}/test
)
find_package (Cppcheck)

add_library (sparta ${SourceCppFiles})
set (SPARTA_STATIC_LIBS ${PROJECT_BINARY_DIR}/libsparta.a)

# Build the SimDB library
add_subdirectory (simdb)

#
# Testing, examples, and tools
#
add_subdirectory (test EXCLUDE_FROM_ALL)
add_subdirectory (example EXCLUDE_FROM_ALL)


#
# Installation
#
install(DIRECTORY sparta/ DESTINATION include/sparta)
install(FILES ${SPARTA_STATIC_LIBS} DESTINATION lib)
