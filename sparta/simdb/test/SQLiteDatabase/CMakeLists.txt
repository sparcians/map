project(SIMDB_SQLiteDatabase_test)

add_executable(SIMDB_SQLiteDatabase_test SQLiteDatabase_test.cpp)

include(../TestingMacros.cmake)

simdb_recursive_copy(SIMDB_SQLiteDatabase_test test_dbs)

simdb_test(SIMDB_SQLiteDatabase_test SIMDB_SQLiteDatabase_test_RUN)
