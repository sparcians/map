###############################################################
#                     RequiredLibraries                       #
###############################################################

# Find SQLite3
find_package(SQLite3 3.19 REQUIRED)
message("-- Using SQLite3 ${SQLite3_VERSION}")

# Find HDF5.  Need to enable C language for HDF5 testing
enable_language(C)
find_package(HDF5 1.10 REQUIRED)

set(SimDB_LIBS simdb hdf5 sqlite3 z pthread)
