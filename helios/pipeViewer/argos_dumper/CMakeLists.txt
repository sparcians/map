project(Argos_test)

# Enable testing
enable_testing ()

add_executable(Argos_test ArgosCollection_test.cpp)
add_test (NAME Argos_test_RUN COMMAND Argos_test)

target_link_libraries (Argos_test ${Sparta_LIBS})

add_subdirectory(DatabaseDump)
