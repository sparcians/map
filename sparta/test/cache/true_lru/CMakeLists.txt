project(true_lru)

add_executable(true_lru main.cpp)

include(../../TestingMacros.cmake)

sparta_test(true_lru a.out_RUN)
