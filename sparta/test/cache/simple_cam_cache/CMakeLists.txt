project(Simple_cam_cache)

add_executable(simple_cam_cache main.cpp)

include(../../TestingMacros.cmake)

sparta_test(simple_cam_cache a.out_RUN)
