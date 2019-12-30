project(Vpt_test)

add_executable(VPT_test VPT_test.cpp)

include(../TestingMacros.cmake)
# We need to copy the tile to the build directory.
sparta_copy(VPT_test input.yaml)

sparta_test(VPT_test VPT_test_RUN)
