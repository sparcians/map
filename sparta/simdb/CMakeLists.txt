cmake_minimum_required(VERSION 3.15)
project(simdb CXX)

set(SIMDB_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SIMDB_BASE}/include)
include(${SIMDB_BASE}/cmake/simdb-config.cmake)

list(APPEND SimDB_CPP
            src/HDF5Connection.cpp
            src/ObjectManager.cpp
            src/ObjectRef.cpp
            src/SQLiteConnection.cpp
            src/TableRef.cpp
            src/simdb.cpp)

add_library(simdb ${SimDB_CPP})
add_subdirectory(test EXCLUDE_FROM_ALL)

install(TARGETS simdb
        LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	)
install(DIRECTORY include/ DESTINATION include)
