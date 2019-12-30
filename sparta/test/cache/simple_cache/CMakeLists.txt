project(Simple_cache)

add_executable(simple_cache main.cpp l1_line_state.cpp)

include(../../TestingMacros.cmake)

sparta_test(simple_cache simple_cache_RUN)
