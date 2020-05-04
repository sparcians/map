project(Simple_cache)
include(../../TestingMacros.cmake)

sparta_add_test_executable(simple_cache main.cpp l1_line_state.cpp)

sparta_test(simple_cache simple_cache_RUN)
