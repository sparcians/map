project(Argos_dumper)

# Enable testing
enable_testing ()

add_executable(Argos_dumper ArgosCollection_test.cpp)
add_test (NAME Argos_dumper_RUN COMMAND Argos_dumper)

target_link_libraries (Argos_dumper ${Sparta_LIBS})

add_subdirectory(DatabaseDump)
