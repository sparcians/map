project(Hybrid_plru16)

add_executable(hybrid_plru16 main.cpp)

include(../../TestingMacros.cmake)

sparta_test(hybrid_plru16 hybrid_plru16_RUN)
