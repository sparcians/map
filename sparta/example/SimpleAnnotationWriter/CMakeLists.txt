project(SIMPLE_ANNOTATION_WRITER)

include (../ExamplesMacros.cmake)

add_executable(simple_annotation_writer
  src/main.cpp)

sparta_application(simple_annotation_writer)

