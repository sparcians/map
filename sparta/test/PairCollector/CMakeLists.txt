project(Collectable_test)
sparta_add_executable(Collectable_test Collectable_test.cpp)
include(../TestingMacros.cmake)
sparta_test(Collectable_test Collectable_test_RUN)
