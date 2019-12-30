project(Tree_plru)

add_executable(tree_plru main.cpp)

include(../../TestingMacros.cmake)

sparta_test(tree_plru tree_plru_RUN)
