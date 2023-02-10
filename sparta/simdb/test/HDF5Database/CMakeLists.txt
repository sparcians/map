project(SIMDB_HDF5Database_test)

add_executable(SIMDB_HDF5Database_test HDF5Database_test.cpp)

include(${CMAKE_SOURCE_DIR}/sparta/cmake/SimdbTestingMacros.cmake)

simdb_recursive_copy(SIMDB_HDF5Database_test test_dbs)

# TODO:
#   It seems that there are some known issues with HDF5/valgrind.
#   https://support.hdfgroup.org/HDF5/faq/valgrind.html
#
#   This needs investigation, and valgrind should be re-enabled
#   for this test.
simdb_named_test_no_valgrind(SIMDB_HDF5Database_test SIMDB_HDF5Database_test)
