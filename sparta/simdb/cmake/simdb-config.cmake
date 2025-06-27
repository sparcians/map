###############################################################
#                     RequiredLibraries                       #
###############################################################

# Find SQLite3
find_package(SQLite3 3.19 REQUIRED)
message("-- Using SQLite3 ${SQLite3_VERSION}")

# Find HDF5.  Need to enable C language for HDF5 testing
enable_language(C)
find_package(HDF5 1.10 REQUIRED)

set(SimDB_LIBS simdb ${HDF5_LIBRARIES} sqlite3 z pthread)

# If HDF5 is built with MPI support, we also need to add the MPI include dirs and link against the MPI library
if(HDF5_IS_PARALLEL)
    find_package(MPI REQUIRED)
    list(APPEND SimDB_LIBS MPI::MPI_CXX)
endif()
