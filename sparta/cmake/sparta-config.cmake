################################################################################
#              RequiredLibraries
#
# This file is responsible for setting up sparta to find the required
# libraries like boost, yaml_cpp, etc.
#
# Variables available afterwards.
# * ${Boost_LIBRARY_DIR}
# * ${Boost_INCLUDE_DIRS}
# * ${YAML_CPP_INCLUDE_DIR} - the path to the yaml-cpp directory.
# * ${RAPIDJSON_INCLUDE_DIRS} - the path RapidJSON
#
################################################################################

# Find Boost
set (_BOOST_COMPONENTS filesystem date_time iostreams serialization timer program_options)
if (COMPILE_WITH_PYTHON)
  list (APPEND _BOOST_COMPONENTS python)
  find_package (Python COMPONENTS Interpreter Development)
endif ()


if (Sparta_VERBOSE)
    set (Boost_VERBOSE ON) # There's also Boost_DEBUG if you need more
    set (CMAKE_FIND_DEBUG_MODE ON) # verbosity for find_package
endif()

set (Boost_USE_STATIC_LIBS OFF)

# BOOST_CMAKE logic for in versions before 1.72 to ask for the shared libraries is broken, you can only force it to use
# them if you're building shared libs?  wtf?
set (existing_build_shared ${BUILD_SHARED_LIBS})
set (BUILD_SHARED_LIBS ON)

execute_process (COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CXX_VERSION_STRING RESULT_VARIABLE rc)
if (NOT rc EQUAL "0")
    message (FATAL_ERROR "could not run compiler command '${CMAKE_CXX_COMPILER} --version', rc=${rc}")
endif ()

if (CXX_VERSION_STRING MATCHES "conda")
    set (USING_CONDA ON)
elseif (APPLE AND NOT CXX_VERSION_STRING MATCHES "^Apple")
    set (USING_CONDA ON)
else ()
    set (USING_CONDA OFF)
endif ()


if (APPLE AND NOT USING_CONDA)
  set (Boost_NO_BOOST_CMAKE ON)
  set (CMAKE_CXX_COMPILER_VERSION 10.0)
  find_package (Boost 1.65.0 REQUIRED HINTS /usr/local/Cellar/boost/* COMPONENTS ${_BOOST_COMPONENTS})
else ()
  find_package (Boost 1.65.0 REQUIRED COMPONENTS ${_BOOST_COMPONENTS})
endif ()

set (BUILD_SHARED_LIBS ${existing_build_shared})
message (STATUS "Using BOOST ${Boost_VERSION_STRING}")

# Find YAML CPP
find_package (yaml-cpp 0.6 REQUIRED)
message (STATUS "Using YAML CPP ${yaml-cpp_VERSION}")

# Find RapidJSON
find_package (RapidJSON 1.1 REQUIRED)
message (STATUS "Using RapidJSON CPP ${RapidJSON_VERSION}")

# Find SQLite3
find_package (SQLite3 3.19 REQUIRED)
message (STATUS "Using SQLite3 ${SQLite3_VERSION}")

# Find HDF5. Need to enable C language for HDF5 testing
enable_language (C)
find_package (HDF5 1.10 REQUIRED)

# Populate the Sparta_LIBS variable with the required libraries for
# basic Sparta linking
set (Sparta_LIBS sparta simdb ${HDF5_LIBRARIES} sqlite3 yaml-cpp z pthread
  Boost::date_time Boost::filesystem Boost::iostreams Boost::serialization Boost::timer Boost::program_options)

# On Linux we need to link against rt as well
if (NOT APPLE)
    list(APPEND Sparta_LIBS rt)
endif ()

#
# Python support
#
option (COMPILE_WITH_PYTHON "Compile in Python support" OFF)
if (COMPILE_WITH_PYTHON)
  # Bring in the python library and include files
  find_package (Python 3.0 REQUIRED COMPONENTS Development)
  add_definitions (-DSPARTA_PYTHON_SUPPORT -DPYTHONHOME="${Python_LIBRARY_DIRS}")
  include_directories (SYSTEM ${Python_INCLUDE_DIRS})
  list (APPEND Sparta_LIBS Python::Python Boost::python)
endif ()

#
# SystemC support
#

# SystemC support.  This will enable/disable Sparta Scheduler support
option (COMPILE_WITH_SYSTEMC "Compile in SystemC support" OFF)
if (COMPILE_WITH_SYSTEMC)
  find_package(SystemCLanguage HINTS ENV{SYSTEMC_HOME})
  if (SystemCLanguage_FOUND)
    if (NOT ${SystemC_CXX_STANDARD} EQUAL ${CMAKE_CXX_STANDARD})
      message (FATAL_ERROR "SystemC was not built with the C++ standard (${SystemC_CXX_STANDARD}) required by Sparta (${CMAKE_CXX_STANDARD})")
    endif ()
    message (STATUS "SystemC enabled: ${SystemCLanguage_VERSION}")
    set (SYSTEMC_SUPPORT True)
    add_definitions(-DSYSTEMC_SUPPORT)
  else ()
    message (STATUS "SystemC not found -- disabling tests/examples")
  endif ()
else()
  message(STATUS "SystemC support disabled")
endif()

#
# Conda support
#
if (USING_CONDA)
    message (STATUS "Using CONDA toolchain")
    # if you don't do this, cmake won't pass the conda $PREFIX/include to
    # the conda compiler and things get crazy
    unset(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
    unset(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    # This has to be done after sparta-config.cmake and before include directories because
    # those variables are maintained as directory properties and will propagate to the subdirectories
    # when we start doing include_directories.  If we clear them after, the toplevel sources will
    # build but the subdirs won't
    #
    # See also https://gitlab.kitware.com/cmake/cmake/issues/17966#note_408480
endif ()

#
# Debug help
#
if (Sparta_VERBOSE)
  get_cmake_property (_variableNames VARIABLES)
  list (SORT _variableNames)
  foreach (_variableName ${_variableNames})
    message (STATUS "${_variableName}=${${_variableName}}")
  endforeach ()
endif ()
