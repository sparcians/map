project(Argosdumper)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DBDumper Database_dumper.cpp)

target_include_directories(DBDumper PRIVATE ${CMAKE_SOURCE_DIR}/pipeViewer/pipe_view)
target_link_libraries (DBDumper SPARTA::sparta)

add_test (NAME DBDumperSmokeTestingDB COMMAND DBDumper db_pipeout/pipeout)
