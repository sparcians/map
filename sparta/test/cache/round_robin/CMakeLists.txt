project(A.out)

add_executable(round_robin main.cpp)

include(../../TestingMacros.cmake)

sparta_test(round_robin round_robin_RUN)
