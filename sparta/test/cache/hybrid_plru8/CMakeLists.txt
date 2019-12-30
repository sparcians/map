project(Hybrid_plru8)

add_executable(hybrid_plru8 main.cpp)

include(../../TestingMacros.cmake)

sparta_test(hybrid_plru8 hybrid_plru8_RUN)
