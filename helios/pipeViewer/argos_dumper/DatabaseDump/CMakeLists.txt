project(Argosdumper)
add_executable(ArgosDumper Database_dumper.cpp)

target_include_directories(ArgosDumper PRIVATE ${CMAKE_SOURCE_DIR}/helios/pipeViewer)
target_link_libraries (ArgosDumper ${Sparta_LIBS})

add_test (NAME ArgosDumperSmokeTestingDB COMMAND ArgosDumper db_pipeout/pipeout)
