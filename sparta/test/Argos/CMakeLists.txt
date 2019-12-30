project(Argos_test)

add_executable(Argos_test ArgosCollection_test.cpp)


include(../TestingMacros.cmake)


sparta_test(Argos_test Argos_test_RUN)


add_subdirectory(DatabaseDump)
