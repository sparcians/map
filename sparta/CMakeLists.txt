cmake_minimum_required (VERSION 3.14)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

project (sparta CXX)

set (SPARTA_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include (${SPARTA_BASE}/cmake/sparta-config.cmake)

# Add RapidJSON
include_directories (SYSTEM ${RAPIDJSON_INCLUDE_DIRS})

# Add YAML CPP
include_directories (${YAML_CPP_INCLUDE_DIR})

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

add_library (sparta ${SourceCppFiles})
set (SPARTA_STATIC_LIBS ${PROJECT_BINARY_DIR}/libsparta.a)

execute_process (COMMAND bash "-c" "git describe --tags --always" OUTPUT_VARIABLE GIT_REPO_VERSION RESULT_VARIABLE rc)
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
  message ("-- Building Sparta in Debug mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  message ("-- Building Sparta in Release mode")
else ()
  message (FATAL_ERROR "Unknown CMAKE_BUILD_TYPE.  See README.md OR type 'make' at the SPARTA root")
endif ()

string (STRIP ${GIT_REPO_VERSION} GIT_REPO_VERSION)
message ("-- Sparta Version: ${GIT_REPO_VERSION}")

# Use ccache if we've got it.
find_program (CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  message ("-- Using ccache")
endif ()

# -Wpedantic
set (CMAKE_CXX_FLAGS "${OPT_FLAGS} \
 -DSPARTA_VERSION='\"${GIT_REPO_VERSION}\"' -g -Werror -fPIC \
 -Wall -Wextra -Winline -Winit-self -Wno-unused-function \
 -Wuninitialized -Wno-sequence-point -Wno-inline -Wno-unknown-pragmas \
 -Woverloaded-virtual -Wno-unused-parameter -Wno-missing-field-initializers")

if (ENABLE_SANITIZERS)
    message ("-- Building with address and undefined behavior sanitizers")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
    set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  # -stdlib=libc++
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic -Wno-gnu-zero-variadic-macro-arguments ")
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
  ${CMAKE_BINARY_DIR}
  ${SPARTA_BASE}/test
)
find_package (Cppcheck)

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

