project(Argos_dumper)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing ()

add_executable(Argos_dumper ArgosCollection_test.cpp)
add_test (NAME Argos_dumper_RUN COMMAND Argos_dumper)

target_include_directories(Argos_dumper PRIVATE ${CMAKE_SOURCE_DIR}/pipeViewer/pipe_view)
target_link_libraries (Argos_dumper SPARTA::sparta rt)

add_subdirectory(DatabaseDump)
