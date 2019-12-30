project(Memorymap_test)

add_executable(MemoryMap_test MemoryMap_test.cpp)

include(../TestingMacros.cmake)

sparta_test(MemoryMap_test MemoryMap_test_RUN)
